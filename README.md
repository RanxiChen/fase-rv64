# FASE (FPGA-Assisted Syscall Emulation) for RV64 ISA

(Working in Progress)

在FPGA上运行用户模式模拟

- 基于RV64GC指令集
- 仅在FPGA上运行核心与内存等部分硬件，无任何外设
- 直接加载Linux ELF可执行文件
- ecall与exception通过串口代理到主机处理

## 软件构建

### 依赖：

- Linux内核版本：**5.15.\*** *（不同的内核版本可能导致主机系统调用接口不同，详细的版本支持信息还需测试）*
- 编译工具与内核头文件: **gcc, g++, cmake, linux-headers**
- RV交叉编译工具：**riscv64-linux-gnu-gcc**


### 编译：

```bash
mkdir build
cd build
cmake ..
make -j16
```

### 编译后的内容：

- **build/nullrvsim**: 模拟器的可执行文件
- **build/conf/default.ini**: 默认配置文件，可通过命令行参数-c重新指定
- **build/example/\***: 用于测试的RV64GC可执行文件与相关数据文件

## 硬件构建

详细的硬件集成方法请前往**hardware/chisel**目录查看

### 依赖：

- chisel3

### 生成verilog：

```bash
cd hardware/chisel
sbt run
```

## 目前的内容

软件特性支持：

- 中断处理支持：ECALL，PageFault
- Linux系统调用支持：文件IO，虚拟内存，mmap，多线程，futex
- 测试用例（RV64GC用户态ELF，可动态链接）（待整理补充）

通讯物理层：

- 串口
- PCIE-XDMA（暂未实现）


## 软件模拟测试

```bash
cd build
./nullrvsim sim -w example/helloworld.riscv
```
成功运行程序，输出程序运行过程中的系统调用记录与HelloWorld字样，则构建成功

## 软硬件协同测试

```bash
cd build
./nullrvsim test1 -s /dev/ttyUSB0
```

其中-s参数为连接硬件控制器的串口设备，test1为测试项1，共有test1至test6六项，全部通过则硬件构建成功。

## 运行FASE

检查配置文件**build/conf/default.ini**:

- **baudrate**：串口波特率，务必与硬件控制器chisel代码中的一致

- **core_num**：FPGA上的RV64核心数量
- **memory_size_mb**：FPGA上核心可用的物理内存大小
- **memory_base_addr_hex**：FPGA上核心可用的物理内存基址
- **vm_is_sv48**：软件是否使用SV48页表，为0则使用SV39页表
- **hard_fp**：软件是否需要处理浮点寄存器
- **using_asid**：软件是否启用ASID机制
- **flush_tlb_all**：软件是否在所有需要刷新TLB的时候均刷新整个TLB

- **ld_path**：软件寻找RV64动态链接库的绝对路径

```bash
cd build
./nullrvsim serial -s /dev/ttyUSB0 -w example/helloworld.riscv
./nullrvsim serial -s /dev/ttyUSB0 -e ENV1=123 -e ENV2=abc -w example/helloworld.riscv
```


## 感谢

使用/包含的项目：
- Easylogging++: [abumq/easyloggingpp](https://github.com/abumq/easyloggingpp)
- ELFIO: [serge1/ELFIO](https://github.com/serge1/ELFIO)




