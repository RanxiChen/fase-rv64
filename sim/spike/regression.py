#!/usr/bin/env python3
"""Check CPU-port latency/backpressure and prove that the oracle detects damage."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sim", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    results = []
    cases = [
        ("fast", ["--latency", "0", "--stall", "0"], 0, "PASS suite"),
        ("default", ["--latency", "2", "--stall", "3"], 0, "PASS suite"),
        ("stalled", ["--latency", "7", "--stall", "5"], 0, "PASS suite"),
        ("fp-readback", ["--probe-fp"], 0, "PASS floating-point register readback"),
        ("checker-negative", ["--corrupt-addi"], 1, "FAIL injected addi expected 11, actual 0xc"),
    ]
    for name, options, expected_rc, marker in cases:
        result = subprocess.run([
            str(args.sim.resolve()), *options,
            "--insn-log", str((args.out / f"{name}-instructions.log").resolve())],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        (args.out / f"{name}.log").write_text(result.stdout)
        ok = result.returncode == expected_rc and marker in result.stdout
        results.append({"name": name, "pass": ok, "returncode": result.returncode,
                        "arguments": options})
        print(f"{'PASS' if ok else 'FAIL'} {name}")
    (args.out / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 0 if all(item["pass"] for item in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
