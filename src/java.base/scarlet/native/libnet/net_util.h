/*
 * Copyright (c) 1997, 2025, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SCARLET_NET_UTIL_H
#define SCARLET_NET_UTIL_H
#include "jvm.h"
#include "jni_util.h"
#include <netdb.h>
#include <sys/socket.h>

extern jclass ia_class;
extern jclass ia4_class;
extern jmethodID ia4_ctrID;
extern jfieldID ia_holderID;
extern jfieldID iac_addressID;
extern jfieldID iac_hostNameID;
extern jfieldID iac_origHostNameID;

JNIEXPORT jint JNICALL ipv4_available(void);
JNIEXPORT jint JNICALL ipv6_available(void);
void initInetAddressIDs(JNIEnv *env);
void setInetAddress_addr(JNIEnv *env, jobject address, int value);
void setInetAddress_hostName(JNIEnv *env, jobject address, jobject name);
void NET_ThrowUnknownHostExceptionWithGaiError(JNIEnv *env, const char *hostname, int error);
#endif
