/*
 * Copyright (c) 1999, 2023, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/vmSymbols.hpp"
#include "code/vtableStubs.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/disassembler.hpp"
#include "interpreter/interpreter.hpp"
#include "jvm.h"
#include <dlfcn.h>
#include "jvmtifiles/jvmti.h"
#include "logging/log.hpp"
#include "logging/logStream.hpp"
#include "memory/allocation.inline.hpp"
#include "oops/oop.inline.hpp"
#include "os_scarlet.inline.hpp"
#include "os_posix.inline.hpp"
#include "prims/jniFastGetField.hpp"
#include "prims/jvm_misc.hpp"
#include "runtime/arguments.hpp"
#include "runtime/atomic.hpp"
#include "runtime/globals.hpp"
#include "runtime/globals_extension.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/osInfo.hpp"
#include "runtime/osThread.hpp"
#include "runtime/perfMemory.hpp"
#include "runtime/semaphore.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/threads.hpp"
#include "runtime/timer.hpp"
#include "services/attachListener.hpp"
#include "nmt/memTracker.hpp"
#include "services/runtimeService.hpp"

#include "utilities/align.hpp"
#include "utilities/decoder.hpp"
#include "utilities/defaultStream.hpp"
#include "utilities/events.hpp"
#include "utilities/growableArray.hpp"
#include "utilities/vmError.hpp"
#if INCLUDE_JFR
#include "jfr/jfrEvents.hpp"
#endif



#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#define MAX_PATH JVM_MAXPATHLEN


static void *thread_native_entry(Thread *thread) {

  thread->record_stack_base_and_size();
  thread->initialize_thread_current();

  OSThread* osthread = thread->osthread();
  Monitor* sync = osthread->startThread_lock();

  osthread->set_thread_id(os::current_thread_id());

#ifdef __APPLE__
  // Store unique OS X thread id used by SA
  osthread->set_unique_thread_id();
#endif

  // initialize signal mask for this thread


  // initialize floating point control register
  os::setup_fpu();
#ifndef ZERO
  guarantee(os::Scarlet::install_fault_handler(), "Cannot install Scarlet native fault handler");
#endif

#ifdef __APPLE__
  // register thread with objc gc
  if (objc_registerThreadWithCollectorFunction != nullptr) {
    objc_registerThreadWithCollectorFunction();
  }
#endif

  // handshaking with parent thread
  {
    MutexLocker ml(sync, Mutex::_no_safepoint_check_flag);

    // notify parent thread
    osthread->set_state(INITIALIZED);
    sync->notify_all();

    // wait until os::start_thread()
    while (osthread->get_state() == INITIALIZED) {
      sync->wait_without_safepoint_check();
    }
  }

  log_info(os, thread)("Thread is alive (tid: " "%zu" ", pthread id: " "%zu" ").",
    os::current_thread_id(), (uintx) pthread_self());

  // call one more level start routine
  thread->call_run();

  // Note: at this point the thread object may already have deleted itself.
  // Prevent dereferencing it from here on out.
  thread = nullptr;

  log_info(os, thread)("Thread finished (tid: " "%zu" ", pthread id: " "%zu" ").",
    os::current_thread_id(), (uintx) pthread_self());

  return 0;
}

bool os::create_thread(Thread* thread, ThreadType thr_type,
                       size_t req_stack_size) {
  assert(thread->osthread() == nullptr, "caller responsible");

  // Allocate the OSThread object
  OSThread* osthread = new (std::nothrow) OSThread();
  if (osthread == nullptr) {
    return false;
  }

  // set the correct thread state
  osthread->set_thread_type(thr_type);

  // Initial state is ALLOCATED but not INITIALIZED
  osthread->set_state(ALLOCATED);

  thread->set_osthread(osthread);

  // init thread attributes
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  // calculate stack size if it's not specified by caller
  size_t stack_size = os::Posix::get_initial_stack_size(thr_type, req_stack_size);
  int status = pthread_attr_setstacksize(&attr, stack_size);
  assert_status(status == 0, status, "pthread_attr_setstacksize");

  ThreadState state;

  {

    ResourceMark rm;
    pthread_t tid;
    int ret = 0;
    int limit = 3;
    do {
      ret = pthread_create(&tid, &attr, (void* (*)(void*)) thread_native_entry, thread);
    } while (ret == EAGAIN && limit-- > 0);

    char buf[64];
    if (ret == 0) {
      log_info(os, thread)("Thread \"%s\" started (pthread id: " "%zu" ", attributes: %s). ",
                           thread->name(), (uintx) tid, os::Posix::describe_pthread_attr(buf, sizeof(buf), &attr));
    } else {
      log_warning(os, thread)("Failed to start thread \"%s\" - pthread_create failed (%s) for attributes: %s.",
                              thread->name(), os::errno_name(ret), os::Posix::describe_pthread_attr(buf, sizeof(buf), &attr));
      // Log some OS information which might explain why creating the thread failed.
      log_info(os, thread)("Number of threads approx. running in the VM: %d", Threads::number_of_threads());
      LogStream st(Log(os, thread)::info());
      os::Posix::print_rlimit_info(&st);
      os::print_memory_info(&st);
    }

    pthread_attr_destroy(&attr);

    if (ret != 0) {
      // Need to clean up stuff we've allocated so far
      thread->set_osthread(nullptr);
      delete osthread;
      return false;
    }

    // Store pthread info into the OSThread
    osthread->set_pthread_id(tid);

    // Wait until child thread is either initialized or aborted
    {
      Monitor* sync_with_child = osthread->startThread_lock();
      MutexLocker ml(sync_with_child, Mutex::_no_safepoint_check_flag);
      while ((state = osthread->get_state()) == ALLOCATED) {
        sync_with_child->wait_without_safepoint_check();
      }
    }

  }

  // The thread is returned suspended (in state INITIALIZED),
  // and is started higher up in the call chain
  assert(state == INITIALIZED, "race condition");
  return true;
}

bool os::create_attached_thread(JavaThread* thread) {
#ifdef ASSERT
  thread->verify_not_published();
#endif

  // Allocate the OSThread object
  OSThread* osthread = new (std::nothrow) OSThread();

  if (osthread == nullptr) {
    return false;
  }

  osthread->set_thread_id(os::current_thread_id());

#ifdef __APPLE__
  // Store unique OS X thread id used by SA
  osthread->set_unique_thread_id();
#endif

  // Store pthread info into the OSThread
  osthread->set_pthread_id(::pthread_self());

  // initialize floating point control register
  os::setup_fpu();
#ifndef ZERO
  guarantee(os::Scarlet::install_fault_handler(), "Cannot install Scarlet native fault handler");
#endif

  // Initial thread state is RUNNABLE
  osthread->set_state(RUNNABLE);

  thread->set_osthread(osthread);

  // initialize signal mask for this thread
  // and save the caller's signal mask


  log_info(os, thread)("Thread attached (tid: " "%zu" ", pthread id: " "%zu"
                       ", stack: " PTR_FORMAT " - " PTR_FORMAT " (" "%zu" "K) ).",
                       os::current_thread_id(), (uintx) pthread_self(),
                       p2i(thread->stack_base()), p2i(thread->stack_end()), thread->stack_size() / K);
  return true;
}

void os::pd_start_thread(Thread* thread) {
  OSThread * osthread = thread->osthread();
  assert(osthread->get_state() != INITIALIZED, "just checking");
  Monitor* sync_with_child = osthread->startThread_lock();
  MutexLocker ml(sync_with_child, Mutex::_no_safepoint_check_flag);
  sync_with_child->notify();
}

void os::numa_make_global(char *addr, size_t bytes) {
}

void os::numa_make_local(char *addr, size_t bytes, int lgrp_hint) {
}

bool os::numa_topology_changed()   { return false; }

size_t os::numa_get_groups_num() {
  return 1;
}

int os::numa_get_group_id() {
  return 0;
}

size_t os::numa_get_leaf_groups(uint *ids, size_t size) {
  if (size > 0) {
    ids[0] = 0;
    return 1;
  }
  return 0;
}

int os::numa_get_group_id_for_address(const void* address) {
  return 0;
}

bool os::numa_get_group_ids_for_range(const void** addresses, int* lgrp_ids, size_t count) {
  return false;
}



void os::large_page_init() {
}

char* os::pd_reserve_memory_special(size_t bytes, size_t alignment, size_t page_size, char* req_addr, bool exec) {
  fatal("os::reserve_memory_special should not be called on BSD.");
  return nullptr;
}

bool os::pd_release_memory_special(char* base, size_t bytes) {
  fatal("os::release_memory_special should not be called on BSD.");
  return false;
}

bool os::can_commit_large_page_memory() {
  // Does not matter, we do not support huge pages.
  return false;
}



void os::os_exception_wrapper(java_call_t f, JavaValue* value,
                              const methodHandle& method, JavaCallArguments* args,
                              JavaThread* thread) {
  f(value, method, args, thread);
}

int os::open(const char *path, int oflag, int mode) {
  if (strlen(path) > MAX_PATH - 1) {
    errno = ENAMETOOLONG;
    return -1;
  }

  // All file descriptors that are opened in the JVM and not
  // specifically destined for a subprocess should have the
  // close-on-exec flag set.  If we don't set it, then careless 3rd
  // party native code might fork and exec without closing all
  // appropriate file descriptors (e.g. as we do in closeDescriptors in
  // UNIXProcess.c), and this in turn might:
  //
  // - cause end-of-file to fail to be detected on some file
  //   descriptors, resulting in mysterious hangs, or
  //
  // - might cause an fopen in the subprocess to fail on a system
  //   suffering from bug 1085341.
  //
  // (Yes, the default setting of the close-on-exec flag is a Unix
  // design flaw)
  //
  // See:
  // 1085341: 32-bit stdio routines should support file descriptors >255
  // 4843136: (process) pipe file descriptor from Runtime.exec not being closed
  // 6339493: (process) Runtime.exec does not close all file descriptors on Solaris 9
  //

  int fd = ::open(path, oflag | O_CLOEXEC, mode);
  if (fd == -1) return -1;

  // If the open succeeded, the file might still be a directory
  {
    struct stat buf;
    int ret = ::fstat(fd, &buf);
    int st_mode = buf.st_mode;

    if (ret != -1) {
      if ((st_mode & S_IFMT) == S_IFDIR) {
        errno = EISDIR;
        ::close(fd);
        return -1;
      }
    } else {
      ::close(fd);
      return -1;
    }
  }

  return fd;
}



jlong os::current_file_offset(int fd) {
  return (jlong)::lseek(fd, (off_t)0, SEEK_CUR);
}

jlong os::seek_to_file_offset(int fd, jlong offset) {
  return (jlong)::lseek(fd, (off_t)offset, SEEK_SET);
}

char* os::pd_map_memory(int fd, const char* file_name, size_t file_offset,
                        char *addr, size_t bytes, bool read_only,
                        bool allow_exec) {
  int prot;
  int flags;

  if (read_only) {
    prot = PROT_READ;
    flags = MAP_SHARED;
  } else {
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_PRIVATE;
  }

  if (allow_exec) {
    prot |= PROT_EXEC;
  }

  if (addr != nullptr) {
    flags |= MAP_FIXED;
  }

  char* mapped_address = (char*)mmap(addr, (size_t)bytes, prot, flags,
                                     fd, file_offset);
  if (mapped_address == MAP_FAILED) {
    return nullptr;
  }
  return mapped_address;
}



bool os::pd_unmap_memory(char* addr, size_t bytes) {
  return munmap(addr, bytes) == 0;
}

void os::current_thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = UINT64_MAX;       // will not wrap in less than 64 bits
  info_ptr->may_skip_backward = false;     // elapsed time not wall time
  info_ptr->may_skip_forward = false;      // elapsed time not wall time
  info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;  // user+system time is returned
}

void os::thread_cpu_time_info(jvmtiTimerInfo *info_ptr) {
  info_ptr->max_value = UINT64_MAX;       // will not wrap in less than 64 bits
  info_ptr->may_skip_backward = false;     // elapsed time not wall time
  info_ptr->may_skip_forward = false;      // elapsed time not wall time
  info_ptr->kind = JVMTI_TIMER_TOTAL_CPU;  // user+system time is returned
}

bool os::supports_map_sync() {
  return false;
}

void os::print_memory_mappings(char* addr, size_t bytes, outputStream* st) {}

struct MemoryInfo { uint64_t page_size, total_pages, free_pages; };
static bool memory_info(MemoryInfo* info) {
  return os::Scarlet::syscall(53, (uintptr_t)info, sizeof(*info)) == sizeof(*info)
      && info->page_size == 4096 && info->free_pages <= info->total_pages
      && info->total_pages <= UINT64_MAX / info->page_size;
}
physical_memory_size_type os::physical_memory() {
  MemoryInfo info;
  guarantee(memory_info(&info), "Scarlet GetMemoryInfo is required");
  return info.total_pages * info.page_size;
}
bool os::available_memory(physical_memory_size_type& value) { return free_memory(value); }
bool os::free_memory(physical_memory_size_type& value) {
  MemoryInfo info;
  if (!memory_info(&info)) return false;
  value = info.free_pages * info.page_size;
  return true;
}
void os::init() {
  OSInfo::set_vm_page_size(4096);
  OSInfo::set_vm_allocation_granularity(4096);
  _page_sizes.add(4096);
  long count = sysconf(_SC_NPROCESSORS_ONLN);
  guarantee(count > 0 && count <= INT_MAX, "Scarlet CPU count unavailable");
  _processor_count = count;
  Posix::init();
}
jint os::init_2() {
  DEBUG_ONLY(os::set_mutex_init_done();)
  Posix::init_2();
  if ((!FLAG_IS_DEFAULT(ReduceSignalUsage) && !ReduceSignalUsage) ||
      UseNUMA || UseNUMAInterleaving || UseLargePages || UseThreadPriorities ||
      (!FLAG_IS_DEFAULT(UsePerfData) && UsePerfData)) {
    vm_exit_during_initialization("Unsupported Scarlet VM option: signals, NUMA, large pages, thread priorities or PerfData");
    return JNI_ERR;
  }
  FLAG_SET_ERGO(ReduceSignalUsage, true);
  FLAG_SET_DEFAULT(UsePerfData, false);
#ifndef ZERO
  if ((!FLAG_IS_DEFAULT(VMContinuations) && VMContinuations) ||
      (!FLAG_IS_DEFAULT(StackReservedPages) && StackReservedPages != 0)) {
    vm_exit_during_initialization("Scarlet JIT currently requires VMContinuations=false and StackReservedPages=0");
    return JNI_ERR;
  }
  FLAG_SET_DEFAULT(VMContinuations, false);
  FLAG_SET_DEFAULT(StackReservedPages, 0);
#endif
  return set_minimum_stack_sizes();
}
int os::active_processor_count() {
  return ActiveProcessorCount > 0 ? ActiveProcessorCount : _processor_count;
}
uint os::processor_id() { return 0; }
intx os::current_thread_id() { return (intx)Scarlet::syscall(7); }
int os::current_process_id() { return getpid(); }
void os::set_native_thread_name(const char*) {}
int os::java_to_os_priority[CriticalPriority + 1] = { 0 };
OSReturn os::set_native_priority(Thread*, int) { return OS_ERR; }
OSReturn os::get_native_priority(const Thread* const, int* priority) { *priority = 0; return OS_ERR; }
bool os::create_main_thread(JavaThread* thread) { return create_attached_thread(thread); }
void os::free_thread(OSThread* thread) { delete thread; }

static const char* java_home() {
  const char* value = getenv("JAVA_HOME");
  return value && value[0] ? value : "/usr/lib/jvm/openjdk-25";
}
void os::init_system_properties_values() {
  Arguments::set_java_home(java_home());
  char path[JVM_MAXPATHLEN];
  jio_snprintf(path, sizeof(path), "%s/lib", java_home());
  Arguments::set_dll_dir(path);
  Arguments::set_library_path("/lib:/usr/lib");
  if (!set_boot_path('/', ':')) vm_exit_during_initialization("Failed setting boot class path");
}
void os::jvm_path(char* buf, jint len) { jio_snprintf(buf, len, "%s/lib/" ZERO_ONLY("zero") NOT_ZERO("server") "/libjvm.a", java_home()); }
const char* os::get_temp_directory() { return "/tmp"; }

char* os::pd_reserve_memory(size_t size, bool exec) {
  void* p = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return p == MAP_FAILED ? nullptr : (char*)p;
}
char* os::pd_attempt_reserve_memory_at(char* addr, size_t size, bool exec) {
  // MAP_FIXED would destroy an unrelated mapping. Use a hint and check it.
  void* p = mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (p == MAP_FAILED) return nullptr;
  if (p != addr) { munmap(p, size); return nullptr; }
  return (char*)p;
}
bool os::pd_release_memory(char* addr, size_t size) { return munmap(addr, size) == 0; }
bool os::pd_commit_memory(char* addr, size_t size, bool exec) {
  return mprotect(addr, size, PROT_READ | PROT_WRITE | (exec ? PROT_EXEC : 0)) == 0;
}
bool os::pd_commit_memory(char* addr, size_t size, size_t alignment, bool exec) {
  return pd_commit_memory(addr, size, exec);
}
void os::pd_commit_memory_or_exit(char* addr, size_t size, bool exec, const char* msg) {
  if (!pd_commit_memory(addr, size, exec)) vm_exit_out_of_memory(size, OOM_MMAP_ERROR, "%s", msg);
}
void os::pd_commit_memory_or_exit(char* addr, size_t size, size_t alignment, bool exec, const char* msg) {
  pd_commit_memory_or_exit(addr, size, exec, msg);
}
bool os::pd_uncommit_memory(char* addr, size_t size, bool exec) {
  // Replace our owned reservation to discard its pages and obtain zeroes on
  // the next commit. The VM serializes access to its reserved ranges.
  return mmap(addr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == addr;
}
void os::pd_realign_memory(char*, size_t, size_t) {}

size_t os::pd_pretouch_memory(void*, void*, size_t) { return 0; }
bool os::pd_create_stack_guard_pages(char*, size_t) { return false; }
bool os::remove_stack_guard_pages(char*, size_t) { return false; }
bool os::protect_memory(char* addr, size_t size, ProtType prot, bool committed) {
  int flags = prot == MEM_PROT_NONE ? PROT_NONE : prot == MEM_PROT_READ ? PROT_READ :
              prot == MEM_PROT_RW ? PROT_READ | PROT_WRITE : PROT_READ | PROT_WRITE | PROT_EXEC;
  return mprotect(addr, size, flags) == 0;
}
bool os::guard_memory(char* addr, size_t size) { return mprotect(addr, size, PROT_NONE) == 0; }
bool os::unguard_memory(char* addr, size_t size) { return mprotect(addr, size, PROT_READ | PROT_WRITE) == 0; }
size_t os::large_page_size() { return 0; }
char* os::pd_attempt_map_memory_to_file_at(char*, size_t, int) { return nullptr; }

// Static built-in JNI libraries are resolved from the executable's symbol table.
extern "C" void* scarlet_jvm_find_builtin(const char* name);
void* os::dll_load(const char* path, char* error, int len) {
  const char* name = strrchr(path, '/');
  name = name ? name + 1 : path;
  const struct { const char* library; const char* entry; } builtins[] = {
    { "libjimage.so", "JIMAGE_Open" }, { "libzip.so", "ZIP_Open" },
    { "libjava.so", "JNI_OnLoad_java" }, { "libverify.so", "VerifyClassCodes" },
    { "libnio.so", "JNI_OnLoad_nio" }, { "libnet.so", "JNI_OnLoad_net" },
    { "libmanagement.so", "JNI_OnLoad_management" }
  };
  for (const auto& builtin : builtins) {
    if (strcmp(name, builtin.library) == 0 && scarlet_jvm_find_builtin(builtin.entry))
      return get_default_process_handle();
  }
  void* handle = ::dlopen(path, RTLD_NOW | RTLD_GLOBAL);
  if (handle == nullptr) {
    const char* reason = ::dlerror();
    jio_snprintf(error, len, "%s", reason != nullptr ? reason : "Cannot load JNI library");
  }
  return handle;
}
bool os::pd_dll_unload(void* handle, char* error, int len) {
  if (handle == get_default_process_handle()) return true;
  if (::dlclose(handle) == 0) return true;
  const char* reason = ::dlerror();
  jio_snprintf(error, len, "%s", reason != nullptr ? reason : "Cannot close JNI library");
  return false;
}
bool os::address_is_in_vm(address) { return false; }
void os::prepare_native_symbols() {}
bool os::dll_address_to_function_name(address, char*, int, int*, bool) { return false; }
bool os::dll_address_to_library_name(address, char*, int, int*) { return false; }
void os::print_dll_info(outputStream* st) { st->print_cr("OpenJDK on Scarlet with scarlet-ld JNI loading"); }
int os::get_loaded_modules_info(LoadedModulesCallbackFunc, void*) { return -1; }
bool os::find(address, outputStream*) { return false; }
void os::get_summary_os_info(char* buf, size_t len) { jio_snprintf(buf, len, "Scarlet"); }
void os::print_os_info_brief(outputStream* st) { st->print_cr("Scarlet (native ABI)"); }
void os::print_os_info(outputStream* st) { print_os_info_brief(st); }
void os::get_summary_cpu_info(char* buf, size_t len) { jio_snprintf(buf, len, "AArch64"); }
void os::pd_print_cpu_info(outputStream* st, char*, size_t) { st->print_cr("AArch64"); }
void os::print_memory_info(outputStream* st) {
  physical_memory_size_type free = 0;
  if (!free_memory(free)) free = 0;
  st->print_cr("Memory: " UINT64_FORMAT " total bytes, " UINT64_FORMAT " free bytes", physical_memory(), free);
}
bool os::start_debugging(char*, int) { return false; }
int os::get_core_path(char*, size_t) { return -1; }
int os::loadavg(double[], int) { return -1; }
double os::elapsedVTime() { return 0; }
jlong os::current_thread_cpu_time() { return -1; }
jlong os::current_thread_cpu_time(bool) { return -1; }
jlong os::thread_cpu_time(Thread*) { return -1; }
jlong os::thread_cpu_time(Thread*, bool) { return -1; }
bool os::is_thread_cpu_time_supported() { return false; }
int os::compare_file_modified_times(const char* a, const char* b) {
  struct stat x, y;
  if (stat(a, &x) != 0 || stat(b, &y) != 0) return 0;
  if (x.st_mtim.tv_sec != y.st_mtim.tv_sec) return x.st_mtim.tv_sec < y.st_mtim.tv_sec ? -1 : 1;
  return (x.st_mtim.tv_nsec > y.st_mtim.tv_nsec) - (x.st_mtim.tv_nsec < y.st_mtim.tv_nsec);
}

size_t os::_compiler_thread_min_stack_allowed = 64 * K;
size_t os::_java_thread_min_stack_allowed = 64 * K;
size_t os::_vm_internal_thread_min_stack_allowed = 64 * K;
size_t os::Posix::default_stack_size(ThreadType) { return 1 * M; }


#ifdef ZERO
address os::current_stack_pointer() { uintptr_t sp; asm volatile("mov %0, sp" : "=r"(sp)); return (address)sp; }
frame os::current_frame() { frame fr; fr.set_sp((intptr_t*)current_stack_pointer()); return fr; }
frame os::get_sender_for_C_frame(frame*) { return frame(); }
char* os::non_memory_address_word() { return (char*)-1; }
#endif
#ifdef ZERO
address os::fetch_frame_from_context(const void*, intptr_t**, intptr_t**) { return nullptr; }
frame os::fetch_frame_from_context(const void*) { return frame(); }
address os::Posix::ucontext_get_pc(const ucontext_t*) { return nullptr; }
void os::Posix::ucontext_set_pc(ucontext_t*, address) { ShouldNotReachHere(); }

#endif // ZERO: architecture context access lives in os_cpu for JIT

void os::current_stack_base_and_size(address* base, size_t* size) {
  address bottom;
  Scarlet::stack_region(&bottom, size);
  *base = bottom + *size;
}

// Use HotSpot's conservative lower bound, as on other POSIX ports without
// a platform query. Reservations below it are not attempted by the VM.
size_t os::vm_min_address() { return _vm_min_address_default; }

// The native ABI has no process-residency/mincore query. These diagnostic
// APIs report unavailable, rather than treating reserved pages as resident.
size_t os::rss() { return 0; }
bool os::committed_in_range(address, size_t, address& start, size_t& size) {
  start = nullptr;
  size = 0;
  return false;
}

// Disclaim is an optional reclamation hint; retain the committed mapping
// until uncommit/release because Scarlet has no MADV_DONTNEED equivalent.
void os::pd_disclaim_memory(char*, size_t) {}
