/*
 * Copyright (c) 2000, 2023, Oracle and/or its affiliates. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include "net_util.h"
#include "java_net_Inet4AddressImpl.h"

JNIEXPORT jstring JNICALL
Java_java_net_Inet4AddressImpl_getLocalHostName(JNIEnv *env, jobject self) {
    /* Native Scarlet has no node-name service. Match the Unix gethostname
     * failure fallback; do not invent a hostname from Linux ABI metadata. */
    return (*env)->NewStringUTF(env, "localhost");
}

/*
 * Find an internet address for a given hostname. Note that this
 * code only works for addresses of type INET. The translation
 * of %d.%d.%d.%d to an address (int) occurs in java now, so the
 * String "host" shouldn't be a %d.%d.%d.%d string. The only
 * exception should be when any of the %d are out of range and
 * we fallback to a lookup.
 *
 * Class:     java_net_Inet4AddressImpl
 * Method:    lookupAllHostAddr
 * Signature: (Ljava/lang/String;)[Ljava/net/InetAddress;
 */
JNIEXPORT jobjectArray JNICALL
Java_java_net_Inet4AddressImpl_lookupAllHostAddr(JNIEnv *env, jobject this,
                                                 jstring host) {
    jobjectArray ret = NULL;
    const char *hostname;
    int error = 0;
    struct addrinfo hints, *res = NULL, *resNew = NULL, *last = NULL,
        *iterator;

    initInetAddressIDs(env);
    JNU_CHECK_EXCEPTION_RETURN(env, NULL);

    if (IS_NULL(host)) {
        JNU_ThrowNullPointerException(env, "host argument is null");
        return NULL;
    }
    hostname = JNU_GetStringPlatformCharsStrict(env, host, NULL);
    CHECK_NULL_RETURN(hostname, NULL);

    // try once, with our static buffer
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_CANONNAME;
    hints.ai_family = AF_INET;

    error = getaddrinfo(hostname, NULL, &hints, &res);

    if (error) {
        // report error
        NET_ThrowUnknownHostExceptionWithGaiError(env, hostname, error);
        goto cleanupAndReturn;
    } else {
        int i = 0;
        iterator = res;
        while (iterator != NULL) {
            // skip duplicates
            int skip = 0;
            struct addrinfo *iteratorNew = resNew;
            while (iteratorNew != NULL) {
                struct sockaddr_in *addr1, *addr2;
                addr1 = (struct sockaddr_in *)iterator->ai_addr;
                addr2 = (struct sockaddr_in *)iteratorNew->ai_addr;
                if (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr) {
                    skip = 1;
                    break;
                }
                iteratorNew = iteratorNew->ai_next;
            }

            if (!skip) {
                struct addrinfo *next
                    = (struct addrinfo *)malloc(sizeof(struct addrinfo));
                if (!next) {
                    JNU_ThrowOutOfMemoryError(env, "Native heap allocation failed");
                    ret = NULL;
                    goto cleanupAndReturn;
                }
                memcpy(next, iterator, sizeof(struct addrinfo));
                next->ai_next = NULL;
                if (resNew == NULL) {
                    resNew = next;
                } else {
                    last->ai_next = next;
                }
                last = next;
                i++;
            }
            iterator = iterator->ai_next;
        }

        // allocate array - at this point i contains the number of addresses
        ret = (*env)->NewObjectArray(env, i, ia_class, NULL);
        if (IS_NULL(ret)) {
            goto cleanupAndReturn;
        }

        i = 0;
        iterator = resNew;
        while (iterator != NULL) {
            jobject iaObj = (*env)->NewObject(env, ia4_class, ia4_ctrID);
            if (IS_NULL(iaObj)) {
                ret = NULL;
                goto cleanupAndReturn;
            }
            setInetAddress_addr(env, iaObj, ntohl(((struct sockaddr_in *)
                                (iterator->ai_addr))->sin_addr.s_addr));
            if ((*env)->ExceptionCheck(env))
                goto cleanupAndReturn;
            setInetAddress_hostName(env, iaObj, host);
            if ((*env)->ExceptionCheck(env))
                goto cleanupAndReturn;
            (*env)->SetObjectArrayElement(env, ret, i++, iaObj);
            (*env)->DeleteLocalRef(env, iaObj);
            if ((*env)->ExceptionCheck(env)) goto cleanupAndReturn;
            iterator = iterator->ai_next;
        }
    }
cleanupAndReturn:
    JNU_ReleaseStringPlatformChars(env, host, hostname);
    while (resNew != NULL) {
        last = resNew;
        resNew = resNew->ai_next;
        free(last);
    }
    if (res != NULL) {
        freeaddrinfo(res);
    }
    return ret;
}

/*
 * Class:     java_net_Inet4AddressImpl
 * Method:    getHostByAddr
 * Signature: ([B)Ljava/lang/String;
 *
 * Theoretically the UnknownHostException could be enriched with gai error
 * information. But as it is silently ignored anyway, there's no need for this.
 * It's only important that either a valid hostname is returned or an
 * UnknownHostException is thrown.
 */
JNIEXPORT jstring JNICALL
Java_java_net_Inet4AddressImpl_getHostByAddr(JNIEnv *env, jobject this,
                                             jbyteArray addrArray) {
    jstring ret = NULL;
    char host[NI_MAXHOST + 1];
    struct sockaddr_in sa;

    // construct a sockaddr_in structure
    memset((char *)&sa, 0, sizeof(struct sockaddr_in));
    (*env)->GetByteArrayRegion(env, addrArray, 0, 4, (jbyte *)&sa.sin_addr);
    JNU_CHECK_EXCEPTION_RETURN(env, NULL);
    sa.sin_family = AF_INET;

    if (getnameinfo((struct sockaddr *)&sa, sizeof(struct sockaddr_in),
                    host, sizeof(host), NULL, 0, NI_NAMEREQD)) {
        JNU_ThrowByName(env, "java/net/UnknownHostException", NULL);
    } else {
        ret = (*env)->NewStringUTF(env, host);
        if (ret == NULL) {
            JNU_ThrowByName(env, "java/net/UnknownHostException", NULL);
        }
    }

    return ret;
}

static jlong monotonicMillis(JNIEnv *env) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
        JNU_ThrowIOExceptionWithLastError(env, "Reachability clock failed");
        return -1;
    }
    return (jlong)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* Use the JDK's TCP echo-port fallback. Native libc has no ICMP packet API.
 * A connection refusal also demonstrates that the target host was reached. */
JNIEXPORT jboolean JNICALL
Java_java_net_Inet4AddressImpl_isReachable0(JNIEnv *env, jobject self,
        jbyteArray address, jint timeout, jbyteArray interface, jint ttl) {
    struct sockaddr_in target = {0}, source = {0};
    int fd = -1, error;
    jboolean reached = JNI_FALSE;
    jlong start = monotonicMillis(env);
    JNU_CHECK_EXCEPTION_RETURN(env, JNI_FALSE);
    jlong deadline = start + timeout;
    if ((*env)->GetArrayLength(env, address) != 4) return JNI_FALSE;
    target.sin_family = AF_INET;
    target.sin_port = htons(7);
    (*env)->GetByteArrayRegion(env, address, 0, 4, (jbyte *)&target.sin_addr);
    JNU_CHECK_EXCEPTION_RETURN(env, JNI_FALSE);
    if (interface != NULL) {
        if ((*env)->GetArrayLength(env, interface) != 4) return JNI_FALSE;
        source.sin_family = AF_INET;
        (*env)->GetByteArrayRegion(env, interface, 0, 4, (jbyte *)&source.sin_addr);
        JNU_CHECK_EXCEPTION_RETURN(env, JNI_FALSE);
    }
    fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) goto ioError;
    if (ttl > 0 && setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
        goto ioError;
    if (interface != NULL && bind(fd, (struct sockaddr *)&source, sizeof(source)) < 0)
        goto ioError;
    if (connect(fd, (struct sockaddr *)&target, sizeof(target)) == 0 || errno == ECONNREFUSED) {
        reached = JNI_TRUE;
        goto cleanup;
    }
    if (errno == ENETUNREACH || errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT
#ifdef EHOSTUNREACH
            || errno == EHOSTUNREACH
#endif
       )
        goto cleanup;
    if (errno != EINPROGRESS) goto ioError;
    for (;;) {
        jlong now = monotonicMillis(env);
        if ((*env)->ExceptionCheck(env)) goto cleanup;
        jint remaining = now >= deadline ? 0 : (jint)(deadline - now);
        struct pollfd waiting = {fd, POLLOUT, 0};
        int ready = poll(&waiting, 1, remaining);
        if (ready < 0) {
            if (errno == EINTR) {
                if (now >= deadline) goto cleanup;
                continue;
            }
            goto ioError;
        }
        if (ready == 0) goto cleanup;
        if (waiting.revents & POLLNVAL) {
            errno = EBADF;
            goto ioError;
        }
        socklen_t length = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) goto ioError;
        reached = error == 0 || error == ECONNREFUSED;
        goto cleanup;
    }
ioError:
    error = errno;
    if (fd >= 0) close(fd);
    errno = error;
    JNU_ThrowIOExceptionWithLastError(env, "TCP reachability probe failed");
    return JNI_FALSE;
cleanup:
    close(fd);
    return reached;
}
