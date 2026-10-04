"""Exercise the real application + OpenCV with an explicitly simulated Galaxy SDK."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run_case(executable, scenario, expected_code, marker, extra=()):
    with tempfile.TemporaryDirectory(prefix="camera-test-") as directory:
        root = Path(directory)
        env = dict(os.environ, CAMERA_MOCK_SCENARIO=scenario,
                   CAMERA_MOCK_EVENTS=str(root / "events.txt"))
        run = subprocess.run([executable, "--no-display", "--frames", "3", *extra],
                             cwd=root, env=env, capture_output=True, text=True, timeout=10)
        log_path = root / "logs/daheng.log"
        log = log_path.read_text(encoding="utf-8") if log_path.exists() else ""
        diagnostic = run.stdout + run.stderr + log
        assert run.returncode == expected_code, (scenario, run.returncode, diagnostic)
        assert marker in diagnostic, (scenario, marker, diagnostic)
        events_path = root / "events.txt"
        events = events_path.read_text().splitlines() if events_path.exists() else []
        if "stream_on" in events and scenario != "stream_failure":
            assert events[-3:] == ["stream_off", "close_device", "close_lib"], (scenario, events)
            assert events.count("q") == events.count("dq") - (
                events.count("dq") if scenario in ("timeout", "dq_failure", "null_frame") else 0
            ), (scenario, events)
        elif any(event.startswith("open ") for event in events) and scenario != "open_failure":
            assert events[-2:] == ["close_device", "close_lib"], (scenario, events)
        elif "init" in events and scenario != "init_failure":
            assert events[-1] == "close_lib", (scenario, events)
        if expected_code == 0:
            assert events.count("dq") == events.count("q") == 3, (scenario, events)
        if scenario == "skip_first" and expected_code == 0:
            assert "open 2" in events and "open 1" not in events, events
        print(f"PASS {scenario} {extra}")


def main():
    executable = str(Path(sys.argv[1]).resolve())
    cases = [
        ("normal", 0, "total frames=3"), ("gige", 0, "total frames=3"),
        ("mono", 0, "BayerRG8 failed"), ("optional_failure", 0, "ExposureAuto failed"),
        ("skip_first", 0, "SDK index=2"), ("no_device", 1, "No camera found"),
        ("unsupported_device", 1, "No supported USB3/GigE"),
        ("init_failure", 1, "GXInitLib failed"), ("enumerate_failure", 1, "GXUpdateAllDeviceList failed"),
        ("info_failure", 1, "GXGetDeviceInfo failed"), ("open_failure", 1, "GXOpenDeviceByIndex failed"),
        ("config_failure", 1, "AcquisitionMode failed"), ("stream_failure", 1, "GXStreamOn failed"),
        ("timeout", 1, "Three consecutive frame timeouts"), ("dq_failure", 1, "GXDQBuf failed"),
        ("bad_frames", 1, "Ten consecutive invalid frames"),
        ("short_frame", 1, "Invalid frame size"), ("invalid_dimensions", 1, "Invalid frame size"),
        ("null_image", 1, "Invalid frame size"), ("null_frame", 1, "SDK returned a null frame"),
        ("unsupported_format", 1, "Unsupported frame pixel format"),
        ("conversion_failure", 1, "Image conversion failed"), ("q_failure", 1, "GXQBuf failed"),
        ("stop_failure", 1, "GXStreamOff failed"), ("close_device_failure", 1, "GXCloseDevice failed"),
        ("close_lib_failure", 1, "GXCloseLib failed"),
    ]
    for case in cases:
        run_case(executable, *case)
    run_case(executable, "skip_first", 0, "SDK index=2", ("--camera-index", "2"))
    run_case(executable, "normal", 1, "Camera index exceeds", ("--camera-index", "2"))
    # Help and invalid arguments must finish before any SDK function is called.
    for arguments, code in [(('--help',), 0), (('--frames', '-1'), 2), (('--unknown',), 2)]:
        with tempfile.TemporaryDirectory() as directory:
            events_path = Path(directory) / "events.txt"
            run = subprocess.run([executable, *arguments], cwd=directory,
                                 env=dict(os.environ, CAMERA_MOCK_EVENTS=str(events_path)),
                                 capture_output=True, text=True, timeout=10)
            assert run.returncode == code, (arguments, run.stdout, run.stderr)
            assert not events_path.exists(), (arguments, "SDK called before argument validation")
    print("All 28 acquisition scenarios and 3 command-line scenarios passed")


if __name__ == "__main__":
    main()
