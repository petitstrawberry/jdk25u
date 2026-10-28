/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS_SCARLET_OSTHREAD_SCARLET_HPP
#define OS_SCARLET_OSTHREAD_SCARLET_HPP
#include "runtime/osThreadBase.hpp"
#include "utilities/globalDefinitions.hpp"

class OSThread : public OSThreadBase {
  typedef pid_t thread_id_t;
  thread_id_t _thread_id;
  pthread_t _pthread_id;
  Monitor* _startThread_lock;
  int _thread_type;
public:
  OSThread();
  ~OSThread();
  thread_id_t thread_id() const { return _thread_id; }
  void set_thread_id(thread_id_t id) { _thread_id = id; }
  uintx thread_id_for_printing() const override { return (uintx)_thread_id; }
  int thread_type() const { return _thread_type; }
  void set_thread_type(int type) { _thread_type = type; }
  pthread_t pthread_id() const { return _pthread_id; }
  void set_pthread_id(pthread_t id) { _pthread_id = id; }
  Monitor* startThread_lock() const { return _startThread_lock; }
};
#endif
