
#include "serial/serialfpga.h"

#include "configuration.h"
#include "simroot.h"

#include "sysv2/pagetablev2.h"
#include "sysv2/pagememv2.h"

bool test_serial_1(string dev_path) {

    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    printf("Test 1 Start\n");

    printf("Save Reg X5 as 0x1122334455667788\n");
    dev->regacc_write(0, 5, 0x1122334455667788UL);
    
    printf("Read Reg X5\n");
    assert(0x1122334455667788UL == dev->regacc_read(0, 5));

    printf("Write 0x8001_0000 as 0x0102030405060708\n");
    dev->pxymem_write(0, 0x80010000UL, 0x0102030405060708UL);

    printf("Read 0x8001_0000\n");
    assert(0x0102030405060708UL == dev->pxymem_read(0, 0x80010000UL));

    printf("Read Reg X5\n");
    assert(0x1122334455667788UL == dev->regacc_read(0, 5));

    printf("Test 1 PASSED\n");

    return true;
}

bool test_serial_2(string dev_path) {

    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    printf("Test 2 Start\n");

    printf("Set Page 0x8001_0 as 0x8877665544332211\n");
    dev->pxymem_page_set(0, 0x80010UL, 0x8877665544332211UL);

    printf("Write 0x8001_0080 as 0x0505050505050505\n");
    dev->pxymem_write(0, 0x80010080UL, 0x0505050505050505UL);

    printf("Read 0x8001_0000\n");
    assert(0x8877665544332211UL == dev->pxymem_read(0, 0x80010000UL));

    printf("Read 0x8001_0040\n");
    assert(0x8877665544332211UL == dev->pxymem_read(0, 0x80010040UL));

    printf("Read 0x8001_0080\n");
    assert(0x0505050505050505UL == dev->pxymem_read(0, 0x80010080UL));

    printf("Copy Page 0x8001_0 to 0x8004_0\n");
    dev->pxymem_page_copy(0, 0x80040UL, 0x80010UL);
    
    printf("Read 0x8004_0000\n");
    assert(0x8877665544332211UL == dev->pxymem_read(0, 0x80040000UL));

    printf("Read 0x8004_0040\n");
    assert(0x8877665544332211UL == dev->pxymem_read(0, 0x80040040UL));

    printf("Read 0x8004_0080\n");
    assert(0x0505050505050505UL == dev->pxymem_read(0, 0x80040080UL));
    
    printf("Test 2 PASSED\n");

    return true;
}


bool test_serial_3(string dev_path) {

    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    printf("Test 3 Start\n");

    vector<uint64_t> pg;
    pg.assign(512, 0);
    for(uint64_t i = 0; i < 512; i++) pg[i] = i*8 + 0x4000UL;

    printf("Write Page 0x8001_0\n");
    dev->pxymem_page_write(0, 0x80010UL, pg.data());

    printf("Read 0x8001_0000\n");
    assert(0x4000UL == dev->pxymem_read(0, 0x80010000UL));

    printf("Read 0x8001_0100\n");
    assert(0x4100UL == dev->pxymem_read(0, 0x80010100UL));

    printf("Read 0x8001_0400\n");
    assert(0x4400UL == dev->pxymem_read(0, 0x80010400UL));

    printf("Copy Page 0x8001_0 to 0x8004_0\n");
    dev->pxymem_page_copy(0, 0x80040UL, 0x80010UL);
    
    printf("Read 0x8004_0000\n");
    assert(0x4000UL == dev->pxymem_read(0, 0x80040000UL));

    printf("Read 0x8004_0140\n");
    assert(0x4140UL == dev->pxymem_read(0, 0x80040140UL));

    printf("Read 0x8004_0880\n");
    assert(0x4880UL == dev->pxymem_read(0, 0x80040880UL));

    vector<uint64_t> pgrd;
    pgrd.assign(512, 0);
    printf("Read Page 0x8004_0\n");
    dev->pxymem_page_read(0, 0x80040UL, pgrd.data());
    for(uint64_t i = 0; i < 512; i++) assert(pgrd[i] == pg[i]);
    
    printf("Test 3 PASSED\n");

    return true;
}

bool test_serial_4(string dev_path) {

    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    printf("Test 4 Start\n");

    printf("Clear Page 0x82000 - 0x82003\n");
    dev->pxymem_page_set(0, 0x82000UL, 0);
    dev->pxymem_page_set(0, 0x82001UL, 0);
    dev->pxymem_page_set(0, 0x82002UL, 0);
    dev->pxymem_page_set(0, 0x82003UL, 0);

    printf("Init PageTable of VPage 0x10000 -> 0x82003\n");
    
    dev->pxymem_write(0, 0x82000000UL + 8 * ((0x10000UL >> 18) & 0x1ff), (0x82001UL << 10) + 1);
    dev->pxymem_write(0, 0x82001000UL + 8 * ((0x10000UL >> 9) & 0x1ff), (0x82002UL << 10) + 1);
    dev->pxymem_write(0, 0x82002000UL + 8 * ((0x10000UL) & 0x1ff), (0x82003UL << 10) + 0xdf);
    dev->pxymem_write(0, 0x82003000UL, 0x06400893UL | (0x00000073UL << 32));

    printf("Setup MMU\n");
    dev->set_mmu(0, 0x82000000UL, 0);

    printf("Flush TLB\n");
    dev->flush_tlb_all(0);

    printf("\nFence-I\n");
    dev->sync_inst_stream(0);
    
    printf("Start ILA Trigger, and Type \"1\" to Continue...\n");
    do {
        if(getchar() == '1') break;
    } while(true);

    printf("\nRedirect to VAddr 0x10000000\n");
    dev->redirect(0, 0x10000000UL);

    uint32_t cpuid = 0;
    VirtAddrT pc = 0;
    uint32_t cause = 0;
    uint64_t arg = 0;
    assert(dev->next(&cpuid, &pc, &cause, &arg));

    printf("Got Event on CPU %d, @0x%lx, Cause %d, Arg 0x%lx\n", cpuid, pc, cause, arg);

    printf("Test 4 PASSED\n");

    return true;
}

bool test_serial_5(string dev_path) {

    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    printf("Test 5 Start\n");

    printf("Random DDR RW Test on 0x%lx\n", mem_base);
    uint64_t mem_size = (1UL << 30);

    std::unordered_map<uint64_t, uint64_t> sts;
    
    for(int i = 0; i < 100; i++) {
        uint64_t addr = (ALIGN(rand_long(), 8) % mem_size) + mem_base;
        uint64_t data = rand_long();
        sts.insert(std::make_pair(addr, data));
        dev->pxymem_write(0, addr, data);
    }

    int passed = 0;
    int failed = 0;
    for(auto &e : sts) {
        uint64_t data = dev->pxymem_read(0, e.first);
        if(data != e.second) {
            failed++;
            printf("@0x%lx: supposed 0x%lx, get 0x%lx\n", e.first, e.second, data);
        } else {
            passed++;
        }
    }

    printf("Finished, %d passed, %d failed\n", passed, failed);

    return true;
}

void _target_memst(SerialFPGAAdapter *dev, TgtMemSet64 &st) {
    if(st.dwords == PAGE_LEN_BYTE/8 && !(st.base & (PAGE_LEN_BYTE - 1))) {
        if(st.multivalue.size() == st.dwords) {
            dev->pxymem_page_write(0, st.base >> PAGE_ADDR_OFFSET, st.multivalue.data());
            printf("Page Write @0x%lx\n", st.base >> PAGE_ADDR_OFFSET);
        } else {
            dev->pxymem_page_set(0, st.base >> PAGE_ADDR_OFFSET, st.value);
            printf("Page Set @0x%lx: 0x%lx\n", st.base >> PAGE_ADDR_OFFSET, st.value);
        }
    } else {
        if(st.multivalue.size() == st.dwords) {
            for(uint64_t i = 0; i < st.dwords; i++) {
                dev->pxymem_write(0, st.base + i * 8, st.multivalue[i]);
                printf("Mem Write @0x%lx: 0x%lx\n", st.base + i * 8, st.multivalue[i]);
            }
        } else {
            for(uint64_t i = 0; i < st.dwords; i++) {
                dev->pxymem_write(0, st.base + i * 8, st.value);
                printf("Mem Write @0x%lx: 0x%lx\n", st.base + i * 8, st.value);
            }
        }
    }
}

bool test_serial_6(string dev_path) {
    
    uint32_t baudrate = conf::get_int("serial", "baudrate", 115200);

    uint64_t mem_base = conf::get_inthex("root", "memory_base_addr_hex", 0);
    simroot_assert((mem_base % PAGE_LEN_BYTE) == 0);

    SerialFPGAAdapter * dev = new SerialFPGAAdapter(dev_path, baudrate);

    const uint64_t test_data = 0x1122334455667788UL;

    printf("Test 6 Start\n");

    TgtMemSetList stlist;

    unique_ptr<PhysPageAllocatorV2> ppman = make_unique<PhysPageAllocatorV2>(mem_base, 512UL * 1024UL * 1024UL);

    unique_ptr<PageTable4K> pt = make_unique<PageTable4K>(PTType::SV39, ppman.get(), &stlist);

    PhysAddrT pt_base = pt->get_page_table_base();

    printf("Init test pagetable on physical address 0x%lx\n", pt_base);

    PageIndexT inst_pg = ppman->alloc();
    PageIndexT data_pg = ppman->alloc();
    VPageIndexT inst_vpn = 0x20UL;
    VPageIndexT data_vpn = 0x10UL;

    pt->pt_insert(inst_vpn, (inst_pg << 10) | PTE_LEAF_V | PTE_R | PTE_X, &stlist);
    pt->pt_insert(data_vpn, (data_pg << 10) | PTE_LEAF_V | PTE_R | PTE_W, &stlist);

    vector<uint32_t> insts;
    insts.push_back(0x000102b7U); // lui	t0,0x10
    insts.push_back(0x0002b503U); // ld	a0,0(t0) # 10000
    insts.push_back(0x06400893U); // li	a7,100
    insts.push_back(0x00000073U); // ecall
    for(int i = 0; i < 12; i++) {
        insts.push_back(0x00000013U);
    }

    stlist.emplace_back();
    stlist.back().base = (inst_pg << 12);
    stlist.back().dwords = insts.size()/2;
    stlist.back().value = 0;
    stlist.back().multivalue.assign(insts.size()/2, 0);
    memcpy(stlist.back().multivalue.data(), insts.data(), (insts.size()/2)*8);

    stlist.emplace_back();
    stlist.back().base = (data_pg << 12);
    stlist.back().dwords = insts.size()/2;
    stlist.back().value = test_data;

    for(auto &st : stlist) {
        _target_memst(dev, st);
    }

    printf("Setup MMU\n");
    dev->set_mmu(0, pt_base, 0);

    printf("Flush TLB\n");
    dev->flush_tlb_all(0);

    printf("\nFence-I\n");
    dev->sync_inst_stream(0);
    
    printf("Start ILA Trigger, and Type \"1\" to Continue...\n");
    do {
        if(getchar() == '1') break;
    } while(true);

    printf("\nRedirect to VAddr 0x%lx\n", inst_vpn << 12);
    dev->redirect(0, inst_vpn << 12);

    uint32_t cpuid = 0;
    VirtAddrT pc = 0;
    uint32_t cause = 0;
    uint64_t arg = 0;
    assert(dev->next(&cpuid, &pc, &cause, &arg));

    printf("Got Event on CPU %d, @0x%lx, Cause %d, Arg 0x%lx\n", cpuid, pc, cause, arg);

    printf("A7 Value: 0x%lx, A0 Value: 0x%lx\n",
        dev->regacc_read(0, isa::ireg_index_of("a7")),
        dev->regacc_read(0, isa::ireg_index_of("a0"))
    );

    printf("Test 6 PASSED\n");

    return true;

}

