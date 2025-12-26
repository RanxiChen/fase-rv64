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

#include "udsocket.h"
#include "simroot.h"
#include "configuration.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

constexpr bool EnableDebugLog = false;

UDSocketAdapter::UDSocketAdapter(const string sockfile) {
    int32_t sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    simroot_assertf(sockfd >= 0, "UDSocketAdapter: socket creation failed on file %s: %s", sockfile.c_str(), strerror(errno));

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockfile.c_str(), sizeof(addr.sun_path) - 1);

    simroot_assertf(connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == 0,
        "UDSocketAdapter: socket connect failed on file %s: %s", sockfile.c_str(), strerror(errno));
    
    fd = sockfd;
}

UDSocketAdapter::~UDSocketAdapter() {
    if(fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void UDSocketAdapter::process_frames(HTPFrames &frames) {
    uint64_t total_send_bytes = 0, total_recv_bytes = 0;
    for (auto & frame : frames) {
        uint8_t op = static_cast<uint8_t>(frame.opcode);
        simroot_assertf(op < SEROP_NUM, "UDSocketAdapter: Invalid HTOP opcode %d", op);
        auto & meta = HTOP_DEFAULT_METAS[op];
        total_send_bytes += (meta.has_id ? 1 : 0)
            + meta.x1_bytes + meta.x2_bytes + meta.x3_bytes + meta.d1_bytes + 1; // +1 for opcode
        total_recv_bytes += (meta.has_ret_id ? 1 : 0)
            + meta.ret_x1_bytes + meta.ret_x2_bytes + meta.ret_x3_bytes + meta.ret_d1_bytes + 1; // +1 for opcode
    }
    vector<uint8_t> send_buf;
    send_buf.resize(total_send_bytes);
    uint64_t send_offset = 0;
    for (auto & frame : frames) {
        uint8_t op = static_cast<uint8_t>(frame.opcode);
        auto & meta = HTOP_DEFAULT_METAS[op];
        send_buf[send_offset++] = op;
        if (meta.has_id) {
            send_buf[send_offset++] = static_cast<uint8_t>(frame.cpuid);
        }
        if (meta.x1_bytes > 0) {
            memcpy(&send_buf[send_offset], &frame.x1, meta.x1_bytes);
            send_offset += meta.x1_bytes;
        }
        if (meta.x2_bytes > 0) {
            memcpy(&send_buf[send_offset], &frame.x2, meta.x2_bytes);
            send_offset += meta.x2_bytes;
        }
        if (meta.x3_bytes > 0) {
            memcpy(&send_buf[send_offset], &frame.x3, meta.x3_bytes);
            send_offset += meta.x3_bytes;
        }
        if (meta.d1_bytes > 0) {
            simroot_assertf(frame.d1.size() == meta.d1_bytes,
                "UDSocketAdapter: HTOP opcode %d d1 size mismatch: expected %d, got %ld",
                op, meta.d1_bytes, frame.d1.size());
            memcpy(&send_buf[send_offset], frame.d1.data(), meta.d1_bytes);
            send_offset += meta.d1_bytes;
        }
    }
    simroot_assertf(send_offset == total_send_bytes,
        "UDSocketAdapter: send bytes calculation mismatch: expected %ld, got %ld",
        total_send_bytes, send_offset);
    if (EnableDebugLog) {
        printf("UDSocketAdapter: Sending %ld bytes: ", total_send_bytes);
        for (uint64_t i = 0; i < total_send_bytes; i++) {
            printf("%02x ", send_buf[i]);
        }
        printf("\n");
    }
    send_blocked(send_buf.data(), send_buf.size());
    vector<uint8_t> recv_buf;
    recv_buf.resize(total_recv_bytes);
    recv_blocked(recv_buf.data(), recv_buf.size());
    if (EnableDebugLog) {
        printf("UDSocketAdapter: Received %ld bytes: ", total_recv_bytes);
        for (uint64_t i = 0; i < total_recv_bytes; i++) {
            printf("%02x ", recv_buf[i]);
        }
        printf("\n");
    }
    uint64_t recv_offset = 0;
    for (auto & frame : frames) {
        uint8_t op = static_cast<uint8_t>(frame.opcode);
        auto & meta = HTOP_DEFAULT_METAS[op];
        uint8_t ret_op = recv_buf[recv_offset++];
        simroot_assertf(ret_op == op, "UDSocketAdapter: HTOP opcode %d return error: got %d", op, ret_op);
        frame.cpuid = 0;
        frame.x1 = frame.x2 = frame.x3 = 0;
        frame.d1.clear();
        if (meta.has_ret_id) {
            frame.cpuid = recv_buf[recv_offset++];
        }
        if (meta.ret_x1_bytes > 0) {
            memcpy(&frame.x1, &recv_buf[recv_offset], meta.ret_x1_bytes);
            recv_offset += meta.ret_x1_bytes;
        }
        if (meta.ret_x2_bytes > 0) {
            memcpy(&frame.x2, &recv_buf[recv_offset], meta.ret_x2_bytes);
            recv_offset += meta.ret_x2_bytes;
        }
        if (meta.ret_x3_bytes > 0) {
            memcpy(&frame.x3, &recv_buf[recv_offset], meta.ret_x3_bytes);
            recv_offset += meta.ret_x3_bytes;
        }
        if (meta.ret_d1_bytes > 0) {
            frame.d1.resize(meta.ret_d1_bytes);
            memcpy(frame.d1.data(), &recv_buf[recv_offset], meta.ret_d1_bytes);
            recv_offset += meta.ret_d1_bytes;
        }
    }
}

void UDSocketAdapter::recv_blocked(uint8_t *buf, const uint64_t size) {
    uint64_t received = 0;
    while (received < size) {
        ssize_t ret = recv(fd, buf + received, size - received, MSG_WAITALL);
        simroot_assertf(ret > 0, "UDSocketAdapter: recv failed: %s", strerror(errno));
        received += ret;
    }
}
void UDSocketAdapter::send_blocked(const uint8_t *buf, const uint64_t size) {
    uint64_t sent = 0;
    while (sent < size) {
        ssize_t ret = send(fd, buf + sent, size - sent, 0);
        simroot_assertf(ret > 0, "UDSocketAdapter: send failed: %s", strerror(errno));
        sent += ret;
    }
}


