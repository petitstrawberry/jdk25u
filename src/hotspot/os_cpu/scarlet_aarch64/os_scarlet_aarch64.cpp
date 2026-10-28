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
#include "os_scarlet.hpp"
#include "os_posix.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/javaThread.hpp"
#include "code/codeCache.hpp"
#include "interpreter/interpreter.hpp"
extern "C" int SpinPause() { asm volatile("yield" ::: "memory"); return 1; }
void os::setup_fpu() { asm volatile("msr fpcr, xzr; isb" ::: "memory"); }
NOINLINE address os::current_stack_pointer() { uintptr_t sp; asm volatile("mov %0, sp" : "=r"(sp)); return (address)sp; }
char* os::non_memory_address_word() { return (char*)0xffffffffffffUL; }
frame os::get_sender_for_C_frame(frame* fr) {
  address pc = fr->sender_pc();
  CodeBlob* cb = CodeCache::find_blob(pc);
  bool use_codeblob = cb != nullptr && cb->frame_size() > 0;
  intptr_t* sender_sp = use_codeblob ? (fr->link() + frame::metadata_words - cb->frame_size()) : fr->link();
  return frame(sender_sp, sender_sp, fr->link(), pc, cb, true);
}
NOINLINE frame os::current_frame() {
  intptr_t* fp = *(intptr_t**)__builtin_frame_address(0);
  frame self((intptr_t*)os::current_stack_pointer(), fp, CAST_FROM_FN_PTR(address, os::current_frame));
  return os::is_first_C_frame(&self) ? frame() : os::get_sender_for_C_frame(&self);
}
address os::fetch_frame_from_context(const void* raw, intptr_t** sp, intptr_t** fp) {
  const Scarlet::FaultContext* c = (const Scarlet::FaultContext*)raw;
  if (sp) *sp = c ? (intptr_t*)c->sp : nullptr;
  if (fp) *fp = c ? (intptr_t*)c->registers[29] : nullptr;
  return c ? (address)c->pc : nullptr;
}
frame os::fetch_frame_from_context(const void* raw) {
  intptr_t *sp, *fp;
  address pc = fetch_frame_from_context(raw, &sp, &fp);
  return frame(sp, fp, pc);
}
frame os::fetch_compiled_frame_from_context(const void* raw) {
  const Scarlet::FaultContext* c = (const Scarlet::FaultContext*)raw;
  return frame((intptr_t*)c->sp, (intptr_t*)c->registers[29], (address)(c->registers[30] - 4));
}
address os::Posix::ucontext_get_pc(const ucontext_t* c) { return (address)((const Scarlet::FaultContext*)c)->pc; }
void os::Posix::ucontext_set_pc(ucontext_t* c, address pc) { ((Scarlet::FaultContext*)c)->pc = (uintptr_t)pc; }

void os::print_context(outputStream* st, const void*) { st->print_cr("Scarlet AArch64 native context"); }
void os::print_register_info(outputStream*, const void*, int& continuation) { continuation = -1; }

#include "utilities/copy.hpp"
#include <unistd.h>
bool os::is_primordial_thread() { return (pid_t)os::current_thread_id() == getpid(); }
int os::extra_bang_size_in_bytes() { return 0; }
template<typename T> static void conjoint_atomic(const T* src, T* dst, size_t count) {
  const volatile T* from = src;
  volatile T* to = dst;
  if ((uintptr_t)from < (uintptr_t)to) {
    while (count != 0) { --count; to[count] = from[count]; }
  } else if (from != to) {
    for (size_t i = 0; i < count; ++i) to[i] = from[i];
  }
}
extern "C" {
void _Copy_conjoint_jshorts_atomic(const jshort* s, jshort* d, size_t n) { conjoint_atomic(s,d,n); }
void _Copy_conjoint_jints_atomic(const jint* s, jint* d, size_t n) { conjoint_atomic(s,d,n); }
void _Copy_conjoint_jlongs_atomic(const jlong* s, jlong* d, size_t n) { conjoint_atomic(s,d,n); }
void _Copy_arrayof_conjoint_bytes(const HeapWord* s, HeapWord* d, size_t n) { conjoint_atomic((const jbyte*)s,(jbyte*)d,n); }
void _Copy_arrayof_conjoint_jshorts(const HeapWord* s, HeapWord* d, size_t n) { conjoint_atomic((const jshort*)s,(jshort*)d,n); }
void _Copy_arrayof_conjoint_jints(const HeapWord* s, HeapWord* d, size_t n) { conjoint_atomic((const jint*)s,(jint*)d,n); }
void _Copy_arrayof_conjoint_jlongs(const HeapWord* s, HeapWord* d, size_t n) { conjoint_atomic((const jlong*)s,(jlong*)d,n); }
}
