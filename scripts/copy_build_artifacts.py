#!/usr/bin/env python3
import os
import shutil
import re

try:
    from SCons.Script import Import
    Import("env")
except Exception:
    env = None

ENV_TO_CHIP = {
    "esp32dev": "esp32",
    "esp32wrover": "esp32wrover",
    "esp32c3": "esp32c3",
    "esp32s2": "esp32s2",
    "esp32s3_4mb": "esp32s3_4mb",
    "esp32s3_8mb": "esp32s3_8mb",
}

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

def get_fw_version(project_dir):
    try:
        import sys
        scripts_dir = os.path.dirname(os.path.abspath(__file__))
        if scripts_dir not in sys.path:
            sys.path.insert(0, scripts_dir)
        from git_version import get_git_version
        return get_git_version(project_dir)
    except Exception:
        pass

    cfg_file = os.path.join(project_dir, "src", "ConfigSettings.h")
    if os.path.exists(cfg_file):
        try:
            with open(cfg_file, "r", encoding="utf-8") as f:
                match = re.search(r'#define\s+FW_VERSION\s+"([^"]+)"', f.read())
                if match:
                    return match.group(1).strip()
        except Exception:
            pass

    appver_file = os.path.join(project_dir, "data-dev", "appversion")
    if os.path.exists(appver_file):
        try:
            with open(appver_file, "r", encoding="utf-8") as f:
                v = f.read().strip()
                if v:
                    return v if v.startswith("v") else f"v{v}"
        except Exception:
            pass

    return "v2.6.2"

def get_destination_dir(project_dir, version):
    dist_dir = os.path.join(project_dir, "dist", version)
    os.makedirs(dist_dir, exist_ok=True)
    return dist_dir

def copy_file(src, dst):
    try:
        os.makedirs(os.path.dirname(os.path.abspath(dst)), exist_ok=True)
        # Avoid unnecessary re-copy if already identical
        if os.path.exists(dst) and os.path.getmtime(src) == os.path.getmtime(dst) and os.path.getsize(src) == os.path.getsize(dst):
            return
        shutil.copy2(src, dst)
        size_kb = os.path.getsize(dst) / 1024.0
        rel_dst = os.path.relpath(dst, get_project_dir())
        print(f"[copy-bin] -> {rel_dst} ({size_kb:.1f} KB)")
    except Exception as e:
        print(f"[copy-bin] Error copying {src} to {dst}: {e}")

def resolve_bin_file(arg1, arg2):
    for arg in (arg1, arg2):
        if not arg:
            continue
        raw = arg[0] if isinstance(arg, (list, tuple)) else arg
        path = raw.get_abspath() if hasattr(raw, "get_abspath") else str(raw)
        if os.path.isfile(path) and path.endswith(".bin"):
            return path
    return None

def handle_firmware_copy(target, source, env):
    src_bin = resolve_bin_file(target, source)
    if not src_bin:
        return

    project_dir = get_project_dir()
    version = get_fw_version(project_dir)
    env_name = env.subst("$PIOENV") if env and hasattr(env, "subst") else "esp32dev"
    chip = ENV_TO_CHIP.get(env_name, env_name)

    dest_dir = get_destination_dir(project_dir, version)
    filename = f"SomfyController.ino.{chip}.bin"
    copy_file(src_bin, os.path.join(dest_dir, filename))

def handle_littlefs_copy(target, source, env):
    src_bin = resolve_bin_file(target, source)
    if not src_bin:
        return

    project_dir = get_project_dir()
    version = get_fw_version(project_dir)

    dest_dir = get_destination_dir(project_dir, version)
    filename = "SomfyController.littlefs.bin"
    copy_file(src_bin, os.path.join(dest_dir, filename))

def handle_buildfs_alias(source, target, env):
    build_dir = env.subst("$BUILD_DIR") if env and hasattr(env, "subst") else ""
    if not build_dir:
        return
    for name in ["littlefs.bin", "spiffs.bin"]:
        candidate = os.path.join(build_dir, name)
        if os.path.isfile(candidate):
            handle_littlefs_copy(source, [candidate], env)

# Hook into SCons if running inside PlatformIO
if env is not None:
    # Post-action on firmware binary build
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", handle_firmware_copy)
    # Post-action on LittleFS / filesystem binary build
    env.AddPostAction("$BUILD_DIR/littlefs.bin", handle_littlefs_copy)
    env.AddPostAction("$BUILD_DIR/spiffs.bin", handle_littlefs_copy)
    env.AddPostAction("buildfs", handle_buildfs_alias)

if __name__ == "__main__":
    # Standalone execution scanning .pio/build
    p_dir = get_project_dir()
    pio_build = os.path.join(p_dir, ".pio", "build")
    if os.path.isdir(pio_build):
        for e in sorted(os.listdir(pio_build)):
            env_dir = os.path.join(pio_build, e)
            if not os.path.isdir(env_dir):
                continue
            fw_bin = os.path.join(env_dir, "firmware.bin")
            if os.path.isfile(fw_bin):
                class DummyEnv:
                    def __init__(self, name): self.name = name
                    def subst(self, s): return self.name if "$PIOENV" in s else s
                handle_firmware_copy(None, [fw_bin], DummyEnv(e))
            for fs_name in ["littlefs.bin", "spiffs.bin"]:
                fs_bin = os.path.join(env_dir, fs_name)
                if os.path.isfile(fs_bin):
                    handle_littlefs_copy(None, [fs_bin], DummyEnv(e))

