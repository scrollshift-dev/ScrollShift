#include "smoothwheel/relay.hpp"
#include "smoothwheel/transform.hpp"
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace smoothwheel { namespace {
constexpr std::size_t kBitsPerWord = sizeof(unsigned long) * 8;
std::atomic_bool stop_requested{false};
void signal_handler(int) { stop_requested.store(true); }

template <std::size_t N> bool bit_set(const std::array<unsigned long,N>& b,unsigned bit) {
  const auto w=bit/kBitsPerWord; return w<N && (b[w]&(1UL<<(bit%kBitsPerWord)))!=0;
}
void must_ioctl(int fd,unsigned long request,int value,const char* what) {
  if (::ioctl(fd,request,value)<0) throw std::runtime_error(std::string(what)+": "+std::strerror(errno));
}
class Fd { public: explicit Fd(int fd=-1):fd_(fd){} ~Fd(){if(fd_>=0)::close(fd_);} int get()const{return fd_;} private:int fd_; };
class Grab { public: explicit Grab(int fd):fd_(fd){must_ioctl(fd_,EVIOCGRAB,1,"EVIOCGRAB acquire");} ~Grab(){::ioctl(fd_,EVIOCGRAB,0);} private:int fd_; };

void clone_capabilities(int source,int target) {
  std::array<unsigned long,(EV_MAX/kBitsPerWord)+2> ev{};
  if(::ioctl(source,EVIOCGBIT(0,sizeof(ev)),ev.data())<0) throw std::runtime_error("cannot read event capabilities");
  must_ioctl(target,UI_SET_EVBIT,EV_SYN,"UI_SET_EVBIT EV_SYN");
  if(bit_set(ev,EV_REL)) {
    must_ioctl(target,UI_SET_EVBIT,EV_REL,"UI_SET_EVBIT EV_REL");
    std::array<unsigned long,(REL_MAX/kBitsPerWord)+2> bits{};
    if(::ioctl(source,EVIOCGBIT(EV_REL,sizeof(bits)),bits.data())<0) throw std::runtime_error("cannot read relative capabilities");
    for(unsigned c=0;c<=REL_MAX;++c) if(bit_set(bits,c)) must_ioctl(target,UI_SET_RELBIT,static_cast<int>(c),"UI_SET_RELBIT");
  }
  if(bit_set(ev,EV_KEY)) {
    must_ioctl(target,UI_SET_EVBIT,EV_KEY,"UI_SET_EVBIT EV_KEY");
    std::array<unsigned long,(KEY_MAX/kBitsPerWord)+2> bits{};
    if(::ioctl(source,EVIOCGBIT(EV_KEY,sizeof(bits)),bits.data())<0) throw std::runtime_error("cannot read key capabilities");
    for(unsigned c=0;c<=KEY_MAX;++c) if(bit_set(bits,c)) must_ioctl(target,UI_SET_KEYBIT,static_cast<int>(c),"UI_SET_KEYBIT");
  }
}
void write_event(int fd,const input_event& event) {
  const char* p=reinterpret_cast<const char*>(&event); std::size_t left=sizeof(event);
  while(left){auto n=::write(fd,p,left); if(n<0&&errno==EINTR)continue; if(n<=0)throw std::runtime_error(std::string("uinput write: ")+std::strerror(errno)); p+=n; left-=static_cast<std::size_t>(n);}
}
} // namespace

namespace {
int run_relay_impl(const std::filesystem::path& device,int seconds,int delay_seconds,std::ostream& out,
                   const std::filesystem::path& uinput_path,const AccelerationProfile* profile) {
  if(seconds<=0||seconds>60||delay_seconds<0||delay_seconds>10){out<<"smoothwheel: relay duration/delay out of range\n";return 2;}
  try {
    Fd source(::open(device.c_str(),O_RDONLY|O_CLOEXEC)); if(source.get()<0) throw std::runtime_error("cannot open source: "+std::string(std::strerror(errno)));
    Fd target(::open(uinput_path.c_str(),O_WRONLY|O_NONBLOCK|O_CLOEXEC)); if(target.get()<0) throw std::runtime_error("cannot open uinput: "+std::string(std::strerror(errno)));
    clone_capabilities(source.get(),target.get());
    uinput_setup setup{}; std::strncpy(setup.name,profile?"SmoothWheel Accelerated":"SmoothWheel Relay",UINPUT_MAX_NAME_SIZE-1); setup.id.bustype=BUS_VIRTUAL; setup.id.vendor=0x5357; setup.id.product=profile?0x0003:0x0002; setup.id.version=1;
    if(::ioctl(target.get(),UI_DEV_SETUP,&setup)<0) throw std::runtime_error("UI_DEV_SETUP: "+std::string(std::strerror(errno)));
    must_ioctl(target.get(),UI_DEV_CREATE,0,"UI_DEV_CREATE");
    struct Destroy { int fd; ~Destroy(){::ioctl(fd,UI_DEV_DESTROY);} } destroy{target.get()};
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    for(int i=delay_seconds;i>0;--i){out<<(profile?"Accelerated relay":"Exclusive relay")<<" begins in "<<i<<"...\n";out.flush();std::this_thread::sleep_for(std::chrono::seconds(1));}
    stop_requested.store(false); auto old_int=std::signal(SIGINT,signal_handler); auto old_term=std::signal(SIGTERM,signal_handler);
    {
      Grab grab(source.get());
      if(profile) out<<"Accelerated relay active ("<<profile->name<<") for up to "<<seconds<<" seconds. Ctrl-C stops early.\n";
      else out<<"Relay active for up to "<<seconds<<" seconds. Ctrl-C stops early.\n";
      out.flush();
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
      pollfd pfd{source.get(),POLLIN,0}; input_event event{};
      std::vector<input_event> packet;
      WheelPacketTransformer transformer(profile?profile->velocity:VelocityConfig{});
      while(!stop_requested.load()&&std::chrono::steady_clock::now()<deadline){
        int r=::poll(&pfd,1,100);if(r<0&&errno==EINTR)continue;if(r<0)throw std::runtime_error("poll failed");if(r==0)continue;
        if(pfd.revents&(POLLERR|POLLHUP|POLLNVAL))throw std::runtime_error("source device disappeared");
        if(pfd.revents&POLLIN){
          auto n=::read(source.get(),&event,sizeof(event));
          if(n==static_cast<ssize_t>(sizeof(event))){
            if(!profile){ write_event(target.get(),event); continue; }
            packet.push_back(event);
            if(event.type==EV_SYN && event.code==SYN_REPORT){
              const auto transformed=transformer.transform(packet);
              for(const auto& e:transformed) write_event(target.get(),e);
              packet.clear();
            }
          } else if(n<0&&errno==EINTR) continue; else throw std::runtime_error("source read failed");
        }
      }
      if(profile && !packet.empty()) for(const auto& e:packet) write_event(target.get(),e);
    }
    std::signal(SIGINT,old_int); std::signal(SIGTERM,old_term); out<<"Relay stopped; physical device released.\n"; return 0;
  } catch(const std::exception& e){out<<"smoothwheel: relay failed: "<<e.what()<<"\n";return 1;}
}
} // namespace

int run_pointer_relay(const std::filesystem::path& device,int seconds,int delay_seconds,std::ostream& out,const std::filesystem::path& uinput_path) {
  return run_relay_impl(device,seconds,delay_seconds,out,uinput_path,nullptr);
}

int run_accelerated_relay(const std::filesystem::path& device,const std::string& profile_name,int seconds,int delay_seconds,std::ostream& out,const std::filesystem::path& uinput_path) {
  const auto* profile=find_acceleration_profile(profile_name);
  if(!profile){out<<"smoothwheel: unknown acceleration profile: "<<profile_name<<"\n";return 2;}
  return run_relay_impl(device,seconds,delay_seconds,out,uinput_path,profile);
}
} // namespace smoothwheel
