#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/../.." && pwd)
spike_src=${SPIKE_SRC:-/home/chen/riscv-isa-sim-fase}
spike_build=${SPIKE_BUILD:-/home/chen/FUN/fase-spike-runs/20261106/spike-build}
run_dir=${RUN_DIR:-/home/chen/FUN/fase-spike-runs/20261106/rtl}
sbt_launch=${SBT_LAUNCH_JAR:-/home/chen/FUN/fase-spike-runs/20261106/sbt-launch.jar}
mkdir -p "$run_dir"
git -C "$repo_dir" rev-parse HEAD > "$run_dir/fase-head.txt"
git -C "$spike_src" rev-parse HEAD > "$run_dir/spike-head.txt"
if [[ ${SKIP_ELABORATE:-0} != 1 ]]; then
  (cd "$repo_dir/hardware/chisel" && java -XX:ActiveProcessorCount=4 -Xmx2G -jar "$sbt_launch" "runMain nulctrl.NulCPUCtrlSpikeSimMain --target-dir $run_dir/generated") > "$run_dir/elaborate.log" 2>&1
fi
verilator --cc --exe --build -j 4 --trace --top-module NulCPUCtrlMP \
  --Mdir "$run_dir/obj_dir" \
  -CFLAGS "-std=c++20 -iquote $spike_build -iquote $spike_src -iquote $spike_src/riscv -iquote $spike_src/fesvr -iquote $spike_src/softfloat -iquote $spike_src/fdt" \
  -LDFLAGS "-L$spike_build -Wl,-rpath,$spike_build -lriscv -ldisasm -lsoftfloat -lfesvr -lfdt -lpthread -ldl" \
  "$run_dir/generated/NulCPUCtrlMP.v" "$repo_dir/sim/spike/main.cpp" \
  > "$run_dir/verilator-build.log" 2>&1
"$run_dir/obj_dir/VNulCPUCtrlMP" --trace "$run_dir/wave.vcd" --insn-log "$run_dir/instructions.log" "$@" 2>&1 | tee "$run_dir/test.log"
