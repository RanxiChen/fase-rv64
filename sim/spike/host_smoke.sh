#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/../.." && pwd)
build_root=${BUILD_ROOT:-/home/chen/FUN/fase-spike-runs/20261106}
sim_dir=${RUN_DIR:-$build_root/rtl}
host_run=${HOST_RUN_DIR:-$build_root/host-$(date +%Y%m%dT%H%M%S)}
mkdir -p "$build_root"
cmake -S "$repo_dir" -B "$build_root/host-build" -G Ninja > "$build_root/host-configure.log" 2>&1
cmake --build "$build_root/host-build" --target fase-rv64 -j4 > "$build_root/host-build.log" 2>&1
"${RISCV_CC:-riscv64-linux-gnu-gcc}" -nostdlib -static -march=rv64imafdc -mabi=lp64d \
  -Wl,--build-id=none -Wl,-e,_start "$repo_dir/sim/spike/hello.S" -o "$build_root/hello.elf"
python3 "$repo_dir/sim/spike/host_smoke.py" \
  --run-dir "$host_run" --sim "$sim_dir/obj_dir/VNulCPUCtrlMP" \
  --host "$build_root/host-build/fase-rv64" --elf "$build_root/hello.elf" \
  --config "$repo_dir/sim/spike/host.ini"
printf 'Logs: %s\n' "$host_run"
