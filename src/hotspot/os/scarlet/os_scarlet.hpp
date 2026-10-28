/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS_SCARLET_OS_SCARLET_HPP
#define OS_SCARLET_OS_SCARLET_HPP
#include "runtime/os.hpp"
#include <stdint.h>
class os::Scarlet {
public:
  struct FaultContext {
    uint64_t version, size, fault_address, syndrome;
    uint64_t registers[31], sp, pc, nzcv;
  };
#ifndef ZERO
  static bool install_fault_handler();
#endif
  static uintptr_t syscall(uintptr_t number, uintptr_t a0 = 0, uintptr_t a1 = 0, uintptr_t a2 = 0) {
    register uintptr_t x0 asm("x0") = a0;
    register uintptr_t x1 asm("x1") = a1;
    register uintptr_t x2 asm("x2") = a2;
    register uintptr_t x8 asm("x8") = number;
    asm volatile("svc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x8) : : "memory", "cc", "x3", "x4", "x5", "x6", "x7", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x30", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
    return x0;
  }
  static void stack_region(address* bottom, size_t* size) {
    uintptr_t bounds[2];
    uintptr_t sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    guarantee(syscall(703, sp, (uintptr_t)bounds) == 0,
              "Scarlet MemoryQuery failed for current stack");
    guarantee(bounds[0] <= sp && sp < bounds[1], "invalid stack mapping");
    *bottom = (address)bounds[0];
    *size = bounds[1] - bounds[0];
  }
};
#endif
