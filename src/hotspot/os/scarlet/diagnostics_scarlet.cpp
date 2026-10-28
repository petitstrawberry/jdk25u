/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#include "jvm.h"
#include "memory/allocation.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/os.hpp"
#include "os_scarlet.hpp"
#include "runtime/perfMemory.hpp"
#include "runtime/suspendedThreadTask.hpp"
#include "utilities/vmError.hpp"

// No asynchronous signal or stack-sampling API is exposed by this port.
// Return explicit failure for optional operations; never report installed
// handlers or recovered signals. SafeFetch uses a guarded kernel scalar load.
JVM_ENTRY_NO_ENV(void*, JVM_RegisterSignal(jint sig, void* handler))
  return (void*)-1;
JVM_END
JVM_ENTRY_NO_ENV(jboolean, JVM_RaiseSignal(jint sig))
  return JNI_FALSE;
JVM_END
void jdk_misc_signal_init() { ShouldNotReachHere(); }
void os::signal_notify(int) { ShouldNotReachHere(); }
int os::signal_wait() { ShouldNotReachHere(); return -1; }
int os::sigexitnum_pd() { return -1; }
bool os::signal_thread(Thread*, int, const char*) { return false; }
void os::run_periodic_checks(outputStream*) {}
bool os::signal_sent_by_kill(const void*) { return false; }
void os::print_siginfo(outputStream* st, const void*) { st->print_cr("Native signal information unavailable"); }
void os::print_signal_handlers(outputStream* st, char*, size_t) { st->print_cr("No POSIX signal handlers (Scarlet native)"); }
const char* os::exception_name(int, char*, size_t) { return nullptr; }
int os::get_signal_number(const char*) { return -1; }
void SuspendedThreadTask::internal_do_task() { ShouldNotReachHere(); }

int SafeFetch32_impl(int* p, int err_value) {
  return (int)os::Scarlet::syscall(704, (uintptr_t)p, 4, (uintptr_t)err_value);
}
intptr_t SafeFetchN_impl(intptr_t* p, intptr_t err_value) {
  return (intptr_t)os::Scarlet::syscall(704, (uintptr_t)p, sizeof(*p), (uintptr_t)err_value);
}
bool handle_safefetch(int, address, void*) { return false; }

const void* VMError::crash_handler_address = nullptr;
void VMError::reporting_started() {}
void VMError::interrupt_reporting_thread() {}
void VMError::install_secondary_signal_handler() {}
void VMError::check_failing_cds_access(outputStream*, const void*) {}

void PerfMemory::create_memory_region(size_t) { guarantee(!UsePerfData, "PerfData is unsupported on Scarlet"); }
void PerfMemory::delete_memory_region() {}
void PerfMemory::attach(int, char**, size_t*, TRAPS) {
  THROW_MSG(vmSymbols::java_lang_UnsupportedOperationException(), "PerfData is unsupported on Scarlet");
}
void PerfMemory::detach(char*, size_t) {}
