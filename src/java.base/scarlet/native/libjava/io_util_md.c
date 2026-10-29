/*
 * Copyright (c) 2001, 2022, Oracle and/or its affiliates. All rights reserved.
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

#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "io_util.h"
#include "io_util_md.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/* off_t is already 64 bits in the Scarlet ABI. */
_Static_assert(sizeof(off_t) == sizeof(jlong), "Scarlet requires 64-bit file offsets");

FD handleOpen(const char *path, int flags, int mode) {
    FD fd;
    int result;
    struct stat st;
    RESTARTABLE(open(path, flags, mode), fd);
    if (fd < 0) return -1;
    RESTARTABLE(fstat(fd, &st), result);
    if (result < 0 || S_ISDIR(st.st_mode)) {
        int error = result < 0 ? errno : EISDIR;
        close(fd);
        errno = error;
        return -1;
    }
    return fd;
}

FD getFD(JNIEnv *env, jobject obj, jfieldID fid) {
    jobject descriptor = (*env)->GetObjectField(env, obj, fid);
    FD fd;
    if (descriptor == NULL) return -1;
    fd = (*env)->GetIntField(env, descriptor, IO_fd_fdID);
    (*env)->DeleteLocalRef(env, descriptor);
    return fd;
}

void fileOpen(JNIEnv *env, jobject obj, jstring path, jfieldID fid, int flags) {
    WITH_PLATFORM_STRING(env, path, native_path) {
        FD fd = handleOpen(native_path, flags, 0666);
        if (fd < 0) {
            throwFileNotFoundException(env, path);
        } else {
            jobject descriptor = (*env)->GetObjectField(env, obj, fid);
            if (descriptor == NULL) {
                close(fd);
                if (!(*env)->ExceptionCheck(env))
                    JNU_ThrowIOException(env, "Stream Closed");
            } else {
                (*env)->SetIntField(env, descriptor, IO_fd_fdID, fd);
                (*env)->SetBooleanField(env, descriptor, IO_append_fdID,
                                       (flags & O_APPEND) ? JNI_TRUE : JNI_FALSE);
                (*env)->DeleteLocalRef(env, descriptor);
            }
        }
    } END_PLATFORM_STRING(env, native_path);
}

void fileDescriptorClose(JNIEnv *env, jobject descriptor) {
    FD fd = (*env)->GetIntField(env, descriptor, IO_fd_fdID);
    if ((*env)->ExceptionCheck(env) || fd == -1) return;
    (*env)->SetIntField(env, descriptor, IO_fd_fdID, -1);
    if ((*env)->ExceptionCheck(env)) return;
    /* dup2 is unavailable. Close the actual descriptor, including standard
     * streams, rather than claim to have redirected it to /dev/null. */
    if (close(fd) < 0)
        JNU_ThrowIOExceptionWithLastError(env, "close failed");
}

ssize_t handleRead(FD fd, void *buffer, jint length) {
    ssize_t result;
    RESTARTABLE(read(fd, buffer, length), result);
    return result;
}

ssize_t handleWrite(FD fd, const void *buffer, jint length) {
    ssize_t result;
    RESTARTABLE(write(fd, buffer, length), result);
    return result;
}

jint handleAvailable(FD fd, jlong *bytes) {
    struct stat st;
    off_t position;
    int result;
    RESTARTABLE(fstat(fd, &st), result);
    if (result < 0) {
        /* Native pipe handles have no FileObject metadata capability. Check
         * that the descriptor is still open before returning the conservative
         * zero estimate permitted by InputStream.available(). */
        if (fcntl(fd, F_GETFD) < 0) return 0;
        *bytes = 0;
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        /* Native has no queued-byte query for streams. Zero is an estimate,
         * not EOF: callers must still perform their normal blocking read. */
        *bytes = 0;
        return 1;
    }
    RESTARTABLE(lseek(fd, 0, SEEK_CUR), position);
    if (position < 0) return 0;
    *bytes = st.st_size > position ? st.st_size - position : 0;
    return 1;
}

jint handleSetLength(FD fd, jlong length) {
    int result;
    RESTARTABLE(ftruncate(fd, length), result);
    return result;
}

jlong handleGetLength(FD fd) {
    struct stat st;
    int result;
    RESTARTABLE(fstat(fd, &st), result);
    if (result < 0) return -1;
    if (!S_ISREG(st.st_mode)) {
        errno = ENOTSUP;
        return -1;
    }
    return st.st_size;
}

jboolean handleIsRegularFile(JNIEnv* env, FD fd) {
    struct stat st;
    int result;
    RESTARTABLE(fstat(fd, &st), result);
    if (result < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "fstat failed");
        return JNI_FALSE;
    }
    return S_ISREG(st.st_mode) ? JNI_TRUE : JNI_FALSE;
}
