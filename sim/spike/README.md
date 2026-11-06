# 原 FASE 控制器 RTL 与 Spike 联合仿真

本目录用于检查 FASE 的控制行为。链路为：

```text
测试驱动 / 原 FASE Host 的 SerialFPGAAdapter
  -> 原命令字节流
  -> NulCPUCtrlMP 原 RTL（Verilator）
  -> NulCPUBundle 的功能适配层
  -> Spike 单核及仿真内存
```

`NulCtrlMP.scala` 和 `port.scala` 保持原样。仿真入口只去掉 UART PHY，直接连接控制器已有的字节 ready/valid 接口。通过伪串口运行时，原 Host、命令编码、ELF 装载、页表管理和 syscall 处理代码也保持原样。

## 固定版本与位置

- FASE fork：`https://github.com/RanxiChen/fase-rv64`。
- FASE 基线：`pcie-spike-adapter-v1`，`a80595dbf2f9093fded8d745f2ca48c1efdc0d59`。
- Alan 实现目录：`/home/chen/fase-spike-rtl`，分支 `feat/spike-rtl-validation`。它是上述 fork 的独立 worktree。
- Spike fork：`/home/chen/riscv-isa-sim-fase`，`fase-pcie-v1`，`93a10ae685ac85bd3d8a62054f8f180a4f76fc82`；不修改 Spike 源码。
- 默认构建及证据目录：`/home/chen/FUN/fase-spike-runs/20261106`。
- 初次工具：SBT 1.9.7、Java 11、Verilator 5.028。构建脚本记录两个源码 HEAD；判断本地修改还需结合 `git status` 和提交内容。

## 在 Alan 上复现

```bash
cd /home/chen/fase-spike-rtl

# 如需重建 Spike 库；已有库可跳过。
bash sim/spike/build_spike.sh

# 生成原控制器 RTL、构建联合仿真程序并运行基本检查。
bash sim/spike/run.sh

# 三组握手/执行延迟，以及检查器的负向自检。
python3 sim/spike/regression.py \
  --sim /home/chen/FUN/fase-spike-runs/20261106/rtl/obj_dir/VNulCPUCtrlMP \
  --out /home/chen/FUN/fase-spike-runs/20261106/regression

# 构建原 Host 与最小 ELF，经过伪串口运行。
bash sim/spike/host_smoke.sh
```

`run.sh` 支持 `SPIKE_SRC`、`SPIKE_BUILD`、`RUN_DIR`、`SBT_LAUNCH_JAR`。默认 SBT launcher 为证据目录中的 `sbt-launch.jar`，版本 1.9.7；这是独立拷贝，没有修补 Alan 的全局 SBT 启动器。仅改 C++ 驱动时可设置 `SKIP_ELABORATE=1`。不全局关闭 Verilator 警告。

基本检查涵盖：

- 控制器初始化所注入的 PMP、mstatus 指令。
- 32 个整数寄存器的 64 位写入/读回和 x0 行为。
- 一次 `addi x5,x5,1`：初始 10，结果 11，只执行一次，其余寄存器保持。
- MEMWT/MEMRD 经原控制器注入 SD/LD，检查目标内存及临时寄存器恢复。
- 一个 U 态 `write + exit` 程序，经 REDIR/MRET、两次 ECALL、NEXT、Host 返回值和 HALT。
- 指令接受、执行完成、寄存器访问与发送端背压。

这些检查既读取 FASE 返回值，也直接检查 Spike 中的寄存器和内存。`--corrupt-addi` 故意在适配层将 ADDI 的立即数改为 2，正确行为是退出 1 并报告期望 11、实际 12，以验证检查器确实能发现错误。

`host_smoke.sh` 用原 FASE Host 加载 `hello.S` 编译的 ELF。目标通过 `write` 输出 `FASE_SPIKE_HOST_HELLO`，检查 syscall 返回的字节数，然后 `exit(0)`。脚本要求 Host 正常结束、出现实际程序输出、仿真端 a0 为 0，并保存两端日志。每次创建独立的运行目录。

## 已发现的失败：浮点寄存器 REGRD 回复偏移

运行独立复现：

```bash
/home/chen/FUN/fase-spike-runs/20261106/rtl/obj_dir/VNulCPUCtrlMP \
  --probe-fp \
  --trace /home/chen/FUN/fase-spike-runs/20261106/rtl/fp-minimal.vcd \
  --insn-log /home/chen/FUN/fase-spike-runs/20261106/rtl/fp-minimal-insns.log
```

测试通过 FASE 将 `f3` 写为 `0x0123456789abcdef`，再单独注入 `fmv.x.d x5,f3`，确认 Spike 实际保存了该值，最后发出 FASE 浮点 REGRD。原实现应在这里暴露读回不一致，进程退出 1。

源码依据：`NulCtrlMP.scala` 的浮点 `STATE_REGRD` 分支调用 `read_reg_to_retarg(0, 2, 8)`，将 8 字节写入 `retarg[2..9]`；REGRD 回复却发送 `retarg[0..7]`。返回值因此带两个旧字节，并丢掉最高两个字节。复现前序不同，两个旧字节也可能不同。这一失败单独报告，不计为基本检查通过，也没有通过改动原控制器来隐藏它。

## 已发现的失败：批量 MMU 命令重复转换页号

原 Host 端到端运行在首条用户指令处报告 instruction access fault。仿真日志记录 `satp=0x8000000000000080`，页表根指向 `0x80000`；实际页表根应为 `0x80000000`，对应 PPN `0x80000`。

调用链中 `SMPSystemV2::_pop_context_and_execute` 先将物理地址右移 12 位，`htp_push_set_mmu` 将页号原样放入 `frame.x2`，但 `SerialFPGAAdapter::_send_frame` 的 MMU 分支再次右移 12 位后发送。直接调用 `SerialFPGAAdapter::set_mmu` 则发送未再次右移的页号。该问题发生在原 Host 的批量编码路径，基本测试驱动不经过它，所以必须单独跑原 Host。

原版证据保存在 Alan 默认证据目录下 `host-smoke-3/host.log`、`server.log`、`instructions.log`。早期 `host-smoke-1` 的 SFENCE 失败属于适配层未启用 Sv39，不计为 FASE 缺陷；适配层修正后的 `host-smoke-2/3` 才复现上述地址问题。

## 功能模型的边界

- CPU 为 RV64IMAFDC + Zicsr/Zifencei，M/S/U、Sv39、单核。目标 RAM 为 `0x80000000` 起的 16 MiB。
- Spike 通过正常 `processor_t::step(1)` 执行注入指令。适配层在独立 `0x1000` 页提供注入指令，执行普通顺序指令后恢复原取指 PC；MRET 的跳转和特权级变化由 Spike 执行并保留。
- 该注入取指槽只支持原控制器使用的顺序指令及 MRET；PC 相对/分支注入会明确报不支持。它不证明任意指令注入语义。
- 从完成启动的 M 态接入，`inited=1`。未模拟实际处理器的启动固件；首次用户执行必须经过控制器注入 MRET。
- 握手及延迟是仿真环境施加的，不是实际处理器流水线或时钟性能模型。未覆盖乱序提交、cache/coherence、CDC、UART PHY、PCIe、实板和多核。
- 运行中的外部中断接管未实现，遇到相关请求明确报错；HALT 在已发生 trap 的 M 态执行。未将该范围算成完整暂停/中断验证。
- 注入指令异常会记录 raw/mcause/mtval 并使检查失败；尚不提供注入异常后的恢复策略。
- 测试驱动处理 ECALL 的基本检查与原 Host 端到端测试分开；前者通过不等于原 Host 全部功能通过。
