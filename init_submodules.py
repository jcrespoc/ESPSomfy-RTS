"""
PlatformIO pre-script: initialize missing git submodules (e.g. lib/jcrespoc-elog).

Only submodules that are NOT initialized yet (fresh clone without
--recurse-submodules) are checked out. Submodules that already exist are
never touched, so local branches / uncommitted work inside them are safe.
To move a submodule to the commit recorded in this repo, run manually:
    git submodule update <path>
"""
Import("env")

import os
import subprocess

project_dir = env.subst("$PROJECT_DIR")


def _git(*args):
    return subprocess.run(
        ["git", *args], cwd=project_dir, capture_output=True, text=True
    )


if not os.path.isfile(os.path.join(project_dir, ".gitmodules")):
    pass  # no submodules declared
elif not os.path.exists(os.path.join(project_dir, ".git")):
    print("init_submodules: not a git checkout, skipping (submodules must be provided manually)")
else:
    status = _git("submodule", "status")
    if status.returncode != 0:
        print("init_submodules: 'git submodule status' failed:\n" + status.stderr)
    else:
        # Lines starting with '-' are submodules that are not initialized
        missing = [
            line[1:].split()[1]
            for line in status.stdout.splitlines()
            if line.startswith("-")
        ]
        if missing:
            print("init_submodules: initializing " + ", ".join(missing))
            result = _git("submodule", "update", "--init", "--recursive", "--", *missing)
            if result.returncode != 0:
                env.Exit(
                    "init_submodules: failed to initialize submodules:\n" + result.stderr
                )

