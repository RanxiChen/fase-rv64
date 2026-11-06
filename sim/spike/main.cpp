// Functional CPU-port model for the original FASE controller RTL.
// Spike executes every injected instruction; this is not a pipeline model.
#include "VNulCPUCtrlMP.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "processor.h"
#include "simif.h"
#include "mmu.h"
#include "encoding.h"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

static void check(bool ok, const std::string& message) {
  if (!ok) throw std::runtime_error(message);
}

static std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << "0x" << std::hex << value;
  return out.str();
}

class SpikeCore final : public simif_t {
 public:
  static constexpr uint64_t ram_base = 0x80000000;
  static constexpr uint64_t inject_pc = 0x1000;
  cfg_t cfg;
  std::vector<uint8_t> ram = std::vector<uint8_t>(16 * 1024 * 1024);
  std::array<uint8_t, 4096> injection_page{};
  std::map<size_t, processor_t*> harts;
  std::unique_ptr<processor_t> cpu;
  uint64_t injected = 0;
  uint64_t user_steps = 0;
  std::ofstream insn_log;

  explicit SpikeCore(const std::string& log_path) : insn_log(log_path) {
    cfg.isa = "RV64IMAFDC_Zicsr_Zifencei";
    cfg.priv = "MSU";
    cfg.hartids = {0};
    cfg.pmpregions = 16;
    cfg.mem_layout = {mem_cfg_t(ram_base, ram.size())};
    debug_mmu = nullptr;
    cpu = std::make_unique<processor_t>(cfg.isa, cfg.priv, &cfg, this,
                                        0, false, nullptr, std::cerr);
    // sim_t normally obtains this from its device tree. We instantiate the
    // processor directly, so configure the FASE target's Sv39 MMU explicitly.
    cpu->set_max_vaddr_bits(39);
    harts.emplace(0, cpu.get());
  }

  char* addr_to_mem(reg_t address) override {
    if (address >= ram_base && address - ram_base < ram.size())
      return reinterpret_cast<char*>(ram.data() + address - ram_base);
    if (address >= inject_pc && address - inject_pc < injection_page.size())
      return reinterpret_cast<char*>(injection_page.data() + address - inject_pc);
    return nullptr;
  }
  bool mmio_load(reg_t, size_t, uint8_t*) override { return false; }
  bool mmio_store(reg_t, size_t, const uint8_t*) override { return false; }
  void proc_reset(unsigned) override {}
  const cfg_t& get_cfg() const override { return cfg; }
  const std::map<size_t, processor_t*>& get_harts() const override { return harts; }
  const char* get_symbol(uint64_t) override { return nullptr; }

  uint64_t reg(unsigned index) { return cpu->get_state()->XPR[index]; }
  void write_reg(unsigned index, uint64_t value) {
    cpu->get_state()->XPR.write(index, value);
  }
  unsigned privilege() { return cpu->get_state()->prv; }

  void inject(uint32_t instruction, uint64_t cycle) {
    check(privilege() == PRV_M, "injection accepted outside M mode");
    const auto opcode = instruction & 0x7f;
    check(opcode != 0x17 && opcode != 0x63 && opcode != 0x67 && opcode != 0x6f,
          "PC-relative/branch injection is outside this CPU-port model");
    auto* state = cpu->get_state();
    const auto saved_pc = state->pc;
    for (unsigned i = 0; i < 4; ++i)
      injection_page[i] = static_cast<uint8_t>(instruction >> (8 * i));
    // A reserved fetch slot supplies the external instruction to Spike's
    // normal fetch/decode/execute path. It never overwrites target RAM.
    state->pc = inject_pc;
    cpu->get_mmu()->flush_icache();
    const auto before = cpu->get_csr(CSR_MINSTRET);
    cpu->step(1);
    check(cpu->get_csr(CSR_MINSTRET) == before + 1,
          "injected instruction trapped: raw=" + hex64(instruction) +
          " mcause=" + hex64(cpu->get_csr(CSR_MCAUSE)) +
          " mtval=" + hex64(cpu->get_csr(CSR_MTVAL)));
    if (instruction != 0x30200073) {
      check(state->pc == inject_pc + 4,
            "unsupported injected control-flow instruction " + hex64(instruction));
      state->pc = saved_pc;
    }
    ++injected;
    insn_log << cycle << " inject " << hex64(instruction)
             << " priv=" << privilege() << " pc=" << hex64(state->pc) << '\n';
  }

  void step_user(uint64_t cycle) {
    check(privilege() == PRV_U, "normal execution outside U mode");
    const auto pc = cpu->get_state()->pc;
    cpu->step(1);
    ++user_steps;
    insn_log << cycle << " user pc=" << hex64(pc)
             << " next=" << hex64(cpu->get_state()->pc)
             << " priv=" << privilege() << '\n';
    if (privilege() != PRV_U) {
      insn_log << " trap mcause=" << hex64(cpu->get_csr(CSR_MCAUSE))
               << " mtval=" << hex64(cpu->get_csr(CSR_MTVAL))
               << " satp=" << hex64(cpu->get_csr(CSR_SATP))
               << " pmpcfg0=" << hex64(cpu->get_csr(CSR_PMPCFG0))
               << " pmpaddr0=" << hex64(cpu->get_csr(CSR_PMPADDR0)) << '\n';
      auto table = (cpu->get_csr(CSR_SATP) & ((uint64_t(1) << 44) - 1)) << 12;
      if (table) for (int level = 2; level >= 0; --level) {
        const auto pte_address = table + ((pc >> (12 + 9 * level)) & 511) * 8;
        auto* pointer = addr_to_mem(pte_address);
        if (!pointer) {
          insn_log << " PTE unbacked address=" << hex64(pte_address) << '\n';
          break;
        }
        uint64_t pte = 0;
        std::memcpy(&pte, pointer, 8);
        insn_log << " PTE level=" << level << " address=" << hex64(pte_address)
                 << " value=" << hex64(pte) << '\n';
        if (!(pte & 1) || (pte & 0xe)) break;
        table = (pte >> 10) << 12;
      }
    }
  }
};

class Simulation {
 public:
  SpikeCore core;
  VNulCPUCtrlMP rtl;
  std::unique_ptr<VerilatedVcdC> wave;
  uint64_t cycle = 0;
  unsigned latency;
  unsigned stall;
  bool corrupt_addi;
  std::deque<uint8_t> host_tx;
  std::deque<uint8_t> host_rx;
  struct Pending { uint32_t instruction; unsigned remaining; };
  std::optional<Pending> pending;
  uint64_t accepted = 0;

  Simulation(unsigned lat, unsigned st, bool corrupt,
             const std::string& trace, const std::string& log_path)
      : core(log_path), latency(lat), stall(st), corrupt_addi(corrupt) {
    if (!trace.empty()) {
      Verilated::traceEverOn(true);
      wave = std::make_unique<VerilatedVcdC>();
      rtl.trace(wave.get(), 99);
      wave->open(trace.c_str());
    }
    rtl.reset = 1;
    for (unsigned i = 0; i < 5; ++i) tick();
    rtl.reset = 0;
    for (unsigned i = 0; i < 10000; ++i) {
      tick();
      if ((rtl.io_dbg_sta & 0x80) && rtl.io_rx_ready) return;
    }
    throw std::runtime_error("controller initialization timeout");
  }
  ~Simulation() { if (wave) wave->close(); }

  void tick() {
    bool completed = false;
    if (!rtl.reset && pending) {
      if (pending->remaining == 0) {
        core.inject(pending->instruction, cycle);
        pending.reset();
        completed = true;
      } else {
        --pending->remaining;
      }
    }
    rtl.clock = 0;
    rtl.io_cpu_0_inited = 1;
    rtl.io_cpu_0_priv = core.privilege();
    rtl.io_cpu_0_inst64_busy = pending.has_value();
    rtl.io_cpu_0_inst64_ready = !pending && (cycle % (stall + 1) == 0);
    rtl.io_cpu_0_regacc_busy = cycle % (stall + 1) != 0;
    rtl.io_rx_valid = !host_tx.empty() && cycle % 3 != 1;
    rtl.io_rx_bits = host_tx.empty() ? 0 : host_tx.front();
    rtl.io_tx_ready = cycle % (stall + 2) == 0;
    rtl.eval();
    rtl.io_cpu_0_regacc_rdata = core.reg(rtl.io_cpu_0_regacc_idx);
    rtl.eval();

    const bool receive = rtl.io_rx_valid && rtl.io_rx_ready;
    const bool transmit = rtl.io_tx_valid && rtl.io_tx_ready;
    const auto tx_byte = rtl.io_tx_bits;
    const bool reg_write = rtl.io_cpu_0_regacc_wt && !rtl.io_cpu_0_regacc_busy;
    const auto reg_index = rtl.io_cpu_0_regacc_idx;
    const auto reg_data = rtl.io_cpu_0_regacc_wdata;
    const bool external_interrupt = rtl.io_cpu_0_ext_itr;
    const bool inject = rtl.io_cpu_0_inst64 && rtl.io_cpu_0_inst64_ready;
    auto instruction = rtl.io_cpu_0_inst64_raw;
    const bool run = !rtl.io_cpu_0_stop_fetch && !pending && !inject &&
                     !completed && core.privilege() == PRV_U;
    if (wave) wave->dump(2 * cycle);
    rtl.clock = 1;
    rtl.eval();
    if (!rtl.reset) {
      check(!external_interrupt || core.privilege() != PRV_U,
            "live external-interrupt takeover is outside this CPU-port model");
      if (receive) host_tx.pop_front();
      if (transmit) host_rx.push_back(tx_byte);
      if (reg_write) core.write_reg(reg_index, reg_data);
      if (inject) {
        if (corrupt_addi && instruction == 0x00128293) instruction = 0x00228293;
        pending = Pending{instruction, latency};
        ++accepted;
      }
      if (run) core.step_user(cycle);
      check(!(rtl.io_dbg_sta & 0x40), "controller entered error state");
    }
    if (wave) wave->dump(2 * cycle + 1);
    ++cycle;
  }

  std::vector<uint8_t> command(const std::vector<uint8_t>& request, size_t reply) {
    check(host_tx.empty() && host_rx.empty(), "undrained host transaction");
    host_tx.insert(host_tx.end(), request.begin(), request.end());
    const auto deadline = cycle + 200000;
    while (host_rx.size() < reply && cycle < deadline) tick();
    check(host_rx.size() == reply, "command timeout opcode=" +
          hex64(request.front()) + " dbg=" + hex64(rtl.io_dbg_sta));
    std::vector<uint8_t> result(host_rx.begin(), host_rx.end());
    host_rx.clear();
    check(result[0] == request[0], "reply opcode mismatch");
    return result;
  }
  static void append(std::vector<uint8_t>& bytes, uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) bytes.push_back(value >> (8 * i));
  }
  static uint64_t unpack(const std::vector<uint8_t>& bytes, size_t offset, unsigned count) {
    uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value |= uint64_t(bytes.at(offset + i)) << (8 * i);
    return value;
  }
  void reg_write(unsigned index, uint64_t value) {
    std::vector<uint8_t> bytes{9, 0};
    append(bytes, index, 2);
    append(bytes, value, 8);
    command(bytes, 1);
  }
  uint64_t reg_read(unsigned index) {
    std::vector<uint8_t> bytes{8, 0};
    append(bytes, index, 2);
    return unpack(command(bytes, 9), 1, 8);
  }
  void inst(uint32_t raw) {
    std::vector<uint8_t> bytes{30, 0};
    append(bytes, raw, 4);
    command(bytes, 1);
  }
  void mem_write(uint64_t address, uint64_t value) {
    std::vector<uint8_t> bytes{11, 0};
    append(bytes, address, 6);
    append(bytes, value, 8);
    command(bytes, 1);
  }
  uint64_t mem_read(uint64_t address) {
    std::vector<uint8_t> bytes{10, 0};
    append(bytes, address, 6);
    return unpack(command(bytes, 9), 1, 8);
  }
  void redirect(uint64_t pc) {
    std::vector<uint8_t> bytes{4, 0};
    append(bytes, pc, 6);
    command(bytes, 1);
  }
  std::vector<uint8_t> next() { return command({0}, 15); }
};

static void run_checks(Simulation& sim) {
  check(sim.core.cpu->get_csr(CSR_PMPCFG0) == 0x1f, "PMP configuration mismatch");
  check(sim.core.cpu->get_csr(CSR_MSTATUS) & MSTATUS_FS, "FP not initialized");
  std::cout << "PASS controller initialization through injected CSR instructions\n";
  for (unsigned i = 0; i < 32; ++i) {
    const uint64_t expected = i ? 0xfedcba9876540000ULL + i : 0;
    sim.reg_write(i, 0xfedcba9876540000ULL + i);
    check(sim.core.reg(i) == expected, "Spike register write mismatch x" + std::to_string(i));
    check(sim.reg_read(i) == expected, "FASE register read mismatch x" + std::to_string(i));
  }
  std::cout << "PASS all 32 integer registers, 64-bit values, x0 hardwired\n";
  sim.reg_write(5, 10);
  std::array<uint64_t, 32> before{};
  for (unsigned i = 0; i < 32; ++i) before[i] = sim.core.reg(i);
  const auto injections_before = sim.core.injected;
  sim.inst(0x00128293); // addi x5,x5,1
  check(sim.core.injected == injections_before + 1, "instruction executed more than once");
  check(sim.core.reg(5) == 11, "injected addi expected 11, actual " + hex64(sim.core.reg(5)));
  check(sim.reg_read(5) == 11, "FASE readback after injected addi");
  for (unsigned i = 0; i < 32; ++i)
    check(sim.core.reg(i) == (i == 5 ? 11 : before[i]), "unrelated register clobbered");
  std::cout << "PASS single injected ADDI, exactly once, unrelated registers preserved\n";

  const uint64_t address = SpikeCore::ram_base + 0x2000;
  sim.mem_write(address, 0x8877665544332211ULL);
  uint64_t direct = 0;
  std::memcpy(&direct, sim.core.addr_to_mem(address), sizeof(direct));
  check(direct == 0x8877665544332211ULL, "direct RAM oracle disagrees with MEMWT");
  check(sim.mem_read(address) == direct, "MEMRD mismatch");
  check(sim.core.reg(5) == 11 && sim.core.reg(6) == before[6], "memory command clobbered scratch registers");
  std::cout << "PASS memory write/read through injected SD/LD, scratch registers preserved\n";

  // An ordinary U-mode program performs write(1, "hello\n", 6), then exit(0).
  // Load it exclusively through FASE MEMWT. The test driver handles its ECALLs.
  const uint64_t code = SpikeCore::ram_base;
  const uint64_t data = code + 0x1000;
  const std::vector<uint32_t> program{
    0x00100513, // addi a0,zero,1
    0x00001597, // auipc a1,1 (PC = code+4)
    0xffc58593, // addi a1,a1,-4
    0x00600613, // addi a2,zero,6
    0x04000893, // addi a7,zero,64
    0x00000073, // ecall
    0x00000513, // addi a0,zero,0
    0x05d00893, // addi a7,zero,93
    0x00000073, // ecall
    0x00000013  // padding
  };
  for (size_t i = 0; i < program.size(); i += 2)
    sim.mem_write(code + 4 * i, uint64_t(program[i]) | uint64_t(program[i+1]) << 32);
  sim.mem_write(data, 0x00000a6f6c6c6568ULL);
  sim.command({7, 0}, 1); // SYNCI
  sim.redirect(code); // real injected MEPC/mstatus/MRET sequence
  auto event = sim.next();
  check(event[1] == 0 && event[2] == 8, "write did not report a U-mode ECALL");
  check(Simulation::unpack(event, 3, 6) == code + 20, "wrong write ECALL PC");
  check(Simulation::unpack(event, 9, 6) == 64, "wrong syscall number");
  check(sim.reg_read(10) == 1 && sim.reg_read(11) == data && sim.reg_read(12) == 6,
        "write syscall arguments changed");
  const auto message_word = sim.mem_read(data);
  std::string message;
  for (unsigned i = 0; i < 6; ++i) message.push_back(message_word >> (8 * i));
  check(message == "hello\n", "write syscall buffer mismatch");
  sim.reg_write(10, 6); // Host write return value
  sim.redirect(code + 24);
  event = sim.next();
  check(event[1] == 0 && event[2] == 8, "exit did not report a U-mode ECALL");
  check(Simulation::unpack(event, 3, 6) == code + 32, "wrong exit ECALL PC");
  check(Simulation::unpack(event, 9, 6) == 93 && sim.reg_read(10) == 0, "exit arguments mismatch");
  sim.command({1, 0}, 1); // HALT
  check(sim.next()[1] == 0xff, "NEXT did not report all halted");
  check(sim.core.reg(5) == 11 && sim.core.reg(6) == before[6], "event recovery clobbered scratch registers");
  std::cout << "PASS U-mode write+exit, ECALL events, Host return, REDIR/MRET, HALT/NEXT\n";

  check(sim.accepted == sim.core.injected && !sim.pending,
        "accepted/completed instruction counts disagree");
}

static void probe_fp_read(Simulation& sim) {
  const uint64_t value = 0x0123456789abcdefULL;
  sim.reg_write(32 + 3, value);
  // Observe FPR through a separate Spike-executed instruction before checking
  // the controller's reply serialization.
  sim.inst(0xe20182d3); // fmv.x.d x5,f3
  check(sim.core.reg(5) == value, "floating-point register write mismatch");
  std::cout << "PASS Spike independently observes f3=" << hex64(sim.core.reg(5)) << '\n';
  const auto actual = sim.reg_read(32 + 3);
  check(actual == value, "floating-point REGRD expected " + hex64(value) + " actual " + hex64(actual));
  std::cout << "PASS floating-point register readback\n";
}

static volatile std::sig_atomic_t stop_server = 0;
static void request_stop(int) { stop_server = 1; }

// The original SerialFPGAAdapter can connect to the slave PTY without changes.
// The UART PHY is omitted; every command byte still passes through the RTL
// controller's original ready/valid interface.
static void serve_serial(Simulation& sim, const std::string& path_file) {
  const int master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
  check(master >= 0, "posix_openpt failed");
  check(grantpt(master) == 0 && unlockpt(master) == 0, "PTY setup failed");
  const char* slave_name = ptsname(master);
  check(slave_name != nullptr, "ptsname failed");
  const int slave = open(slave_name, O_RDWR | O_NOCTTY);
  check(slave >= 0, "open PTY slave failed");
  termios attributes{};
  check(tcgetattr(slave, &attributes) == 0, "tcgetattr failed");
  cfmakeraw(&attributes);
  check(tcsetattr(slave, TCSANOW, &attributes) == 0, "tcsetattr failed");
  {
    std::ofstream path(path_file);
    check(bool(path), "cannot write PTY path file");
    path << slave_name << '\n';
  }
  std::signal(SIGTERM, request_stop);
  std::signal(SIGINT, request_stop);
  std::cout << "READY " << slave_name << std::endl;
  while (!stop_server) {
    pollfd descriptor{master, POLLIN, 0};
    const int ready = poll(&descriptor, 1, 1);
    if (ready < 0 && errno != EINTR) throw std::runtime_error("PTY poll failed");
    if (descriptor.revents & POLLIN) {
      uint8_t bytes[4096];
      const auto count = read(master, bytes, sizeof(bytes));
      if (count > 0) sim.host_tx.insert(sim.host_tx.end(), bytes, bytes + count);
      else if (count < 0 && errno != EAGAIN && errno != EINTR)
        throw std::runtime_error("PTY read failed");
    }
    for (unsigned i = 0; i < 128; ++i) sim.tick();
    if (!sim.host_rx.empty()) {
      std::vector<uint8_t> bytes(sim.host_rx.begin(), sim.host_rx.end());
      const auto count = write(master, bytes.data(), bytes.size());
      if (count > 0)
        for (ssize_t i = 0; i < count; ++i) sim.host_rx.pop_front();
      else if (count < 0 && errno != EAGAIN && errno != EINTR)
        throw std::runtime_error("PTY write failed");
    }
  }
  close(slave);
  close(master);
  std::cout << "STOP cycles=" << sim.cycle << " injected=" << sim.core.injected
            << " user_steps=" << sim.core.user_steps
            << " a0=" << hex64(sim.core.reg(10)) << std::endl;
}

int main(int argc, char** argv) {
  try {
    Verilated::commandArgs(argc, argv);
    unsigned latency = 2, stall = 3;
    bool corrupt = false, probe_fp = false;
    std::string trace, pty_file, log_path = "instructions.log";
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--latency" && i+1 < argc) latency = std::stoul(argv[++i]);
      else if (arg == "--stall" && i+1 < argc) stall = std::stoul(argv[++i]);
      else if (arg == "--trace" && i+1 < argc) trace = argv[++i];
      else if (arg == "--insn-log" && i+1 < argc) log_path = argv[++i];
      else if (arg == "--pty-file" && i+1 < argc) pty_file = argv[++i];
      else if (arg == "--corrupt-addi") corrupt = true;
      else if (arg == "--probe-fp") probe_fp = true;
      else throw std::runtime_error("unknown argument: " + arg);
    }
    check(latency < 1000 && stall < 1000, "latency/stall must be below 1000");
    Simulation sim(latency, stall, corrupt, trace, log_path);
    if (!pty_file.empty()) {
      serve_serial(sim, pty_file);
      return 0;
    }
    if (probe_fp) probe_fp_read(sim);
    else run_checks(sim);
    std::cout << "PASS suite cycles=" << sim.cycle << " injected=" << sim.core.injected
              << " user_steps=" << sim.core.user_steps
              << " latency=" << latency << " stall=" << stall << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
