#!/usr/bin/env python3
"""Run the unmodified FASE Host against controller RTL and Spike on a PTY."""
import argparse
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--sim", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    args = parser.parse_args()
    run = args.run_dir.resolve()
    run.mkdir(parents=True, exist_ok=True)
    path_file = run / "serial-path.txt"
    if path_file.exists():
        raise RuntimeError("use a fresh run directory (PTY path file already exists)")
    with (run / "server.log").open("w") as server_log:
        server = subprocess.Popen([
            str(args.sim.resolve()), "--pty-file", str(path_file),
            "--insn-log", str(run / "instructions.log")],
            stdout=server_log, stderr=subprocess.STDOUT, cwd=run)
        try:
            deadline = time.monotonic() + 15
            while not path_file.exists():
                if server.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("simulation server did not start; see server.log")
                time.sleep(0.05)
            serial_path = path_file.read_text().strip()
            with (run / "host.log").open("w") as host_log:
                result = subprocess.run([
                    str(args.host.resolve()), "serial", "-c", str(args.config.resolve()),
                    "-s", serial_path, "-w", str(args.elf.resolve())],
                    stdout=host_log, stderr=subprocess.STDOUT, cwd=run, timeout=120)
            if result.returncode:
                raise RuntimeError(f"FASE Host exited {result.returncode}; see host.log")
            output = (run / "host.log").read_text()
            if "FASE_SPIKE_HOST_HELLO\n" not in output or "All cores halted, exit" not in output:
                raise RuntimeError("missing program output or normal completion; see host.log")
        finally:
            if server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
        if server.returncode != 0:
            raise RuntimeError(f"simulation server exited {server.returncode}; see server.log")
    if "a0=0x0" not in (run / "server.log").read_text():
        raise RuntimeError("target did not exit with a0=0")
    print("PASS original FASE Host -> SerialFPGAAdapter -> controller RTL -> Spike")
    print("PASS ELF loading, Sv39, write return value, exit, all-halted completion")


if __name__ == "__main__":
    main()
