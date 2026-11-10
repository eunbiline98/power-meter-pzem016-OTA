Import("env")

from pathlib import Path
import shutil


def copy_firmware(source, target, env):
    source_path = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
    project_dir = Path(env.subst("$PROJECT_DIR"))
    destination = project_dir / "firmware.bin"

    if not source_path.is_file():
        raise RuntimeError(f"PlatformIO firmware binary not found: {source_path}")

    shutil.copy2(source_path, destination)
    print(f"Copied firmware to {destination}")


firmware_path = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))
if firmware_path.is_file():
    copy_firmware(None, None, env)

env.AddPostAction(firmware_path.as_posix(), copy_firmware)
