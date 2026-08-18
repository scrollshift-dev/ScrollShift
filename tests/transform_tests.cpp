#include "smoothwheel/transform.hpp"
#include <cassert>
#include <iostream>

namespace {
input_event ev(long sec, long usec, unsigned short type, unsigned short code, int value) {
  input_event e{}; e.time.tv_sec=sec; e.time.tv_usec=usec; e.type=type; e.code=code; e.value=value; return e;
}
int value_of(const std::vector<input_event>& p, unsigned short code) {
  for (const auto& e:p) if(e.type==EV_REL&&e.code==code) return e.value; return 0;
}
}
int main() {
  using namespace smoothwheel;
  auto* profile=find_acceleration_profile("balanced"); assert(profile);
  assert(profile->velocity.min_multiplier == 1.0);
  assert(profile->velocity.max_multiplier == 4.0);
  assert(profile->velocity.curve_power == 1.0);

  WheelPacketTransformer t(profile->velocity);
  auto p=t.transform({ev(1,0,EV_REL,REL_WHEEL,1),ev(1,0,EV_REL,REL_WHEEL_HI_RES,120),ev(1,0,EV_SYN,SYN_REPORT,0)});
  assert(value_of(p,REL_WHEEL)==1);
  assert(value_of(p,REL_WHEEL_HI_RES)==120);

  // Slow/isolated detents remain native-speed in the restored balanced profile.
  p=t.transform({ev(1,600000,EV_REL,REL_WHEEL,1),ev(1,600000,EV_REL,REL_WHEEL_HI_RES,120),ev(1,600000,EV_SYN,SYN_REPORT,0)});
  assert(value_of(p,REL_WHEEL)==1);
  assert(value_of(p,REL_WHEEL_HI_RES)==120);

  t.reset();
  p=t.transform({ev(3,0,EV_REL,REL_WHEEL,1),ev(3,0,EV_REL,REL_WHEEL_HI_RES,120),ev(3,0,EV_SYN,SYN_REPORT,0)});
  for (int i=1;i<=18;++i)
    p=t.transform({ev(3,i*35000,EV_REL,REL_WHEEL,1),ev(3,i*35000,EV_REL,REL_WHEEL_HI_RES,120),ev(3,i*35000,EV_SYN,SYN_REPORT,0)});
  assert(value_of(p,REL_WHEEL)>=3);
  assert(value_of(p,REL_WHEEL_HI_RES)>=360);

  // Reversal must immediately drop back to precision speed and clear opposing legacy remainder.
  p=t.transform({ev(4,0,EV_REL,REL_WHEEL,-1),ev(4,0,EV_REL,REL_WHEEL_HI_RES,-120),ev(4,0,EV_SYN,SYN_REPORT,0)});
  assert(value_of(p,REL_WHEEL)==-1);
  assert(value_of(p,REL_WHEEL_HI_RES)==-120);

  t.reset();
  p=t.transform({ev(5,0,EV_REL,REL_X,7),ev(5,0,EV_REL,REL_Y,-3),ev(5,0,EV_SYN,SYN_REPORT,0)});
  assert(value_of(p,REL_X)==7); assert(value_of(p,REL_Y)==-3);
  std::cout<<"transform tests passed\n";
}
