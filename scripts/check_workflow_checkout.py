#!/usr/bin/env python3
"""Static regression check: in every GitHub Actions job, any invocation of a
repository-owned script (under scripts/, packaging/ or tests/) must occur only
after an actions/checkout step. Otherwise the file will not exist in the runner
workspace.

Uses only the Python standard library so the check itself has no undeclared
dependency on a clean CI runner. The workflow files are simple, controlled YAML;
this parses just the job/step structure needed to enforce the ordering
invariant.

Usage: check_workflow_checkout.py WORKFLOW [WORKFLOW ...]
"""

import sys

REPO_SCRIPT_PREFIXES = ("scripts/", "packaging/", "tests/")
CHECKOUT_USES = "actions/checkout"


def load_jobs(path):
    """Return {job_name: [step, ...]} where each step is a dict with keys
    'uses' and/or 'run'. Only the ordering-relevant fields are captured."""
    jobs = {}
    in_jobs = False
    current_job = None
    in_steps = False
    step = None

    def flush():
        nonlocal step
        if step is not None:
            jobs.setdefault(current_job, []).append(step)
            step = None

    with open(path, encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line.strip():
                continue
            indent = len(line) - len(line.lstrip())
            content = line.strip()
            if indent == 0:
                in_jobs = content == "jobs:"
                if not in_jobs:
                    flush()
                    current_job = None
                    in_steps = False
                continue
            if not in_jobs:
                continue
            if indent == 2 and content.endswith(":") and not content.startswith("-"):
                flush()
                current_job = content[:-1]
                jobs.setdefault(current_job, [])
                in_steps = False
                continue
            if current_job is None:
                continue
            if indent == 4 and content == "steps:":
                flush()
                in_steps = True
                continue
            if not in_steps:
                continue
            if indent == 6 and content.startswith("- "):
                flush()
                step = {}
                rest = content[2:]
                if rest.startswith("uses:"):
                    step["uses"] = rest[len("uses:"):].strip()
                elif rest.startswith("run:"):
                    value = rest[len("run:"):].strip()
                    step["run"] = "" if value == "|" else value
                    step["run_block"] = value == "|"
                continue
            if step is None:
                continue
            if content.startswith("run:"):
                value = content[len("run:"):].strip()
                step["run"] = (step.get("run", "") + "\n" + value).strip()
                step["run_block"] = value == "|"
                continue
            if step.get("run_block") and indent > 6:
                step["run"] = (step.get("run", "") + "\n" + content).strip()
                continue
    flush()
    return jobs


def check(path):
    """Return the list of job names that invoke repository scripts before (or
    without) an actions/checkout step."""
    jobs = load_jobs(path)
    violations = []
    for job_name, steps in jobs.items():
        seen_checkout = False
        for step in steps:
            if str(step.get("uses", "")).startswith(CHECKOUT_USES):
                seen_checkout = True
            run = step.get("run", "")
            if run and any(prefix in run for prefix in REPO_SCRIPT_PREFIXES) and not seen_checkout:
                violations.append(job_name)
                break
    return violations


def main(argv):
    failed = False
    for path in argv[1:]:
        violations = check(path)
        if violations:
            failed = True
            print(
                f"{path}: jobs invoke repository scripts before or without "
                f"actions/checkout: {', '.join(violations)}",
                file=sys.stderr,
            )
    if failed:
        sys.exit(1)
    print(f"workflow checkout invariant OK ({len(argv[1:])} workflow(s))")


if __name__ == "__main__":
    main(sys.argv)