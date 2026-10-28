/* Copyright (c) 2026, Scarlet contributors. SPDX-License-Identifier: GPL-2.0-only */
#ifndef OS_SCARLET_THREADCRASHPROTECTION_SCARLET_HPP
#define OS_SCARLET_THREADCRASHPROTECTION_SCARLET_HPP
#include "memory/allocation.hpp"
class Thread;
class ThreadCrashProtection : public StackObj {
public:
  static bool is_crash_protected(Thread*) { return false; }
};
#endif
