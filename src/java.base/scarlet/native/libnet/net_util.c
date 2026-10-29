/*
 * Copyright (c) 1998, 2025, Oracle and/or its affiliates. All rights reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_util.h"

static jint IPv4_available;

JNIEXPORT jint JNICALL
DEF_JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_2) != JNI_OK)
        return JNI_EVERSION;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    IPv4_available = fd >= 0;
    if (fd >= 0) close(fd);
    return JNI_VERSION_1_2;
}

JNIEXPORT jint JNICALL ipv4_available(void) { return IPv4_available; }
/* Both the native resolver and socket backend currently support IPv4 only. */
JNIEXPORT jint JNICALL ipv6_available(void) { return JNI_FALSE; }

void initInetAddressIDs(JNIEnv *env) {
    /* JNI FindClass initializes Inet4Address under its Java class lock. The
     * shared init entry point then publishes its global class/constructor IDs. */
    jclass c = (*env)->FindClass(env, "java/net/Inet4Address");
    if (c != NULL) (*env)->DeleteLocalRef(env, c);
}

void setInetAddress_addr(JNIEnv *env, jobject iaObj, int address) {
    jobject holder = (*env)->GetObjectField(env, iaObj, ia_holderID);
    CHECK_NULL_THROW_NPE(env, holder, "InetAddress holder is null");
    (*env)->SetIntField(env, holder, iac_addressID, address);
    (*env)->DeleteLocalRef(env, holder);
}

void setInetAddress_hostName(JNIEnv *env, jobject iaObj, jobject host) {
    jobject holder = (*env)->GetObjectField(env, iaObj, ia_holderID);
    CHECK_NULL_THROW_NPE(env, holder, "InetAddress holder is null");
    (*env)->SetObjectField(env, holder, iac_hostNameID, host);
    (*env)->SetObjectField(env, holder, iac_origHostNameID, host);
    (*env)->DeleteLocalRef(env, holder);
}

void NET_ThrowUnknownHostExceptionWithGaiError(JNIEnv *env,
        const char *hostname, int error) {
    if (error == EAI_MEMORY) {
        JNU_ThrowOutOfMemoryError(env, "Native resolver allocation failed");
        return;
    }
    /* Read the JDK's initialized policy rather than maintaining a second,
     * unsynchronized native cache for concurrent resolver failures. */
    jclass policy = (*env)->FindClass(env, "jdk/internal/util/Exceptions");
    CHECK_NULL(policy);
    jfieldID field = (*env)->GetStaticFieldID(env, policy, "enhancedNonSocketExceptionText", "Z");
    CHECK_NULL(field);
    jboolean includeHost = (*env)->GetStaticBooleanField(env, policy, field);
    (*env)->DeleteLocalRef(env, policy);
    const char *detail = gai_strerror(error);
    if (detail == NULL) detail = "Resolver failure";
    size_t size = strlen(detail) + 3 + (includeHost ? strlen(hostname) : 0);
    char *buffer = malloc(size);
    if (buffer == NULL) {
        JNU_ThrowOutOfMemoryError(env, "Native resolver error allocation failed");
        return;
    }
    if (includeHost) snprintf(buffer, size, "%s: %s", hostname, detail);
    else snprintf(buffer, size, "%s", detail);
    jstring message = JNU_NewStringPlatform(env, buffer);
    free(buffer);
    if (message == NULL) return;
    jobject exception = JNU_NewObjectByName(env, "java/net/UnknownHostException",
            "(Ljava/lang/String;)V", message);
    (*env)->DeleteLocalRef(env, message);
    if (exception != NULL) (*env)->Throw(env, exception);
}
