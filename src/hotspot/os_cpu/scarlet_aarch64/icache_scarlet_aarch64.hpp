/*
 * Copyright (c) 1999, 2023, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2014, Red Hat Inc. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

// no precompiled headers
#ifndef OS_CPU_SCARLET_AARCH64_ICACHE_HPP
#define OS_CPU_SCARLET_AARCH64_ICACHE_HPP
class ICache : public AbstractICache {
 public:
  static void initialize(int phase);
  static void invalidate_word(address addr) { invalidate_range(addr, 4); }
  static void invalidate_range(address start, int nbytes) {
    if (nbytes <= 0) return;
    uintptr_t ctr;
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uintptr_t dline = 4UL << ((ctr >> 16) & 15);
    uintptr_t iline = 4UL << (ctr & 15);
    uintptr_t end = (uintptr_t)start + nbytes;
    for (uintptr_t p = (uintptr_t)start & ~(dline - 1); p < end; p += dline)
      asm volatile("dc cvau, %0" :: "r"(p) : "memory");
    asm volatile("dsb ish" ::: "memory");
    for (uintptr_t p = (uintptr_t)start & ~(iline - 1); p < end; p += iline)
      asm volatile("ic ivau, %0" :: "r"(p) : "memory");
    asm volatile("dsb ish; isb" ::: "memory");
  }
};
#endif
