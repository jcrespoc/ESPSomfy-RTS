#!/usr/bin/env python3
import os
import subprocess
import re

try:
    from SCons.Script import Import
    Import("env")
except Exception:
    env = None

def get_project_dir():
    if "env" in globals() and env is not None:
        try:
            return env.subst("$PROJECT_DIR")
        except Exception:
            pass
    current = os.path.dirname(os.path.abspath(__file__))
    if os.path.basename(current) == "scripts":
        return os.path.dirname(current)
    return current

def get_git_version(project_dir=None):
    if not project_dir:
        project_dir = get_project_dir()

    # 1. Try Git tag (cleanest tag)
    try:
        tag = subprocess.check_output(
            ["git", "-c", "safe.directory=*", "describe", "--tags", "--abbrev=0"],
            cwd=project_dir,
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()
        if tag:
            return tag if tag.startswith("v") else f"v{tag}"
    except Exception:
        pass

    # 2. Try git describe --tags --always (e.g. v2.6.2-4-g748c2a2)
    try:
        desc = subprocess.check_output(
            ["git", "-c", "safe.directory=*", "describe", "--tags", "--always"],
            cwd=project_dir,
            text=True,
            stderr=subprocess.DEVNULL
        ).strip()
        if desc:
            match = re.search(r"^(v?[0-9]+\.[0-9]+(?:\.[0-9]+)?)", desc)
            if match:
                v = match.group(1)
                return v if v.startswith("v") else f"v{v}"
    except Exception:
        pass

    # 3. Fallback to src/ConfigSettings.h
    cfg_file = os.path.join(project_dir, "src", "ConfigSettings.h")
    if os.path.exists(cfg_file):
        try:
            with open(cfg_file, "r", encoding="utf-8") as f:
                match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
                if match:
                    v = match.group(1).strip()
                    return v if v.startswith("v") else f"v{v}"
        except Exception:
            pass

    # 4. Fallback to data-dev/appversion
    appver_file = os.path.join(project_dir, "data-dev", "appversion")
    if os.path.exists(appver_file):
        try:
            with open(appver_file, "r", encoding="utf-8") as f:
                lines = [line.strip() for line in f.read().splitlines() if line.strip()]
                if lines:
                    v = lines[0]
                    return v if v.startswith("v") else f"v{v}"
        except Exception:
            pass

    return "v2.6.2"

def sync_version_files(project_dir, fw_version):
    # App version without 'v' prefix for data-dev/appversion to maintain upstream format
    app_version_clean = fw_version.lstrip("v")

    # 1. Update data-dev/appversion if needed
    appver_path = os.path.join(project_dir, "data-dev", "appversion")
    if os.path.exists(os.path.dirname(appver_path)):
        current_content = ""
        if os.path.exists(appver_path):
            try:
                with open(appver_path, "r", encoding="utf-8") as f:
                    lines = [line.strip() for line in f.read().splitlines() if line.strip()]
                    current_content = lines[0] if lines else ""
            except Exception:
                pass
        if current_content != app_version_clean:
            try:
                with open(appver_path, "w", encoding="utf-8") as f:
                    f.write(f"{app_version_clean}\n")
                print(f"[git-version] Synchronized data-dev/appversion -> {app_version_clean}")
            except Exception as e:
                print(f"[git-version] Warning: failed to write {appver_path}: {e}")

    # 2. Update data-dev/index.js if needed
    js_path = os.path.join(project_dir, "data-dev", "index.js")
    if os.path.exists(js_path):
        try:
            with open(js_path, "r", encoding="utf-8") as f:
                js_content = f.read()
            new_js, count = re.subn(
                r"(appVersion\s*=\s*['\"])[^'\"]+(['\"];)",
                rf"\g<1>{fw_version}\g<2>",
                js_content,
                count=1
            )
            if count > 0 and new_js != js_content:
                with open(js_path, "w", encoding="utf-8") as f:
                    f.write(new_js)
                print(f"[git-version] Synchronized data-dev/index.js -> {fw_version}")
        except Exception as e:
            print(f"[git-version] Warning: failed to update {js_path}: {e}")

def main():
    proj_dir = get_project_dir()
    fw_ver = get_git_version(proj_dir)
    print(f"[git-version] Version resolved from Git: {fw_ver}")

    sync_version_files(proj_dir, fw_ver)

    if "env" in globals() and env is not None:
        if hasattr(env, "StringifyMacro"):
            macro_val = env.StringifyMacro(fw_ver)
        else:
            macro_val = f'\\"{fw_ver}\\"'
        env.Append(CPPDEFINES=[("FW_VERSION", macro_val)])
        print(f"[git-version] Injected CPPDEFINES: FW_VERSION={macro_val}")

if __name__ == "__main__" or ("env" in globals() and env is not None):
    main()

