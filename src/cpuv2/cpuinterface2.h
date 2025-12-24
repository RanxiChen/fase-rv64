// MIT License

// Copyright (c) 2025 Meng Chengzhen, in Shandong University

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#pragma once

#include "common.h"
#include "simerror.h"

#include "htp.hpp"

#include "cpu/isa.h"

#include <deque>

using isa::RVRegArray;

typedef list<HTPFrame> HTPFrames;

void htp_push_halt(HTPFrames &frames, uint32_t cpu_id);
void htp_push_interrupt(HTPFrames &frames, uint32_t cpu_id);
void htp_push_set_mmu(HTPFrames &frames, uint32_t cpu_id, PhysAddrT pgtable, AsidT asid);
void htp_push_redirect(HTPFrames &frames, uint32_t cpu_id, VirtAddrT addr);
void htp_push_next(HTPFrames &frames);
void htp_push_flush_tlb_all(HTPFrames &frames, uint32_t cpu_id);
void htp_push_flush_tlb_vpgidx(HTPFrames &frames, uint32_t cpu_id, VirtAddrT vaddr, AsidT asid);
void htp_push_sync_inst_stream(HTPFrames &frames, uint32_t cpu_id);
void htp_push_regacc_read(HTPFrames &frames, uint32_t cpu_id, RVRegIndexT vreg);
void htp_push_regacc_write(HTPFrames &frames, uint32_t cpu_id, RVRegIndexT vreg, RawDataT data);
void htp_push_pxymem_read(HTPFrames &frames, uint32_t cpu_id, PhysAddrT paddr);
void htp_push_pxymem_write(HTPFrames &frames, uint32_t cpu_id, PhysAddrT paddr, RawDataT data);
void htp_push_pxymem_page_read(HTPFrames &frames, uint32_t cpu_id, PageIndexT ppn);
void htp_push_pxymem_page_set(HTPFrames &frames, uint32_t cpu_id, PageIndexT ppn, RawDataT value);
void htp_push_pxymem_page_write(HTPFrames &frames, uint32_t cpu_id, PageIndexT ppn, void *data);
void htp_push_pxymem_page_copy(HTPFrames &frames, uint32_t cpu_id, PageIndexT dst, PageIndexT src);
void htp_push_pxymem_page_zero(HTPFrames &frames, uint32_t cpu_id, PageIndexT ppn);
void htp_push_get_current_tick(HTPFrames &frames);
void htp_push_get_current_utick(HTPFrames &frames, uint32_t cpu_id);
void htp_push_hfutex_setmask(HTPFrames &frames, uint32_t cpu_id, VirtAddrT vaddr);
void htp_push_hfutex_clearmask(HTPFrames &frames, uint32_t cpu_id);

void htp_pop_next(HTPFrames &frames, uint32_t *itr_cpu, VirtAddrT *itr_pc, uint32_t *itr_cause, RawDataT *itr_arg);
void htp_pop_regacc_read(HTPFrames &frames, RawDataT *data);
void htp_pop_pxymem_read(HTPFrames &frames, RawDataT *data);
void htp_pop_page_read(HTPFrames &frames, void *data);
void htp_pop_get_current_tick(HTPFrames &frames, uint64_t *data);
void htp_pop_get_current_utick(HTPFrames &frames, uint64_t *data);

uint64_t htp_pop_regacc_read(HTPFrames &frames);
uint64_t htp_pop_pxymem_read(HTPFrames &frames);
uint64_t htp_pop_get_current_tick(HTPFrames &frames);
uint64_t htp_pop_get_current_utick(HTPFrames &frames);

HTOP htp_pop_next_return(HTPFrames &frames);

namespace simcpu {

class CPUGroupInterface {
public:
    virtual void process_frames(HTPFrames &frames) = 0;

    virtual void halt(uint32_t cpu_id) = 0;
    virtual void interrupt(uint32_t cpu_id) = 0;
    virtual void set_mmu(uint32_t cpu_id, PhysAddrT pgtable, AsidT asid) = 0;
    virtual void redirect(uint32_t cpu_id, VirtAddrT addr) = 0;

    virtual bool next(uint32_t *itr_cpu, VirtAddrT *itr_pc, uint32_t *itr_cause, RawDataT *itr_arg) = 0;

    virtual void flush_tlb_all(uint32_t cpu_id) = 0;
    virtual void flush_tlb_asid(uint32_t cpu_id, AsidT asid) = 0;
    virtual void flush_tlb_vpgidx(uint32_t cpu_id, VirtAddrT vaddr, AsidT asid) = 0;

    virtual void sync_inst_stream(uint32_t cpu_id) {};

    virtual RawDataT regacc_read(uint32_t cpu_id, RVRegIndexT vreg) = 0;
    virtual void regacc_write(uint32_t cpu_id, RVRegIndexT vreg, RawDataT data) = 0;

    virtual RawDataT pxymem_read(uint32_t cpu_id, PhysAddrT paddr) = 0;
    virtual void pxymem_write(uint32_t cpu_id, PhysAddrT paddr, RawDataT data) = 0;

    virtual void pxymem_page_read(uint32_t cpu_id, PageIndexT ppn, void * buf) = 0;
    virtual void pxymem_page_set(uint32_t cpu_id, PageIndexT ppn, RawDataT value) = 0;
    virtual void pxymem_page_write(uint32_t cpu_id, PageIndexT ppn, void * buf) = 0;
    virtual void pxymem_page_copy(uint32_t cpu_id, PageIndexT dst, PageIndexT src) = 0;
    virtual void pxymem_page_zero(uint32_t cpu_id, PageIndexT ppn) = 0;

    virtual uint64_t get_current_tick() = 0;
    virtual uint64_t get_current_utick(uint32_t cpu_id) = 0;

    virtual void hfutex_setmask(uint32_t cpu_id, VirtAddrT vaddr) = 0;
    virtual void hfutex_clearmask(uint32_t cpu_id) = 0;
};


}

