/*
 * Copyright (c) 2014, 2023, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 */

#include <unistd.h>
#include "jni.h"
#include "jni_util.h"
#include "java_lang_ProcessHandleImpl.h"
#include "java_lang_ProcessHandleImpl_Info.h"

static void unsupported(JNIEnv *env) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                   "External process inspection/control is not available on Scarlet");
}

JNIEXPORT void JNICALL
Java_java_lang_ProcessHandleImpl_initNative(JNIEnv *env, jclass cls) {
    /* Current pid/parent need no native initialization. */
}

JNIEXPORT jlong JNICALL
Java_java_lang_ProcessHandleImpl_getCurrentPid0(JNIEnv *env, jclass cls) {
    return getpid();
}

JNIEXPORT jlong JNICALL
Java_java_lang_ProcessHandleImpl_isAlive0(JNIEnv *env, jobject obj, jlong pid) {
    if (pid == (jlong)getpid()) return 0; /* Alive; start time unavailable. */
    if (pid <= 0) return -1;
    unsupported(env);
    return -1;
}

JNIEXPORT jlong JNICALL
Java_java_lang_ProcessHandleImpl_parent0(JNIEnv *env, jobject obj, jlong pid,
                                        jlong startTime) {
    if (pid == (jlong)getpid()) return getppid();
    unsupported(env);
    return -1;
}

JNIEXPORT jint JNICALL
Java_java_lang_ProcessHandleImpl_waitForProcessExit0(JNIEnv *env, jclass cls,
                                                    jlong pid, jboolean reap) {
    unsupported(env);
    return -1;
}

JNIEXPORT jboolean JNICALL
Java_java_lang_ProcessHandleImpl_destroy0(JNIEnv *env, jobject obj, jlong pid,
                                         jlong startTime, jboolean force) {
    unsupported(env);
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_java_lang_ProcessHandleImpl_getProcessPids0(JNIEnv *env, jclass cls,
    jlong pid, jlongArray pids, jlongArray parents, jlongArray startTimes) {
    unsupported(env);
    return -1;
}

JNIEXPORT void JNICALL
Java_java_lang_ProcessHandleImpl_00024Info_initIDs(JNIEnv *env, jclass cls) {
    /* No cached fields: info0 reports the unavailable facility. */
}

JNIEXPORT void JNICALL
Java_java_lang_ProcessHandleImpl_00024Info_info0(JNIEnv *env, jobject info, jlong pid) {
    unsupported(env);
}
