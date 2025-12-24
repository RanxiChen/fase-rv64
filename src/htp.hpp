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

#include <deque>

using std::deque;


#define SEROP_NEXT          (0)     //  OP[8]                               -> ACK[8] ID[8] CAUSE[8] PC[48] (ARG[48])
#define SEROP_HALT          (1)     //  OP[8]       ID[8]                  -> ACK[8]
#define SEROP_ITR           (2)     //  OP[8]       ID[8]                  -> ACK[8]
#define SEROP_MMU           (3)     //  OP[8]       ID[8] ASID[16] PT[40]  -> ACK[8]
#define SEROP_REDIR         (4)     //  OP[8]       ID[8] PC[48]           -> ACK[8]
#define SEROP_FTLB          (5)     //  OP[8]       ID[8]                  -> ACK[8]
#define SEROP_FTLB2         (6)     //  OP[8]       ID[8] ASID[16] VPN[40] -> ACK[8]
#define SEROP_SYNCI         (7)     //  OP[8]       ID[8]                  -> ACK[8]
#define SEROP_REGRD         (8)     //  OP[8]       ID[8] REG[16]          -> ACK[8] DATA[64]
#define SEROP_REGWT         (9)     //  OP[8]       ID[8] REG[16] DATA[64] -> ACK[8]
#define SEROP_MEMRD         (10)    //  OP[8]       ID[8] PA[48]           -> ACK[8] DATA[64]
#define SEROP_MEMWT         (11)    //  OP[8]       ID[8] PA[48] DATA[64]  -> ACK[8]
#define SEROP_PGRD          (12)    // (div8) OFF[3]OP[5] ID[8] PPN[40]          -> ACK[8] DATA[512B]
#define SEROP_PGST          (13)    //  OP[8]       ID[8] PPN[40] VALUE[64]-> ACK[8]
#define SEROP_PGWT          (14)    // (div8) OFF[3]OP[5] ID[8] PPN[40]          DATA[512B]-> ACK[8]
#define SEROP_PGCP          (15)    //  OP[8]       ID[8] DST[40] SRC[40]  -> ACK[8]
#define SEROP_CLK           (16)    //  OP[8]                               -> ACK[8] CLK[64]
#define SEROP_UCLK          (17)    //  OP[8]       ID[8]                  -> ACK[8] CLK[64]
#define SEROP_HFSET         (18)    //  OP[8]       ID[8] VA[48]           -> ACK[8]
#define SEROP_HFCLR         (19)    //  OP[8]       ID[8]                  -> ACK[8]
#define SEROP_PGZERO        (20)    //  OP[8]       ID[8] PPN[40]          -> ACK[8]
#define SEROP_NUM           (21)

enum class HTOP {
    next = SEROP_NEXT,          //  void                -> X1ID X2CAUSE X3PC D1ARG
    halt = SEROP_HALT,          //  ID
    itr = SEROP_ITR,            //  ID
    mmu = SEROP_MMU,            //  ID  X1ASID  X2PTADDR
    redir = SEROP_REDIR,        //  ID  X1PC
    ftlb = SEROP_FTLB,          //  ID 
    ftlb2 = SEROP_FTLB2,        //  ID  X1ASID  X2VADDR
    synci = SEROP_SYNCI,        //  ID
    regrd = SEROP_REGRD,        //  ID  X1REG           ->    X2DATA
    regwt = SEROP_REGWT,        //  ID  X1REG  X2DATA  
    memrd = SEROP_MEMRD,        //  ID  X1PA            ->    X2DATA
    memwt = SEROP_MEMWT,        //  ID  X1PA  X2DATA
    pgrd = SEROP_PGRD,          //  ID  X1PPN           ->    D1DATA
    pgst = SEROP_PGST,          //  ID  X1PPN  X2VALUE
    pgwt = SEROP_PGWT,          //  ID  X1PPN  D1DATA
    pgcp = SEROP_PGCP,          //  ID  X1DST  X2SRC
    clk = SEROP_CLK,            //  void                ->    X1CLK
    uclk = SEROP_UCLK,          //  ID                  ->    X1CLK
    hfset = SEROP_HFSET,        //  ID  X1VA
    hfclr = SEROP_HFCLR,        //  ID 
    pgzero = SEROP_PGZERO,      //  ID  X1PPN

    pgrd1 = (SEROP_PGRD | (1U << 5)),
    pgrd2 = (SEROP_PGRD | (2U << 5)),
    pgrd3 = (SEROP_PGRD | (3U << 5)),
    pgrd4 = (SEROP_PGRD | (4U << 5)),
    pgrd5 = (SEROP_PGRD | (5U << 5)),
    pgrd6 = (SEROP_PGRD | (6U << 5)),
    pgrd7 = (SEROP_PGRD | (7U << 5)),
    pgwt1 = (SEROP_PGWT | (1U << 5)),
    pgwt2 = (SEROP_PGWT | (2U << 5)),
    pgwt3 = (SEROP_PGWT | (3U << 5)),
    pgwt4 = (SEROP_PGWT | (4U << 5)),
    pgwt5 = (SEROP_PGWT | (5U << 5)),
    pgwt6 = (SEROP_PGWT | (6U << 5)),
    pgwt7 = (SEROP_PGWT | (7U << 5)),
};

struct HTOPMeta {
    bool has_id;
    uint32_t x1_bytes;
    uint32_t x2_bytes;
    uint32_t x3_bytes;
    uint32_t d1_bytes;
    uint32_t resp_x1_bytes;
    uint32_t resp_x2_bytes;
    uint32_t resp_x3_bytes;
    uint32_t resp_d1_bytes;
};

constexpr std::array<HTOPMeta, SEROP_NUM> HTOPMetas = {{
    {false, 0, 0, 0, 0,     1, 1, 6, 6},           // next
    {true,  0, 0, 0, 0,     0, 0, 0, 0},            // halt
    {true,  0, 0, 0, 0,     0, 0, 0, 0},            // itr
    {true,  2, 5, 0, 0,     0, 0, 0, 0},            // mmu
    {true,  6, 0, 0, 0,     0, 0, 0, 0},            // redir

    {true,  0, 0, 0, 0,     0, 0, 0, 0},            // ftlb
    {true,  2, 5, 0, 0,     0, 0, 0, 0},            // ftlb2
    {true,  0, 0, 0, 0,     0, 0, 0, 0},            // synci
    {true,  2, 0, 0, 0,     8, 0, 0, 0},            // regrd
    {true,  2, 8, 0, 0,     0, 0, 0, 0},            // regwt

    {true,  6, 0, 0, 0,     8, 0, 0 ,0},            // memrd
    {true,  6, 8 ,0 ,0 ,    0 ,0 ,0 ,0},           // memwt
    {true,  5 ,0 ,0 ,0 ,    0 ,0 ,0 ,512},         // pgrd
    {true,  5 ,8 ,0 ,0 ,    0 ,0 ,0 ,0},           // pgst
    {true,  5 ,0 ,0 ,512 ,  0 ,0 ,0 ,0},            // pgwt

    {true,  5 ,5 ,0 ,0 ,    0 ,0 ,0 ,0},           // pgcp
    {false, 0 ,0 ,0 ,0 ,    8 ,0 ,0 ,0},           // clk
    {true,  0 ,0 ,0 ,0 ,    8 ,0 ,0 ,0},           // uclk
    {true,  6 ,0 ,0 ,0 ,    0 ,0 ,0 ,0},           // hfset
    {true,  0 ,0 ,0 ,0 ,    0 ,0 ,0 ,0},           // hfclr

    {true,  5 ,0 ,0 ,0 ,    0 ,0 ,0 ,0},           // pgzero
}};

class HTPFrame {
public:
    HTOP        opcode = HTOP::next;
    uint32_t    cpuid = 0;
    uint64_t    x1 = 0;
    uint64_t    x2 = 0;
    uint64_t    x3 = 0;
    vector<uint8_t> d1;

    inline uint64_t send_size() {
        const HTOPMeta &meta = HTOPMetas[static_cast<uint32_t>(opcode) & 0x1F];
        return (meta.has_id ? 1 : 0) + meta.x1_bytes + meta.x2_bytes + meta.x3_bytes + meta.d1_bytes;
    }

    inline uint64_t recv_size() {
        const HTOPMeta &meta = HTOPMetas[static_cast<uint32_t>(opcode) & 0x1F];
        return meta.resp_x1_bytes + meta.resp_x2_bytes + meta.resp_x3_bytes + meta.resp_d1_bytes;
    }

    inline void serialize_request(vector<uint8_t> &buf) {
        const HTOPMeta &meta = HTOPMetas[static_cast<uint32_t>(opcode) & 0x1F];

        buf.push_back(static_cast<uint8_t>(opcode));
        if (meta.has_id) {
            buf.push_back(static_cast<uint8_t>(cpuid));
        }
        for (uint32_t i = 0; i < meta.x1_bytes; i++) {
            buf.push_back((x1 >> (i * 8)) & 0xFF);
        }
        for (uint32_t i = 0; i < meta.x2_bytes; i++) {
            buf.push_back((x2 >> (i * 8)) & 0xFF);
        }
        for (uint32_t i = 0; i < meta.x3_bytes; i++) {
            buf.push_back((x3 >> (i * 8)) & 0xFF);
        }
        if (meta.d1_bytes > 0) {
            buf.insert(buf.end(), d1.begin(), d1.end());
        }
    }
    inline void deserialize_response(const uint8_t *buf) {
        const HTOPMeta &meta = HTOPMetas[static_cast<uint32_t>(opcode) & 0x1F];
        uint32_t offset = 0;
        x1 = 0;
        x2 = 0;
        x3 = 0;
        for (uint32_t i = 0; i < meta.resp_x1_bytes; i++) {
            x1 |= (static_cast<uint64_t>(buf[offset++]) << (i * 8));
        }
        for (uint32_t i = 0; i < meta.resp_x2_bytes; i++) {
            x2 |= (static_cast<uint64_t>(buf[offset++]) << (i * 8));
        }
        for (uint32_t i = 0; i < meta.resp_x3_bytes; i++) {
            x3 |= (static_cast<uint64_t>(buf[offset++]) << (i * 8));
        }
        if (meta.resp_d1_bytes > 0) {
            d1.clear();
            d1.reserve(meta.resp_d1_bytes);
            d1.insert(d1.end(), buf + offset, buf + offset + meta.resp_d1_bytes);
            offset += meta.resp_d1_bytes;
        }
    }
};







