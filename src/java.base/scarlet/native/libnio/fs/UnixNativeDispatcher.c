/*
 * Copyright (c) 2008, 2025, Oracle and/or its affiliates. All rights reserved.
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

/* Scarlet's initial NIO backend exposes read-only file metadata and paths.
 * Channel IO, directory enumeration and mutation are separate porting work.
 * Do not advertise optional *at, xattr or birth-time capabilities.
 */
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "jni.h"
#include "jni_util.h"
#include "jlong.h"
#include "sun_nio_fs_UnixNativeDispatcher.h"

static jfieldID attrs_st_mode;
static jfieldID attrs_st_ino;
static jfieldID attrs_st_dev;
static jfieldID attrs_st_rdev;
static jfieldID attrs_st_nlink;
static jfieldID attrs_st_uid;
static jfieldID attrs_st_gid;
static jfieldID attrs_st_size;
static jfieldID attrs_st_atime_sec;
static jfieldID attrs_st_atime_nsec;
static jfieldID attrs_st_mtime_sec;
static jfieldID attrs_st_mtime_nsec;
static jfieldID attrs_st_ctime_sec;
static jfieldID attrs_st_ctime_nsec;

static void throwUnixException(JNIEnv* env, int errnum) {
    jobject x = JNU_NewObjectByName(env, "sun/nio/fs/UnixException",
        "(I)V", errnum);
    if (x != NULL) {
        (*env)->Throw(env, x);
    }
}

static void copy_stat_attributes(JNIEnv* env, struct stat* buf, jobject attrs) {
    (*env)->SetIntField(env, attrs, attrs_st_mode, (jint)buf->st_mode);
    (*env)->SetLongField(env, attrs, attrs_st_ino, (jlong)buf->st_ino);
    (*env)->SetLongField(env, attrs, attrs_st_dev, (jlong)buf->st_dev);
    (*env)->SetLongField(env, attrs, attrs_st_rdev, (jlong)buf->st_rdev);
    (*env)->SetIntField(env, attrs, attrs_st_nlink, (jint)buf->st_nlink);
    (*env)->SetIntField(env, attrs, attrs_st_uid, (jint)buf->st_uid);
    (*env)->SetIntField(env, attrs, attrs_st_gid, (jint)buf->st_gid);
    (*env)->SetLongField(env, attrs, attrs_st_size, (jlong)buf->st_size);
    (*env)->SetLongField(env, attrs, attrs_st_atime_sec, (jlong)buf->st_atime);
    (*env)->SetLongField(env, attrs, attrs_st_mtime_sec, (jlong)buf->st_mtime);
    (*env)->SetLongField(env, attrs, attrs_st_ctime_sec, (jlong)buf->st_ctime);

    (*env)->SetLongField(env, attrs, attrs_st_atime_nsec, (jlong)buf->st_atim.tv_nsec);
    (*env)->SetLongField(env, attrs, attrs_st_mtime_nsec, (jlong)buf->st_mtim.tv_nsec);
    (*env)->SetLongField(env, attrs, attrs_st_ctime_nsec, (jlong)buf->st_ctim.tv_nsec);

}

JNIEXPORT jint JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_init(JNIEnv* env, jclass this)
{
    jclass clazz;

    clazz = (*env)->FindClass(env, "sun/nio/fs/UnixFileAttributes");
    CHECK_NULL_RETURN(clazz, 0);
    attrs_st_mode = (*env)->GetFieldID(env, clazz, "st_mode", "I");
    CHECK_NULL_RETURN(attrs_st_mode, 0);
    attrs_st_ino = (*env)->GetFieldID(env, clazz, "st_ino", "J");
    CHECK_NULL_RETURN(attrs_st_ino, 0);
    attrs_st_dev = (*env)->GetFieldID(env, clazz, "st_dev", "J");
    CHECK_NULL_RETURN(attrs_st_dev, 0);
    attrs_st_rdev = (*env)->GetFieldID(env, clazz, "st_rdev", "J");
    CHECK_NULL_RETURN(attrs_st_rdev, 0);
    attrs_st_nlink = (*env)->GetFieldID(env, clazz, "st_nlink", "I");
    CHECK_NULL_RETURN(attrs_st_nlink, 0);
    attrs_st_uid = (*env)->GetFieldID(env, clazz, "st_uid", "I");
    CHECK_NULL_RETURN(attrs_st_uid, 0);
    attrs_st_gid = (*env)->GetFieldID(env, clazz, "st_gid", "I");
    CHECK_NULL_RETURN(attrs_st_gid, 0);
    attrs_st_size = (*env)->GetFieldID(env, clazz, "st_size", "J");
    CHECK_NULL_RETURN(attrs_st_size, 0);
    attrs_st_atime_sec = (*env)->GetFieldID(env, clazz, "st_atime_sec", "J");
    CHECK_NULL_RETURN(attrs_st_atime_sec, 0);
    attrs_st_atime_nsec = (*env)->GetFieldID(env, clazz, "st_atime_nsec", "J");
    CHECK_NULL_RETURN(attrs_st_atime_nsec, 0);
    attrs_st_mtime_sec = (*env)->GetFieldID(env, clazz, "st_mtime_sec", "J");
    CHECK_NULL_RETURN(attrs_st_mtime_sec, 0);
    attrs_st_mtime_nsec = (*env)->GetFieldID(env, clazz, "st_mtime_nsec", "J");
    CHECK_NULL_RETURN(attrs_st_mtime_nsec, 0);
    attrs_st_ctime_sec = (*env)->GetFieldID(env, clazz, "st_ctime_sec", "J");
    CHECK_NULL_RETURN(attrs_st_ctime_sec, 0);
    attrs_st_ctime_nsec = (*env)->GetFieldID(env, clazz, "st_ctime_nsec", "J");
    CHECK_NULL_RETURN(attrs_st_ctime_nsec, 0);

    // No optional openat, xattr or birth-time capabilities are implemented.
    return 0;
}

JNIEXPORT jbyteArray JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_getcwd(JNIEnv* env, jclass this) {
    jbyteArray result = NULL;
    char buf[PATH_MAX+1];

    /* EINTR not listed as a possible error */
    char* cwd = getcwd(buf, sizeof(buf));
    if (cwd == NULL) {
        throwUnixException(env, errno);
    } else {
        jsize len = (jsize)strlen(buf);
        result = (*env)->NewByteArray(env, len);
        if (result != NULL) {
            (*env)->SetByteArrayRegion(env, result, 0, len, (jbyte*)buf);
        }
    }
    return result;
}

JNIEXPORT jbyteArray JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_strerror(JNIEnv* env, jclass this, jint error)
{
    char tmpbuf[1024];
    jsize len;
    jbyteArray bytes;

    getErrorString((int)error, tmpbuf, sizeof(tmpbuf));
    len = strlen(tmpbuf);
    bytes = (*env)->NewByteArray(env, len);
    if (bytes != NULL) {
        (*env)->SetByteArrayRegion(env, bytes, 0, len, (jbyte*)tmpbuf);
    }
    return bytes;
}

JNIEXPORT jint JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_stat0(JNIEnv* env, jclass this,
    jlong pathAddress, jobject attrs)
{
    int err;
    struct stat buf;
    const char* path = (const char*)jlong_to_ptr(pathAddress);
    RESTARTABLE(stat(path, &buf), err);
    if (err == 0) {
        copy_stat_attributes(env, &buf, attrs);
        return 0;
    } else {
        return errno;
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_lstat0(JNIEnv* env, jclass this,
    jlong pathAddress, jobject attrs)
{
    int err;
    struct stat buf;
    const char* path = (const char*)jlong_to_ptr(pathAddress);
    RESTARTABLE(lstat(path, &buf), err);
    if (err == -1) {
        throwUnixException(env, errno);
    } else {
        copy_stat_attributes(env, &buf, attrs);
    }
}

JNIEXPORT void JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_fstat0(JNIEnv* env, jclass this, jint fd,
    jobject attrs)
{
    int err;
    struct stat buf;
    RESTARTABLE(fstat((int)fd, &buf), err);
    if (err == -1) {
        throwUnixException(env, errno);
    } else {
        copy_stat_attributes(env, &buf, attrs);
    }
}

JNIEXPORT jbyteArray JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_realpath0(JNIEnv* env, jclass this,
    jlong pathAddress)
{
    jbyteArray result = NULL;
    char resolved[PATH_MAX+1];
    const char* path = (const char*)jlong_to_ptr(pathAddress);

    /* EINTR not listed as a possible error */
    if (realpath(path, resolved) == NULL) {
        throwUnixException(env, errno);
    } else {
        jsize len = (jsize)strlen(resolved);
        result = (*env)->NewByteArray(env, len);
        if (result != NULL) {
            (*env)->SetByteArrayRegion(env, result, 0, len, (jbyte*)resolved);
        }
    }
    return result;
}

JNIEXPORT jint JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_access0(JNIEnv* env, jclass this,
    jlong pathAddress, jint amode)
{
    int err;
    const char* path = (const char*)jlong_to_ptr(pathAddress);

    RESTARTABLE(access(path, (int)amode), err);

    return (err == -1) ? errno : 0;
}

JNIEXPORT jboolean JNICALL
Java_sun_nio_fs_UnixNativeDispatcher_fchmodatNoFollowSupported0(JNIEnv* env, jclass this) {
    return JNI_FALSE;
}
