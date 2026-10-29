/*
 * Copyright (c) 2000, 2021, Oracle and/or its affiliates. All rights reserved.
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

/* Scarlet file-channel support. Descriptor limits and network pollers are
 * separate capabilities. The vector bounds are conservative POSIX limits. */
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "sun_nio_ch_IOUtil.h"
#include "nio.h"
#include "nio_util.h"
static jfieldID fd_fdID;
JNIEXPORT void JNICALL
Java_sun_nio_ch_IOUtil_initIDs(JNIEnv *env, jclass clazz)
{
    CHECK_NULL(clazz = (*env)->FindClass(env, "java/io/FileDescriptor"));
    CHECK_NULL(fd_fdID = (*env)->GetFieldID(env, clazz, "fd", "I"));
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_IOUtil_fdVal(JNIEnv *env, jclass clazz, jobject fdo)
{
    return (*env)->GetIntField(env, fdo, fd_fdID);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_IOUtil_setfdVal(JNIEnv *env, jclass clazz, jobject fdo, jint val)
{
    setfdval(env, fdo, val);
}

static int
configureBlocking(int fd, jboolean blocking)
{
    int flags = fcntl(fd, F_GETFL);
    int newflags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);

    return (flags == newflags) ? 0 : fcntl(fd, F_SETFL, newflags);
}

JNIEXPORT void JNICALL
Java_sun_nio_ch_IOUtil_configureBlocking(JNIEnv *env, jclass clazz,
                                         jobject fdo, jboolean blocking)
{
    if (configureBlocking(fdval(env, fdo), blocking) < 0)
        JNU_ThrowIOExceptionWithLastError(env, "Configure blocking failed");
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_IOUtil_iovMax(JNIEnv *env, jclass klass) {
    /* Native libc does not yet expose _SC_IOV_MAX. */
    return 16;
}
JNIEXPORT jlong JNICALL
Java_sun_nio_ch_IOUtil_writevMax(JNIEnv *env, jclass klass) {
    return INT_MAX;
}
JNIEXPORT jlong JNICALL
Java_sun_nio_ch_IOUtil_makePipe(JNIEnv *env, jobject self, jboolean blocking) {
    int fd[2];
    if (pipe2(fd, O_CLOEXEC | (blocking ? 0 : O_NONBLOCK)) < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "Pipe creation failed");
        return 0;
    }
    return ((jlong)fd[0] << 32) | (jlong)fd[1];
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_IOUtil_write1(JNIEnv *env, jclass cl, jint fd, jbyte b)
{
    char c = (char)b;
    return convertReturnVal(env, write(fd, &c, 1), JNI_FALSE);
}

JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_IOUtil_drain(JNIEnv *env, jclass cl, jint fd)
{
    char buf[16];
    int tn = 0;

    for (;;) {
        int n = read(fd, buf, sizeof(buf));
        tn += n;
        if ((n < 0) && (errno != EAGAIN && errno != EWOULDBLOCK))
            JNU_ThrowIOExceptionWithLastError(env, "Drain");
        if (n == (int)sizeof(buf))
            continue;
        return (tn > 0) ? JNI_TRUE : JNI_FALSE;
    }
}

JNIEXPORT jint JNICALL
Java_sun_nio_ch_IOUtil_drain1(JNIEnv *env, jclass cl, jint fd)
{
    int res;
    char buf[1];

    res = read(fd, buf, 1);
    if (res < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            res = 0;
        } else if (errno == EINTR) {
            return IOS_INTERRUPTED;
        } else {
            JNU_ThrowIOExceptionWithLastError(env, "read");
            return IOS_THROWN;
        }
    }
    return res;
}

/* Declared in nio_util.h for use elsewhere in NIO */

jint
convertReturnVal(JNIEnv *env, jint n, jboolean reading)
{
    if (n > 0) /* Number of bytes written */
        return n;
    else if (n == 0) {
        if (reading) {
            return IOS_EOF; /* EOF is -1 in javaland */
        } else {
            return 0;
        }
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
        return IOS_UNAVAILABLE;
    else if (errno == EINTR)
        return IOS_INTERRUPTED;
    else {
        const char *msg = reading ? "Read failed" : "Write failed";
        JNU_ThrowIOExceptionWithLastError(env, msg);
        return IOS_THROWN;
    }
}

/* Declared in nio_util.h for use elsewhere in NIO */

jlong
convertLongReturnVal(JNIEnv *env, jlong n, jboolean reading)
{
    if (n > 0) /* Number of bytes written */
        return n;
    else if (n == 0) {
        if (reading) {
            return IOS_EOF; /* EOF is -1 in javaland */
        } else {
            return 0;
        }
    }
    else if (errno == EAGAIN || errno == EWOULDBLOCK)
        return IOS_UNAVAILABLE;
    else if (errno == EINTR)
        return IOS_INTERRUPTED;
    else {
        const char *msg = reading ? "Read failed" : "Write failed";
        JNU_ThrowIOExceptionWithLastError(env, msg);
        return IOS_THROWN;
    }
}

jint
fdval(JNIEnv *env, jobject fdo)
{
    return (*env)->GetIntField(env, fdo, fd_fdID);
}

void
setfdval(JNIEnv *env, jobject fdo, jint val) {
    (*env)->SetIntField(env, fdo, fd_fdID, val);
}


