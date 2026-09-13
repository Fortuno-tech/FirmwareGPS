"""PlatformIO target for provisioning the public HiveMQ CA into a SIM7600."""

from pathlib import Path
import subprocess
import sys

Import("env")


def upload_certificate(source, target, platformio_env):
    port = platformio_env.subst("$UPLOAD_PORT")
    if not port or port == "$UPLOAD_PORT":
        raise RuntimeError(
            "Set the direct SIM7600 serial port, for example: "
            "pio run -e esp32doit-devkit-v1 -t upload_certificate --upload-port COM5"
        )

    project_dir = Path(platformio_env["PROJECT_DIR"])
    subprocess.check_call([
        sys.executable,
        str(project_dir / "tools" / "upload_certificate.py"),
        "--port",
        port,
    ])


env.AddCustomTarget(
    name="upload_certificate",
    dependencies=None,
    actions=[upload_certificate],
    title="Upload SIM7600 HiveMQ CA",
    description="Upload and verify the public HiveMQ CA with AT+CCERTDOWN",
)
