
#include "serialfpga.h"
#include "simroot.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include <sys/stat.h>
#include <sys/types.h>

int32_t get_baudrate_const(int baudrate) {
    switch(baudrate) {
        case 0:      return B0;
        case 50:     return B50;
        case 75:     return B75;
        case 110:    return B110;
        case 134:    return B134;
        case 150:    return B150;
        case 200:    return B200;
        case 300:    return B300;
        case 600:    return B600;
        case 1200:   return B1200;
        case 1800:   return B1800;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return (speed_t)-1;
    }
}

SerialFPGAAdapter::SerialFPGAAdapter(string devfile, uint32_t baudrate) {

    fd = open(devfile.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    simroot_assertf(fd > 0, "Open Serial Device %s Failed", devfile.c_str());

    struct termios tio;
    bzero(&tio, sizeof(tio) );
    simroot_assertf(tcgetattr(fd,&tio) == 0, "Setup Serail Device %s Failed", devfile.c_str());

    int rate = get_baudrate_const(baudrate);
    simroot_assertf(rate > 0, "Unknown Serial Speed %d", baudrate);
    cfsetospeed(&tio, rate);
    cfsetispeed(&tio, rate);
    
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;

    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;

    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 5;

    simroot_assertf(tcsetattr(fd,TCSANOW,&tio) == 0, "Setup Serail Device %s Failed", devfile.c_str());

    tcflush(fd,TCIFLUSH);
}

SerialFPGAAdapter::~SerialFPGAAdapter() {
    if(fd) close(fd);
}

void SerialFPGAAdapter::_read_serial(void * buf, uint64_t size) {
    int64_t sz = 0;
    while(sz < size) {
        int64_t ret = read(fd, (uint8_t*)(buf) + sz, size - sz);
        simroot_assertf(ret >= 0, "Read Serial Failed: %ld", ret);
        sz += ret;
    }
}

void SerialFPGAAdapter::_write_serial(void * buf, uint64_t size) {
    int64_t ret = write(fd, buf, size);
    simroot_assertf(ret == size, "Write Serial Failed: %ld", ret);
}

void SerialFPGAAdapter::_append_int(BufT &buf, int64_t data, uint64_t bytes) {
    simroot_assert(bytes > 0 && bytes <= 8);
    uint64_t sz = buf.size();
    buf.resize(sz + bytes);
    memcpy(buf.data() + sz, &data, bytes);
}
void SerialFPGAAdapter::_append_buf(BufT &buf, void * data, uint64_t bytes) {
    uint64_t sz = buf.size();
    buf.resize(sz + bytes);
    memcpy(buf.data() + sz, data, bytes);
}
int64_t SerialFPGAAdapter::_pop_int(BufT &buf, uint64_t bytes) {
    int64_t ret = 0;
    simroot_assert(bytes > 0 && bytes <= 8);
    memcpy(&ret, buf.data(), bytes);
    if(bytes < 8 && ret >> (bytes * 8 - 1)) ret |= (~((1 << (bytes * 8)) - 1));
    if(buf.size() > bytes) memmove(buf.data(), buf.data() + bytes, buf.size() - bytes);
    buf.resize(buf.size() - bytes);
    return ret;
}

const uint64_t SEROP_RET_BITS[SEROP_NUM] = {
    16+8+64,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    64,
    0,
    64,
    0,
    4096*8,
    0,
    0,
    0,
    64
};

int8_t SerialFPGAAdapter::_perform_op(int8_t op, BufT &data, BufT &retdata) {
    simroot_assert(op < SEROP_NUM && op >= 0);
    _write_serial(&op, 1);
    _write_serial(data.data(), data.size());
    retdata.resize(SEROP_RET_BITS[op] / 8);
    int8_t ret = 0;
    _read_serial(&ret, 1);
    _read_serial(retdata.data(), retdata.size());
    return ret;
}

void SerialFPGAAdapter::halt(uint32_t cpu_id) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    int8_t value = _perform_op(SEROP_HALT, buf, ret);
    simroot_assertf(SEROP_HALT == value, "Operation Halt on Core %d Failed: %d", cpu_id, value);
}

void SerialFPGAAdapter::interrupt(uint32_t cpu_id) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    int8_t value = _perform_op(SEROP_ITR, buf, ret);
    simroot_assertf(SEROP_ITR == value, "Operation Interrupt on Core %d Failed: %d", cpu_id, value);
}

void SerialFPGAAdapter::set_mmu(uint32_t cpu_id, PhysAddrT pgtable, AsidT asid) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, asid, 2);
    _append_int(buf, pgtable >> PAGE_ADDR_OFFSET, 5);
    int8_t value = _perform_op(SEROP_MMU, buf, ret);
    simroot_assertf(SEROP_MMU == value, "Operation SetMMU on Core %d (0x%lx, 0x%d) Failed: %d", cpu_id, pgtable >> PAGE_ADDR_OFFSET, asid, value);
}

void SerialFPGAAdapter::redirect(uint32_t cpu_id, VirtAddrT addr) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, addr, 6);
    int8_t value = _perform_op(SEROP_REDIR, buf, ret);
    simroot_assertf(SEROP_REDIR == value, "Operation Redirect on Core %d (0x%lx) Failed: %d", cpu_id, addr, value);
}

bool SerialFPGAAdapter::next(uint32_t *itr_cpu, uint32_t *itr_cause, RawDataT *itr_arg) {
    vector<uint8_t> buf, ret;
    int8_t value = _perform_op(SEROP_NEXT, buf, ret);
    simroot_assertf(SEROP_NEXT == value, "Operation Next Failed: %d", value);
    *itr_cpu = _pop_int(ret, 2);
    *itr_cause = _pop_int(ret, 1);
    *itr_arg = _pop_int(ret, 8);
    return true;
}

void SerialFPGAAdapter::flush_tlb_all(uint32_t cpu_id) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    int8_t value = _perform_op(SEROP_FTLB, buf, ret);
    simroot_assertf(SEROP_FTLB == value, "Operation FlushTLB on Core %d Failed: %d", cpu_id, value);
}

void SerialFPGAAdapter::flush_tlb_asid(uint32_t cpu_id, AsidT asid) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, asid, 2);
    int8_t value = _perform_op(SEROP_FTLB2, buf, ret);
    simroot_assertf(SEROP_FTLB2 == value, "Operation FlushTLB2 on Core %d (%d) Failed: %d", cpu_id, asid, value);
}

void SerialFPGAAdapter::flush_tlb_vpgidx(uint32_t cpu_id, VirtAddrT vaddr, AsidT asid) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, asid, 2);
    _append_int(buf, vaddr >> PAGE_ADDR_OFFSET, 5);
    int8_t value = _perform_op(SEROP_FTLB3, buf, ret);
    simroot_assertf(SEROP_FTLB3 == value, "Operation FlushTLB3 on Core %d (%d, 0x%lx) Failed: %d", cpu_id, asid, vaddr >> PAGE_ADDR_OFFSET, value);
}

RawDataT SerialFPGAAdapter::regacc_read(uint32_t cpu_id, RVRegIndexT vreg) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, vreg, 2);
    int8_t value = _perform_op(SEROP_REGRD, buf, ret);
    simroot_assertf(SEROP_REGRD == value, "Operation RegRead on Core %d (%d) Failed: %d", cpu_id, vreg, value);
    return _pop_int(ret, 8);
}

void SerialFPGAAdapter::regacc_write(uint32_t cpu_id, RVRegIndexT vreg, RawDataT data) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, vreg, 2);
    _append_int(buf, data, 8);
    int8_t value = _perform_op(SEROP_REGWT, buf, ret);
    simroot_assertf(SEROP_REGWT == value, "Operation RegWrite on Core %d (%d, 0x%lx) Failed: %d", cpu_id, vreg, data, value);
}

RawDataT SerialFPGAAdapter::pxymem_read(uint32_t cpu_id, PhysAddrT paddr) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, paddr, 8);
    int8_t value = _perform_op(SEROP_MEMRD, buf, ret);
    simroot_assertf(SEROP_MEMRD == value, "Operation RegRead on Core %d (0x%lx) Failed: %d", cpu_id, paddr, value);
    return _pop_int(ret, 8);
}

void SerialFPGAAdapter::pxymem_write(uint32_t cpu_id, PhysAddrT paddr, RawDataT data) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, paddr, 8);
    _append_int(buf, data, 8);
    int8_t value = _perform_op(SEROP_MEMWT, buf, ret);
    simroot_assertf(SEROP_MEMWT == value, "Operation RegWrite on Core %d (0x%lx, 0x%lx) Failed: %d", cpu_id, paddr, data, value);
}

void SerialFPGAAdapter::pxymem_page_read(uint32_t cpu_id, PageIndexT ppn, void * dbuf) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, ppn, 5);
    int8_t value = _perform_op(SEROP_PGRD, buf, ret);
    simroot_assertf(SEROP_PGRD == value, "Operation PageRead on Core %d (0x%lx) Failed: %d", cpu_id, ppn, value);
    simroot_assert(ret.size() == PAGE_LEN_BYTE);
    memcpy(dbuf, ret.data(), PAGE_LEN_BYTE);
}

void SerialFPGAAdapter::pxymem_page_set(uint32_t cpu_id, PageIndexT ppn, RawDataT value) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, ppn, 5);
    _append_int(buf, value, 8);
    int8_t rvalue = _perform_op(SEROP_PGST, buf, ret);
    simroot_assertf(SEROP_PGST == rvalue, "Operation PageSet on Core %d (0x%lx, 0x%ld) Failed: %d", cpu_id, ppn, value, rvalue);
}

void SerialFPGAAdapter::pxymem_page_write(uint32_t cpu_id, PageIndexT ppn, void * dbuf) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, ppn, 5);
    _append_buf(buf, dbuf, PAGE_LEN_BYTE);
    int8_t value = _perform_op(SEROP_PGWT, buf, ret);
    simroot_assertf(SEROP_PGWT == value, "Operation PageWrite on Core %d (0x%lx) Failed: %d", cpu_id, ppn, value);
}

void SerialFPGAAdapter::pxymem_page_copy(uint32_t cpu_id, PageIndexT dst, PageIndexT src) {
    vector<uint8_t> buf, ret;
    _append_int(buf, cpu_id, 2);
    _append_int(buf, dst, 5);
    _append_int(buf, src, 5);
    int8_t value = _perform_op(SEROP_PGCP, buf, ret);
    simroot_assertf(SEROP_PGCP == value, "Operation PageCopy on Core %d (0x%lx, 0x%lx) Failed: %d", cpu_id, dst, src, value);
}

uint64_t SerialFPGAAdapter::get_current_tick() {
    vector<uint8_t> buf, ret;
    int8_t value = _perform_op(SEROP_CLK, buf, ret);
    simroot_assertf(SEROP_CLK == value, "Operation Clock Failed: %d", value);
    return _pop_int(ret, 8);
}
