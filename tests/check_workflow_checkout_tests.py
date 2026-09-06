#!/usr/bin/env python3
"""Deterministic regression tests for scripts/check_workflow_checkout.py.

Covers the ordering invariant:
  * repository script invoked after actions/checkout  -> accepted
  * repository script invoked before actions/checkout  -> rejected
  * repository script invoked with no checkout at all  -> rejected
and verifies the real CI/release workflows currently pass. Standard library only.
"""

import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts"))
import check_workflow_checkout as checker  # noqa: E402

GOOD = """name: test
on:
  push:
jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Verify
        run: sh scripts/verify_release.sh 1.0.0 dist
"""

BAD = """name: test
on:
  push:
jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - name: Verify
        run: sh scripts/verify_release.sh 1.0.0 dist
      - uses: actions/checkout@v4
"""

NONE = """name: test
on:
  push:
jobs:
  publish:
    runs-on: ubuntu-latest
    steps:
      - name: Verify
        run: sh tests/installer_checksum_tests.sh
"""


def main():
    tmp = tempfile.mkdtemp(prefix="scrollshift-checkout-check-")
    try:
        good = os.path.join(tmp, "good.yml")
        bad = os.path.join(tmp, "bad.yml")
        none = os.path.join(tmp, "none.yml")
        for path, text in ((good, GOOD), (bad, BAD), (none, NONE)):
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(text)

        # Ordered correctly: accepted.
        assert checker.check(good) == [], "script after checkout should be accepted"
        # Ordered wrongly: rejected with the job name.
        assert checker.check(bad) == ["publish"], "script before checkout should be rejected"
        # No checkout at all: rejected.
        assert checker.check(none) == ["publish"], "script without checkout should be rejected"

        # The real workflows must currently satisfy the invariant.
        repo = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
        for workflow in ("release.yml", "ci.yml"):
            path = os.path.join(repo, ".github", "workflows", workflow)
            violations = checker.check(path)
            assert not violations, f"real workflow {workflow} violates invariant: {violations}"
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)

    print("workflow checkout ordering tests passed")


if __name__ == "__main__":
    main()