/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#include "runtime/os.hpp"
#include "os_scarlet.hpp"
#include "runtime/javaThread.inline.hpp"
#include "runtime/stackOverflow.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/safepointMechanism.hpp"
#include "asm/macroAssembler.hpp"
#include "code/nativeInst.hpp"
#include "prims/jniFastGetField.hpp"
#include <pthread.h>
#include <sys/mman.h>

static const size_t fault_stack_size = 128 * 1024;
static pthread_key_t fault_stack_key;
static pthread_once_t fault_stack_once = PTHREAD_ONCE_INIT;
static int fault_stack_status;

static void destroy_fault_stack(void* stack) {
  os::Scarlet::syscall(645, 0, 0, 0);
  munmap(stack, fault_stack_size);
}
static void create_fault_stack_key() {
  fault_stack_status = pthread_key_create(&fault_stack_key, destroy_fault_stack);
}

static void native_fault(os::Scarlet::FaultContext* c) {
  Thread* current = Thread::current_or_null();
  JavaThread* thread = current && current->is_Java_thread() ? (JavaThread*)current : nullptr;
  address pc = (address)c->pc;
  address addr = (address)c->fault_address;
  address stub = nullptr;
  bool retry = false;
  unsigned exception = (c->syndrome >> 26) & 63;
  bool memory_fault = exception == 0x20 || exception == 0x24;
  if (thread != nullptr) {
    StackOverflow* overflow = thread->stack_overflow_state();
    if (memory_fault && overflow->in_stack_yellow_reserved_zone(addr)) {
      overflow->disable_stack_yellow_reserved_zone();
      if (thread->thread_state() == _thread_in_Java) {
        stub = SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::STACK_OVERFLOW);
      } else {
        retry = true;
      }
    } else if (thread->thread_state() == _thread_in_Java) {
      if (!memory_fault && nativeInstruction_at(pc)->is_sigill_not_entrant()) {
        stub = SharedRuntime::get_handle_wrong_method_stub();
      } else if (memory_fault && SafepointMechanism::is_poll_address(addr)) {
        stub = SharedRuntime::get_poll_stub(pc);
      } else if (memory_fault && MacroAssembler::uses_implicit_null_check(addr)) {
        stub = SharedRuntime::continuation_for_implicit_exception(thread, pc, SharedRuntime::IMPLICIT_NULL);
      }
    }
    if (memory_fault && stub == nullptr && !retry) {
      address slow = JNI_FastGetField::find_slowcase_pc(pc);
      if (slow != (address)-1) stub = slow;
    }
  }
  if (stub != nullptr) {
    thread->set_saved_exception_pc(pc);
    c->pc = (uintptr_t)stub;
  } else if (!retry) {
    fatal("Unhandled Scarlet native fault: pc=" PTR_FORMAT " address=" PTR_FORMAT " syndrome=" UINT64_FORMAT,
          p2i(pc), p2i(addr), c->syndrome);
  }
  os::Scarlet::syscall(646, (uintptr_t)c);
  fatal("Scarlet native fault return failed");
}

bool os::Scarlet::install_fault_handler() {
  if (pthread_once(&fault_stack_once, create_fault_stack_key) || fault_stack_status) return false;
  if (pthread_getspecific(fault_stack_key)) return true;
  void* stack = mmap(nullptr, fault_stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (stack == MAP_FAILED) return false;
  if (pthread_setspecific(fault_stack_key, stack) != 0) {
    munmap(stack, fault_stack_size);
    return false;
  }
  if (syscall(645, (uintptr_t)native_fault, (uintptr_t)stack, fault_stack_size) != 0) {
    pthread_setspecific(fault_stack_key, nullptr);
    munmap(stack, fault_stack_size);
    return false;
  }
  return true;
}
