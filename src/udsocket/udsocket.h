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

#include "cpuv2/cpuinterface2.h"

using simcpu::CPUGroupInterface;

class UDSocketAdapter : public CPUGroupInterface, public TraceObject {

public:

    UDSocketAdapter(const string sockfile);
    ~UDSocketAdapter();

    virtual void process_frames(HTPFrames &frames);

    virtual void halt(uint32_t cpu_id) {
        HTPFrame frame(HTOP::halt, cpu_id);
        _process_single_op(frame);
    }
    virtual void interrupt(uint32_t cpu_id) {
        HTPFrame frame(HTOP::itr, cpu_id);
        _process_single_op(frame);
    }
    virtual void set_mmu(uint32_t cpu_id, PageIndexT pgtable, AsidT asid) {
        HTPFrame frame(HTOP::mmu, cpu_id, asid, pgtable);
        _process_single_op(frame);
    }
    virtual void redirect(uint32_t cpu_id, VirtAddrT addr) {
        HTPFrame frame(HTOP::redir, cpu_id, addr);
        _process_single_op(frame);
    }

    virtual bool next(uint32_t *itr_cpu, VirtAddrT *itr_pc, uint32_t *itr_cause, RawDataT *itr_arg) {
        HTPFrame frame(HTOP::next, 0);
        _process_single_op(frame);
        *itr_cpu = frame.cpuid;
        *itr_cause = static_cast<uint32_t>(frame.x1);
        *itr_pc = frame.x2;
        *itr_arg = frame.x3;
        return (*itr_cpu < CPUID_ALLHALT);
    }

    virtual void flush_tlb_all(uint32_t cpu_id) {
        HTPFrame frame(HTOP::ftlb, cpu_id);
        _process_single_op(frame);
    }
    virtual void flush_tlb_asid(uint32_t cpu_id, AsidT asid) { flush_tlb_vpgidx(cpu_id, 0, asid); };
    virtual void flush_tlb_vpgidx(uint32_t cpu_id, VPageIndexT vpn, AsidT asid) {
        HTPFrame frame(HTOP::ftlb2, cpu_id, asid, vpn);
        _process_single_op(frame);
    }

    virtual void sync_inst_stream(uint32_t cpu_id) {
        HTPFrame frame(HTOP::synci, cpu_id);
        _process_single_op(frame);
    }

    virtual RawDataT regacc_read(uint32_t cpu_id, RVRegIndexT vreg) {
        HTPFrame frame(HTOP::regrd, cpu_id, vreg);
        _process_single_op(frame);
        return frame.x1;
    }
    virtual void regacc_write(uint32_t cpu_id, RVRegIndexT vreg, RawDataT data) {
        HTPFrame frame(HTOP::regwt, cpu_id, vreg, data);
        _process_single_op(frame);
    }

    virtual RawDataT pxymem_read(uint32_t cpu_id, PhysAddrT paddr) {
        HTPFrame frame(HTOP::memrd, cpu_id, paddr);
        _process_single_op(frame);
        return frame.x1;
    }
    virtual void pxymem_write(uint32_t cpu_id, PhysAddrT paddr, RawDataT data) {
        HTPFrame frame(HTOP::memwt, cpu_id, paddr, data);
        _process_single_op(frame);
    }

    virtual void pxymem_page_read(uint32_t cpu_id, PageIndexT ppn, void * dbuf) {
        HTPFrame frame(HTOP::pgrd, cpu_id, ppn);
        _process_single_op(frame);
        memcpy(dbuf, frame.d1.data(), PAGE_LEN_BYTE);
    }
    virtual void pxymem_page_set(uint32_t cpu_id, PageIndexT ppn, RawDataT value) {
        HTPFrame frame(HTOP::pgst, cpu_id, ppn, value);
        _process_single_op(frame);
    }
    virtual void pxymem_page_write(uint32_t cpu_id, PageIndexT ppn, void * dbuf) {
        HTPFrame frame(HTOP::pgwt, cpu_id, ppn, (uint8_t*)dbuf, PAGE_LEN_BYTE);
        _process_single_op(frame);
    }
    virtual void pxymem_page_copy(uint32_t cpu_id, PageIndexT dst, PageIndexT src) {
        HTPFrame frame(HTOP::pgcp, cpu_id, dst, src);
        _process_single_op(frame);
    }
    virtual void pxymem_page_zero(uint32_t cpu_id, PageIndexT ppn) {
        HTPFrame frame(HTOP::pgzero, cpu_id, ppn);
        _process_single_op(frame);
    }

    virtual uint64_t get_current_tick() {
        HTPFrame frame(HTOP::clk, 0);
        _process_single_op(frame);
        return frame.x1;
    }
    virtual uint64_t get_current_utick(uint32_t cpu_id) {
        HTPFrame frame(HTOP::uclk, cpu_id);
        _process_single_op(frame);
        return frame.x1;
    }


    virtual void hfutex_setmask(uint32_t cpu_id, VirtAddrT vaddr) {
        HTPFrame frame(HTOP::hfset, cpu_id, vaddr);
        _process_single_op(frame);
    }
    virtual void hfutex_clearmask(uint32_t cpu_id) {
        HTPFrame frame(HTOP::hfclr, cpu_id);
        _process_single_op(frame);
    }

    virtual void dump_core(std::ofstream &ofile) {};
    virtual void set_debug(bool on) { debug_op = on; };

private:

    inline void _process_single_op(HTPFrame &frame) {
        HTPFrames frames;
        frames.push_back(frame);
        process_frames(frames);
        frame = frames.front();
    }

    void recv_blocked(uint8_t *buf, const uint64_t size);
    void send_blocked(const uint8_t *buf, const uint64_t size);

    int32_t fd = -1;
    bool debug_op = false;
};






