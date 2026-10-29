/*
 * Copyright (c) 2002, 2025, Oracle and/or its affiliates. All rights reserved.
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

/* Scarlet has no POSIX signal mechanism for interrupting file operations.
 * NativeThread's documented zero-token path tracks nonsignalable operations
 * and waits for them to retire. This does not enable socket/selector support. */
#include "jni.h"
#include "jni_util.h"
#include "sun_nio_ch_NativeThread.h"
JNIEXPORT void JNICALL
Java_sun_nio_ch_NativeThread_init(JNIEnv *env, jclass klass) {}
JNIEXPORT jlong JNICALL
Java_sun_nio_ch_NativeThread_current0(JNIEnv *env, jclass klass) { return 0; }
JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_NativeThread_supportPendingSignals0(JNIEnv *env, jclass klass) { return JNI_FALSE; }
JNIEXPORT void JNICALL
Java_sun_nio_ch_NativeThread_signal0(JNIEnv *env, jclass klass, jlong token) {
    JNU_ThrowByName(env, "java/lang/UnsupportedOperationException",
                   "Native thread signalling is unavailable on Scarlet");
}
