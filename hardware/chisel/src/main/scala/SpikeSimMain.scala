package nulctrl

import chisel3._

// Keep the original controller in the simulation; only omit the UART PHY.
object NulCPUCtrlSpikeSimMain extends App {
  emitVerilog(new NulCPUCtrlMP(1), args)
}
