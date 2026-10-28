/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS_SCARLET_SEMAPHORE_SCARLET_HPP
#define OS_SCARLET_SEMAPHORE_SCARLET_HPP
#include <pthread.h>
#include "utilities/debug.hpp"
class SemaphoreImpl {
  pthread_mutex_t _mutex;
  pthread_cond_t _cond;
  uint _count;
public:
  explicit SemaphoreImpl(uint value) : _count(value) {
    guarantee(pthread_mutex_init(&_mutex, nullptr) == 0, "semaphore mutex init");
    guarantee(pthread_cond_init(&_cond, nullptr) == 0, "semaphore condition init");
  }
  ~SemaphoreImpl() {
    guarantee(pthread_cond_destroy(&_cond) == 0, "semaphore condition destroy");
    guarantee(pthread_mutex_destroy(&_mutex) == 0, "semaphore mutex destroy");
  }
  void signal(uint count = 1) {
    guarantee(pthread_mutex_lock(&_mutex) == 0, "semaphore lock");
    guarantee(count <= UINT_MAX - _count, "semaphore overflow");
    _count += count;
    guarantee(pthread_cond_broadcast(&_cond) == 0, "semaphore notify");
    guarantee(pthread_mutex_unlock(&_mutex) == 0, "semaphore unlock");
  }
  void wait() {
    guarantee(pthread_mutex_lock(&_mutex) == 0, "semaphore lock");
    while (_count == 0) {
      guarantee(pthread_cond_wait(&_cond, &_mutex) == 0, "semaphore wait");
    }
    --_count;
    guarantee(pthread_mutex_unlock(&_mutex) == 0, "semaphore unlock");
  }
  bool trywait() {
    guarantee(pthread_mutex_lock(&_mutex) == 0, "semaphore lock");
    bool available = _count > 0;
    if (available) --_count;
    guarantee(pthread_mutex_unlock(&_mutex) == 0, "semaphore unlock");
    return available;
  }
};
#endif
