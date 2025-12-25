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
#define SEROP_HALT          (1)     //  OP[8]       ID[8]                   -> ACK[8]
#define SEROP_ITR           (2)     //  OP[8]       ID[8]                   -> ACK[8]
#define SEROP_MMU           (3)     //  OP[8]       ID[8] ASID[16] PT[40]   -> ACK[8]
#define SEROP_REDIR         (4)     //  OP[8]       ID[8] PC[48]            -> ACK[8]
#define SEROP_FTLB          (5)     //  OP[8]       ID[8]                   -> ACK[8]
#define SEROP_FTLB2         (6)     //  OP[8]       ID[8] ASID[16] VPN[40]  -> ACK[8]
#define SEROP_SYNCI         (7)     //  OP[8]       ID[8]                   -> ACK[8]
#define SEROP_REGRD         (8)     //  OP[8]       ID[8] REG[16]           -> ACK[8] DATA[64]
#define SEROP_REGWT         (9)     //  OP[8]       ID[8] REG[16] DATA[64]  -> ACK[8]
#define SEROP_MEMRD         (10)    //  OP[8]       ID[8] PA[48]            -> ACK[8] DATA[64]
#define SEROP_MEMWT         (11)    //  OP[8]       ID[8] PA[48] DATA[64]   -> ACK[8]
#define SEROP_PGRD          (12)    //  OP[8] ID[8] PPN[40]                 -> ACK[8] DATA[4096B]
#define SEROP_PGST          (13)    //  OP[8]       ID[8] PPN[40] VALUE[64] -> ACK[8]
#define SEROP_PGWT          (14)    //  OP[8] ID[8] PPN[40] DATA[4096B]     -> ACK[8]
#define SEROP_PGCP          (15)    //  OP[8]       ID[8] DST[40] SRC[40]   -> ACK[8]
#define SEROP_CLK           (16)    //  OP[8]                               -> ACK[8] CLK[64]
#define SEROP_UCLK          (17)    //  OP[8]       ID[8]                   -> ACK[8] CLK[64]
#define SEROP_HFSET         (18)    //  OP[8]       ID[8] VA[48]            -> ACK[8]
#define SEROP_HFCLR         (19)    //  OP[8]       ID[8]                   -> ACK[8]
#define SEROP_PGZERO        (20)    //  OP[8]       ID[8] PPN[40]           -> ACK[8]
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
};

constexpr uint32_t CPUID_ALLHALT = 0xff;

class HTPFrame {
public:
    HTPFrame() {};
    HTPFrame(const HTOP op, const uint32_t cid) : opcode(op), cpuid(cid) {};
    HTPFrame(const HTOP op, const uint32_t cid, const uint64_t a1) : opcode(op), cpuid(cid), x1(a1) {};
    HTPFrame(const HTOP op, const uint32_t cid, const uint64_t a1, const uint64_t a2) : opcode(op), cpuid(cid), x1(a1), x2(a2) {};
    HTPFrame(const HTOP op, const uint32_t cid, const uint64_t a1, const uint64_t a2, const uint64_t a3) : opcode(op), cpuid(cid), x1(a1), x2(a2), x3(a3) {};
    HTPFrame(const HTOP op, const uint32_t cid, const uint64_t a1, const vector<uint8_t> &data) : opcode(op), cpuid(cid), x1(a1), d1(data) {};  
    HTPFrame(const HTOP op, const uint32_t cid, const uint64_t a1, const uint8_t *data, const uint64_t size) : opcode(op), cpuid(cid), x1(a1), d1(data, data + size) {};
    HTOP        opcode = HTOP::next;
    uint32_t    cpuid = 0;
    uint64_t    x1 = 0;
    uint64_t    x2 = 0;
    uint64_t    x3 = 0;
    vector<uint8_t> d1;
};

typedef struct {
    bool has_id;
    uint32_t x1_bytes;
    uint32_t x2_bytes;
    uint32_t x3_bytes;
    uint32_t d1_bytes;
    bool has_ret_id;
    uint32_t ret_x1_bytes;
    uint32_t ret_x2_bytes;
    uint32_t ret_x3_bytes;
    uint32_t ret_d1_bytes;
} HTOPMeta;

static constexpr std::array<HTOPMeta, SEROP_NUM> HTOP_DEFAULT_METAS = {{
    { false,    0, 0, 0, 0,     true,   1, 6, 6, 0 },          // SEROP_NEXT
    { true,     0, 0, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_HALT
    { true,     0, 0, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_ITR
    { true,     2, 5, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_MMU
    { true,     6, 0, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_REDIR
    { true,     0, 0, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_FTLB
    { true,     2, 5, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_FTLB2
    { true,     0, 0, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_SYNCI
    { true,     2, 0, 0, 0,     false,  8, 0, 0, 0 },          // SEROP_REGRD
    { true,     2, 8, 0, 0,     false,  0, 0, 0, 0 },          // SEROP_REGWT
    { true,     6, 0, 0, 0,     false,  8, 0, 0 ,0 },          // SEROP_MEMRD
    { true,     6, 8 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_MEMWT
    { true ,    5 ,0 ,0 ,0 ,    false , 0 ,0 ,0 ,4096 },       // SEROP_PGRD
    { true ,    5 ,8 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_PGST
    { true ,    5 ,0 ,0 ,4096 , false , 0 ,0 ,0 ,0 },          // SEROP_PGWT
    { true ,    5 ,5 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_PGCP
    { false ,   0 ,0 ,0 ,0 ,    false , 8 ,0 ,0 ,0 },          // SEROP_CLK
    { true ,    0 ,0 ,0 ,0 ,    false , 8 ,0 ,0 ,0 },          // SEROP_UCLK
    { true ,    6 ,0 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_HFSET
    { true ,    0 ,0 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_HFCLR
    { true ,    5 ,0 ,0 ,0 ,    false , 0 ,0 ,0 ,0 },          // SEROP_PGZERO
}};





