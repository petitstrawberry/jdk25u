/*
 * Copyright (c) 2003, 2023, Oracle and/or its affiliates. All rights reserved.
 * Copyright 2007, 2008, 2009, 2010 Red Hat, Inc.
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

#include "runtime/os.hpp"
#include "utilities/copy.hpp"
#include "atomic_scarlet_zero.hpp"

// Stubs for things that would be in scarlet_zero.s if it existed.
// You probably want to disassemble these monkeys to check they're ok.

extern "C" {
  int SpinPause() {
    asm volatile("yield" ::: "memory");
    return 1;
  }

  void _Copy_conjoint_jshorts_atomic(const jshort* src, jshort* dst, size_t count) {
    const volatile jshort* from = src;
    volatile jshort* to = dst;
    if (from > to) {
      const volatile jshort *end = from + count;
      while (from < end)
        *(to++) = *(from++);
    }
    else if (from < to) {
      while (count != 0) { --count; to[count] = from[count]; }
    }
  }
  void _Copy_conjoint_jints_atomic(const jint* src, jint* dst, size_t count) {
    const volatile jint* from = src;
    volatile jint* to = dst;
    if (from > to) {
      const volatile jint *end = from + count;
      while (from < end)
        *(to++) = *(from++);
    }
    else if (from < to) {
      while (count != 0) { --count; to[count] = from[count]; }
    }
  }
  void _Copy_conjoint_jlongs_atomic(const jlong* from, jlong* to, size_t count) {
    if (from > to) {
      const jlong *end = from + count;
      while (from < end)
        atomic_copy64(from++, to++);
    }
    else if (from < to) {
      while (count != 0) { --count; atomic_copy64(from+count, to+count); }
    }
  }

  void _Copy_arrayof_conjoint_bytes(const HeapWord* from,
                                    HeapWord* to,
                                    size_t    count) {
    memmove(to, from, count);
  }
  void _Copy_arrayof_conjoint_jshorts(const HeapWord* from,
                                      HeapWord* to,
                                      size_t    count) {
    memmove(to, from, count * 2);
  }
  void _Copy_arrayof_conjoint_jints(const HeapWord* from,
                                    HeapWord* to,
                                    size_t    count) {
    memmove(to, from, count * 4);
  }
  void _Copy_arrayof_conjoint_jlongs(const HeapWord* from,
                                     HeapWord* to,
                                     size_t    count) {
    memmove(to, from, count * 8);
  }
};


void os::setup_fpu() {
  // Round to nearest, preserve subnormals, mask FP exceptions.
  asm volatile("msr fpcr, xzr; isb" ::: "memory");
}
void os::print_context(outputStream* st, const void*) {
  st->print_cr("Native crash context unavailable on Scarlet Zero");
}
void os::print_register_info(outputStream* st, const void*, int& continuation) {
  continuation = -1;
  st->print_cr("Native register context unavailable on Scarlet Zero");
}
