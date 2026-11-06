#!/usr/bin/env bash
set -euo pipefail
spike_src=${SPIKE_SRC:-/home/chen/riscv-isa-sim-fase}
spike_build=${SPIKE_BUILD:-/home/chen/FUN/fase-spike-runs/20261106/spike-build}
mkdir -p "$spike_build"
git -C "$spike_src" rev-parse HEAD > "$spike_build/source-head.txt"
git -C "$spike_src" diff --stat > "$spike_build/source-diff.txt"
cd "$spike_build"
"$spike_src/configure" --prefix="$spike_build/install" \
  --without-boost --without-boost-asio --without-boost-regex > configure.log 2>&1
make -j"${JOBS:-8}" libriscv.so libdisasm.a libsoftfloat.a libfesvr.a libfdt.a > build.log 2>&1
