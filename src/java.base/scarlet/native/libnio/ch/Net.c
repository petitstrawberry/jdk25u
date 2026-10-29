/*
 * Copyright (c) 2001, 2026, Oracle and/or its affiliates. All rights reserved.
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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include "nio.h"
#include "nio_util.h"
#include "sun_nio_ch_Net.h"

static jclass inet_class, isa_class;
static jmethodID get_bytes, from_bytes, isa_constructor;

static jint socketError(JNIEnv *env) {
    const char *type = "java/net/SocketException";
    if (errno == ECONNREFUSED || errno == ETIMEDOUT || errno == ENOTCONN)
        type = "java/net/ConnectException";
    else if (errno == EADDRINUSE || errno == EADDRNOTAVAIL || errno == EACCES)
        type = "java/net/BindException";
    else if (errno == ENETUNREACH) type = "java/net/NoRouteToHostException";
    JNU_ThrowByNameWithLastError(env, type, "Native socket operation failed");
    return IOS_THROWN;
}

JNIEXPORT void JNICALL Java_sun_nio_ch_Net_initIDs(JNIEnv *env, jclass cls) {
    jclass local = (*env)->FindClass(env, "java/net/InetAddress");
    CHECK_NULL(local);
    inet_class = (*env)->NewGlobalRef(env, local); CHECK_NULL(inet_class);
    get_bytes = (*env)->GetMethodID(env, local, "getAddress", "()[B"); CHECK_NULL(get_bytes);
    from_bytes = (*env)->GetStaticMethodID(env, local, "getByAddress", "([B)Ljava/net/InetAddress;"); CHECK_NULL(from_bytes);
    local = (*env)->FindClass(env, "java/net/InetSocketAddress"); CHECK_NULL(local);
    isa_class = (*env)->NewGlobalRef(env, local); CHECK_NULL(isa_class);
    isa_constructor = (*env)->GetMethodID(env, local, "<init>", "(Ljava/net/InetAddress;I)V"); CHECK_NULL(isa_constructor);
}

static int address(JNIEnv *env, jobject value, jint port, struct sockaddr_in *sa) {
    jbyteArray bytes = (*env)->CallObjectMethod(env, value, get_bytes);
    JNU_CHECK_EXCEPTION_RETURN(env, -1);
    CHECK_NULL_RETURN(bytes, -1);
    if ((*env)->GetArrayLength(env, bytes) != 4) {
        (*env)->DeleteLocalRef(env, bytes);
        JNU_ThrowByName(env, "java/net/SocketException", "IPv6 transport is unavailable on Scarlet");
        return -1;
    }
    memset(sa, 0, sizeof(*sa)); sa->sin_family = AF_INET; sa->sin_port = htons(port);
    (*env)->GetByteArrayRegion(env, bytes, 0, 4, (jbyte *)&sa->sin_addr);
    (*env)->DeleteLocalRef(env, bytes);
    return (*env)->ExceptionCheck(env) ? -1 : 0;
}

static jobject inetAddress(JNIEnv *env, struct sockaddr_in *sa) {
    if (sa->sin_family != AF_INET) {
        JNU_ThrowByName(env, "java/net/SocketException", "Unsupported native socket address family");
        return NULL;
    }
    jbyteArray bytes = (*env)->NewByteArray(env, 4); CHECK_NULL_RETURN(bytes, NULL);
    (*env)->SetByteArrayRegion(env, bytes, 0, 4, (jbyte *)&sa->sin_addr);
    JNU_CHECK_EXCEPTION_RETURN(env, NULL);
    jobject result = (*env)->CallStaticObjectMethod(env, inet_class, from_bytes, bytes);
    if ((*env)->ExceptionCheck(env)) result = NULL;
    (*env)->DeleteLocalRef(env, bytes);
    return result;
}

JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_socket0(JNIEnv *env, jclass cls,
        jboolean ipv6, jboolean stream, jboolean reuse, jboolean fast) {
    if (ipv6) { JNU_ThrowByName(env, "java/net/SocketException", "IPv6 transport is unavailable"); return IOS_THROWN; }
    int fd = socket(AF_INET, (stream ? SOCK_STREAM : SOCK_DGRAM) | SOCK_CLOEXEC, 0);
    if (fd < 0) return socketError(env);
    if (reuse) {
        int enabled = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) < 0) {
            int error=errno; close(fd); errno=error; return socketError(env);
        }
    }
    return fd;
}
JNIEXPORT void JNICALL Java_sun_nio_ch_Net_bind0(JNIEnv *env, jclass cls, jobject fdo,
        jboolean ipv6, jboolean exclusive, jobject value, jint port) {
    struct sockaddr_in sa;
    if (address(env,value,port,&sa)<0) return;
    if (bind(fdval(env,fdo),(struct sockaddr *)&sa,sizeof(sa))<0) socketError(env);
}
JNIEXPORT void JNICALL Java_sun_nio_ch_Net_listen(JNIEnv *env,jclass cls,jobject fdo,jint backlog) {
    if (listen(fdval(env,fdo),backlog)<0) socketError(env);
}
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_connect0(JNIEnv *env,jclass cls,jboolean ipv6,jobject fdo,jobject value,jint port) {
    struct sockaddr_in sa;
    if(address(env,value,port,&sa)<0) return IOS_THROWN;
    if(connect(fdval(env,fdo),(struct sockaddr *)&sa,sizeof(sa))==0) return 1;
    if(errno==EINPROGRESS) return IOS_UNAVAILABLE;
    if(errno==EINTR) return IOS_INTERRUPTED;
    return socketError(env);
}
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_accept(JNIEnv *env,jclass cls,jobject fdo,jobject newfdo,jobjectArray values) {
    struct sockaddr_in sa; socklen_t length=sizeof(sa); int fd;
    do { fd=accept(fdval(env,fdo),(struct sockaddr *)&sa,&length); } while(fd<0 && errno==ECONNABORTED);
    if(fd<0) {
        if(errno==EAGAIN || errno==EWOULDBLOCK) return IOS_UNAVAILABLE;
        if(errno==EINTR) return IOS_INTERRUPTED;
        return socketError(env);
    }
    if(fcntl(fd,F_SETFD,FD_CLOEXEC)<0) { int error=errno;close(fd);errno=error;return socketError(env); }
    jobject ip=inetAddress(env,&sa);
    if(ip==NULL) { close(fd);return IOS_THROWN; }
    jobject endpoint=(*env)->NewObject(env,isa_class,isa_constructor,ip,(jint)ntohs(sa.sin_port));
    (*env)->DeleteLocalRef(env,ip);
    if(endpoint==NULL) { close(fd);return IOS_THROWN; }
    (*env)->SetObjectArrayElement(env,values,0,endpoint);
    (*env)->DeleteLocalRef(env,endpoint);
    if((*env)->ExceptionCheck(env)) { close(fd);return IOS_THROWN; }
    setfdval(env,newfdo,fd); return 1;
}
static int socketName(JNIEnv *env,jobject fdo,struct sockaddr_in *sa,jboolean peer) {
    socklen_t length=sizeof(*sa);
    int result=peer ? getpeername(fdval(env,fdo),(struct sockaddr *)sa,&length) : getsockname(fdval(env,fdo),(struct sockaddr *)sa,&length);
    if(result<0) { socketError(env);return -1; }
    return 0;
}
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_localPort(JNIEnv *env,jclass cls,jobject fdo) {
    struct sockaddr_in sa; if(socketName(env,fdo,&sa,JNI_FALSE)<0) return -1; return ntohs(sa.sin_port);
}
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_remotePort(JNIEnv *env,jclass cls,jobject fdo) {
    struct sockaddr_in sa; if(socketName(env,fdo,&sa,JNI_TRUE)<0) return -1; return ntohs(sa.sin_port);
}
JNIEXPORT jobject JNICALL Java_sun_nio_ch_Net_localInetAddress(JNIEnv *env,jclass cls,jobject fdo) {
    struct sockaddr_in sa; if(socketName(env,fdo,&sa,JNI_FALSE)<0) return NULL; return inetAddress(env,&sa);
}
JNIEXPORT jobject JNICALL Java_sun_nio_ch_Net_remoteInetAddress(JNIEnv *env,jclass cls,jobject fdo) {
    struct sockaddr_in sa; if(socketName(env,fdo,&sa,JNI_TRUE)<0) return NULL; return inetAddress(env,&sa);
}
JNIEXPORT void JNICALL Java_sun_nio_ch_Net_shutdown(JNIEnv *env,jclass cls,jobject fdo,jint how) {
    if(shutdown(fdval(env,fdo),how)<0) socketError(env);
}
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_getIntOption0(JNIEnv *env,jclass cls,jobject fdo,jboolean convert,jint level,jint option) {
    int value=0; struct linger linger; void *data=&value; socklen_t length=sizeof(value);
    if(level==SOL_SOCKET && option==SO_LINGER) { data=&linger;length=sizeof(linger); }
    if(getsockopt(fdval(env,fdo),level,option,data,&length)<0) return socketError(env);
    return level==SOL_SOCKET && option==SO_LINGER ? (linger.l_onoff ? linger.l_linger : -1) : value;
}
JNIEXPORT void JNICALL Java_sun_nio_ch_Net_setIntOption0(JNIEnv *env,jclass cls,jobject fdo,jboolean convert,jint level,jint option,jint value,jboolean ipv6) {
    struct linger linger={value>=0,value>=0?value:0}; const void *data=&value; socklen_t length=sizeof(value);
    if(level==SOL_SOCKET && option==SO_LINGER) { data=&linger;length=sizeof(linger); }
    if(setsockopt(fdval(env,fdo),level,option,data,length)<0) socketError(env);
}
JNIEXPORT jint JNICALL
Java_sun_nio_ch_Net_poll(JNIEnv* env, jclass this, jobject fdo, jint events, jlong timeout)
{
    struct pollfd pfd;
    int rv;
    pfd.fd = fdval(env, fdo);
    pfd.events = events;
    if (timeout < -1) {
        timeout = -1;
    } else if (timeout > INT_MAX) {
        timeout = INT_MAX;
    }
    rv = poll(&pfd, 1, (int)timeout);

    if (rv >= 0) {
        return pfd.revents;
    } else if (errno == EINTR) {
        // interrupted, no events to return
        return 0;
    } else {
        socketError(env);
        return IOS_THROWN;
    }
}

JNIEXPORT jboolean JNICALL
Java_sun_nio_ch_Net_pollConnect(JNIEnv *env, jobject this, jobject fdo, jlong timeout)
{
    jint fd = fdval(env, fdo);
    struct pollfd poller;
    int result;

    poller.fd = fd;
    poller.events = POLLOUT;
    poller.revents = 0;
    if (timeout < -1) {
        timeout = -1;
    } else if (timeout > INT_MAX) {
        timeout = INT_MAX;
    }

    result = poll(&poller, 1, (int)timeout);

    if (result > 0) {
        int error = 0;
        socklen_t n = sizeof(int);
        errno = 0;
        result = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &n);
        if (result < 0) {
            socketError(env);
            return JNI_FALSE;
        } else if (error) {
            (errno=error, socketError(env));
            return JNI_FALSE;
        } else if ((poller.revents & POLLHUP) != 0) {
            (errno=ENOTCONN, socketError(env));
            return JNI_FALSE;
        }
        // connected
        return JNI_TRUE;
    } else if (result == 0 || errno == EINTR) {
        return JNI_FALSE;
    } else {
        JNU_ThrowIOExceptionWithLastError(env, "poll failed");
        return JNI_FALSE;
    }
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_pollinValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLIN;
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_polloutValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLOUT;
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_pollerrValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLERR;
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_pollhupValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLHUP;
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_pollnvalValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLNVAL;
}

JNIEXPORT jshort JNICALL
Java_sun_nio_ch_Net_pollconnValue(JNIEnv *env, jclass this)
{
    return (jshort)POLLOUT;
}

JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_isIPv6Available0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_isReusePortAvailable0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_shouldShutdownWriteBeforeClose0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_shouldSetBothIPv4AndIPv6Options0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_canIPv6SocketJoinIPv4Group0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_canJoin6WithIPv4Group0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_sun_nio_ch_Net_canUseIPv6OptionsWithIPv4LocalAddress0(JNIEnv *env,jclass cls) { return JNI_FALSE; }
JNIEXPORT jint JNICALL Java_sun_nio_ch_Net_isExclusiveBindAvailable(JNIEnv *env,jclass cls) { return -1; }
