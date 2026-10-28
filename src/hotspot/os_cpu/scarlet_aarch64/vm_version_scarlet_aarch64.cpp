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
#include "runtime/os.hpp"
#include "runtime/vm_version.hpp"
#include "runtime/globals_extension.hpp"
void VM_Version::get_os_cpu_info() {
  // Baseline AArch64 FP/Advanced SIMD, explicitly enabled by Scarlet's kernel.
  // Optional instructions stay disabled until the OS exposes CPU capabilities.
  _features = CPU_FP | CPU_ASIMD;
  uint64_t ctr;
  asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
  _icache_line_size = 4 << (ctr & 15);
  _dcache_line_size = 4 << ((ctr >> 16) & 15);
  FLAG_SET_DEFAULT(UseBlockZeroing, false);
  _zva_length = -1; // HotSpot's sentinel for unavailable DC ZVA.
}
int VM_Version::get_current_sve_vector_length() { return -1; }
int VM_Version::set_and_get_current_sve_vector_length(int) { return -1; }

void VM_Version::get_compatible_board(char* buf, int len) { os::snprintf_checked(buf, len, "Scarlet (baseline ISA)"); }
