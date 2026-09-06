#!/usr/bin/env python3
"""Static regression check: any GitHub Actions job that invokes a repository-
owned script (under scripts/, packaging/ or tests/) must check out the
repository first, otherwise the file will not exist in the runner workspace.

Usage: check_workflow_checkout.py WORKFLOW [WORKFLOW ...]
"""

import sys
import yaml

REPO_SCRIPT_PREFIXES = ("scripts/", "packaging/", "tests/")
CHECKOUT_USES = "actions/checkout"


def check(path):
    with open(path, encoding="utf-8") as fh:
        workflow = yaml.safe_load(fh)
    violations = []
    for job_name, job in workflow.get("jobs", {}).items():
        steps = job.get("steps", []) or []
        has_checkout = any(
            isinstance(s, dict) and str(s.get("uses", "")).startswith(CHECKOUT_USES)
            for s in steps
        )
        invokes_repo_script = any(
            isinstance(s, dict)
            and "run" in s
            and any(prefix in s["run"] for prefix in REPO_SCRIPT_PREFIXES)
            for s in steps
        )
        if invokes_repo_script and not has_checkout:
            violations.append(job_name)
    return violations


def main(argv):
    failed = False
    for path in argv[1:]:
        violations = check(path)
        if violations:
            failed = True
            print(
                f"{path}: jobs invoke repository scripts without actions/checkout: "
                + ", ".join(violations),
                file=sys.stderr,
            )
    if failed:
        sys.exit(1)
    print(f"workflow checkout invariant OK ({len(argv[1:])} workflow(s))")


if __name__ == "__main__":
    main(sys.argv)