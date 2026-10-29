/*
 * Copyright (c) 2000, 2026, Oracle and/or its affiliates. All rights reserved.
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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <scarlet/network.h>
#include "net_util.h"
#include "java_net_NetworkInterface.h"

static jclass ni_class, binding_class;
static jmethodID ni_constructor, binding_constructor, address_bytes;
static jfieldID ni_name, ni_index, ni_display, ni_addrs, ni_bindings, ni_children;
static jfieldID binding_address, binding_broadcast, binding_prefix;

struct snapshot {
    struct scarlet_network_link_v1 *links;
    struct scarlet_network_ipv4_v2 *ips;
    ssize_t count, ip_count;
};
static void dispose(struct snapshot *s) { free(s->links); free(s->ips); }
static int collect(JNIEnv *env, struct snapshot *s) {
    memset(s,0,sizeof(*s));
    ssize_t count=scarlet_network_list_links_v1(NULL,0);
    if(count<0) goto failure;
    if(count==0) return 1;
    for(int attempt=0;attempt<8;attempt++) {
        if(count>INT_MAX-16) { JNU_ThrowOutOfMemoryError(env,"Native interface count exceeds Java limits");return 0; }
        free(s->links);s->links=calloc((size_t)count,sizeof(*s->links));
        if(s->links==NULL) goto memory;
        ssize_t actual=scarlet_network_list_links_v1(s->links,(size_t)count);
        if(actual<0) goto failure;
        if(actual>count) { count=actual;continue; }
        s->count=actual;
        size_t capacity=(size_t)count+16;
        s->ips=calloc(capacity,sizeof(*s->ips));
        if(s->ips==NULL) goto memory;
        s->ip_count=scarlet_network_list_ipv4_v2(s->ips,capacity);
        if(s->ip_count<0) goto failure;
        return 1;
    }
    JNU_ThrowByName(env,"java/net/SocketException","Network interfaces changed during enumeration");return 0;
 memory:
    JNU_ThrowOutOfMemoryError(env,"Native interface snapshot allocation failed");return 0;
 failure:
    JNU_ThrowByNameWithLastError(env,"java/net/SocketException","Native interface enumeration failed");return 0;
}
static struct scarlet_network_ipv4_v2 *configuration(struct snapshot *s,const char *name) {
    for(ssize_t i=0;i<s->ip_count;i++) if(strncmp(s->ips[i].name,name,32)==0) return &s->ips[i];
    return NULL;
}
static jobject ipv4(JNIEnv *env,const uint8_t bytes[4]) {
    jobject value=(*env)->NewObject(env,ia4_class,ia4_ctrID);CHECK_NULL_RETURN(value,NULL);
    uint32_t raw;memcpy(&raw,bytes,4);setInetAddress_addr(env,value,(jint)ntohl(raw));
    return (*env)->ExceptionCheck(env)?NULL:value;
}
JNIEXPORT void JNICALL Java_java_net_NetworkInterface_init(JNIEnv *env,jclass cls) {
    ni_class=(*env)->NewGlobalRef(env,cls);CHECK_NULL(ni_class);
    ni_constructor=(*env)->GetMethodID(env,cls,"<init>","()V");CHECK_NULL(ni_constructor);
#define FIELD(destination,name,type) destination=(*env)->GetFieldID(env,cls,name,type);CHECK_NULL(destination)
    FIELD(ni_name,"name","Ljava/lang/String;");FIELD(ni_index,"index","I");
    FIELD(ni_display,"displayName","Ljava/lang/String;");FIELD(ni_addrs,"addrs","[Ljava/net/InetAddress;");
    FIELD(ni_bindings,"bindings","[Ljava/net/InterfaceAddress;");FIELD(ni_children,"childs","[Ljava/net/NetworkInterface;");
#undef FIELD
    jclass binding=(*env)->FindClass(env,"java/net/InterfaceAddress");CHECK_NULL(binding);
    binding_class=(*env)->NewGlobalRef(env,binding);CHECK_NULL(binding_class);
    binding_constructor=(*env)->GetMethodID(env,binding,"<init>","()V");CHECK_NULL(binding_constructor);
    binding_address=(*env)->GetFieldID(env,binding,"address","Ljava/net/InetAddress;");CHECK_NULL(binding_address);
    binding_broadcast=(*env)->GetFieldID(env,binding,"broadcast","Ljava/net/Inet4Address;");CHECK_NULL(binding_broadcast);
    binding_prefix=(*env)->GetFieldID(env,binding,"maskLength","S");CHECK_NULL(binding_prefix);
    initInetAddressIDs(env);JNU_CHECK_EXCEPTION(env);
    address_bytes=(*env)->GetMethodID(env,ia_class,"getAddress","()[B");CHECK_NULL(address_bytes);
}
static jobject create(JNIEnv *env,struct snapshot *s,ssize_t i) {
    struct scarlet_network_link_v1 *link=&s->links[i];
    if(link->id==0 || link->id>INT_MAX) { JNU_ThrowByName(env,"java/net/SocketException","Native interface ID exceeds Java limits");return NULL; }
    jobject value=(*env)->NewObject(env,ni_class,ni_constructor);CHECK_NULL_RETURN(value,NULL);
    jstring name=JNU_NewStringPlatform(env,link->name);CHECK_NULL_RETURN(name,NULL);
    (*env)->SetObjectField(env,value,ni_name,name);(*env)->SetObjectField(env,value,ni_display,name);
    (*env)->DeleteLocalRef(env,name);(*env)->SetIntField(env,value,ni_index,(jint)link->id);
    struct scarlet_network_ipv4_v2 *ip=configuration(s,link->name);
    int count=ip!=NULL && ip->has_address;
    jobjectArray addresses=(*env)->NewObjectArray(env,count,ia_class,NULL);CHECK_NULL_RETURN(addresses,NULL);
    jobjectArray bindings=(*env)->NewObjectArray(env,count,binding_class,NULL);CHECK_NULL_RETURN(bindings,NULL);
    if(count) {
        jobject address=ipv4(env,ip->address);CHECK_NULL_RETURN(address,NULL);
        (*env)->SetObjectArrayElement(env,addresses,0,address);
        jobject item=(*env)->NewObject(env,binding_class,binding_constructor);CHECK_NULL_RETURN(item,NULL);
        (*env)->SetObjectField(env,item,binding_address,address);(*env)->DeleteLocalRef(env,address);
        uint8_t broadcast[4];unsigned prefix=0;int seen_zero=0;
        for(int n=0;n<4;n++) {
            broadcast[n]=ip->address[n]|(uint8_t)~ip->netmask[n];
            for(int bit=7;bit>=0;bit--) {
                if(ip->netmask[n] & (1u<<bit)) { if(seen_zero) { JNU_ThrowByName(env,"java/net/SocketException","Invalid native IPv4 netmask");return NULL; } prefix++; }
                else seen_zero=1;
            }
        }
        jobject bcast=ipv4(env,broadcast);CHECK_NULL_RETURN(bcast,NULL);
        (*env)->SetObjectField(env,item,binding_broadcast,bcast);(*env)->DeleteLocalRef(env,bcast);
        (*env)->SetShortField(env,item,binding_prefix,(jshort)prefix);
        (*env)->SetObjectArrayElement(env,bindings,0,item);(*env)->DeleteLocalRef(env,item);
    }
    jobjectArray children=(*env)->NewObjectArray(env,0,ni_class,NULL);CHECK_NULL_RETURN(children,NULL);
    (*env)->SetObjectField(env,value,ni_addrs,addresses);(*env)->DeleteLocalRef(env,addresses);
    (*env)->SetObjectField(env,value,ni_bindings,bindings);(*env)->DeleteLocalRef(env,bindings);
    (*env)->SetObjectField(env,value,ni_children,children);(*env)->DeleteLocalRef(env,children);
    return (*env)->ExceptionCheck(env)?NULL:value;
}
JNIEXPORT jobjectArray JNICALL Java_java_net_NetworkInterface_getAll(JNIEnv *env,jclass cls) {
    struct snapshot s;if(!collect(env,&s)) { dispose(&s);return NULL; }
    jobjectArray result=(*env)->NewObjectArray(env,(jsize)s.count,ni_class,NULL);
    if(result!=NULL) for(ssize_t i=0;i<s.count;i++) {
        jobject item=create(env,&s,i);if(item==NULL) { result=NULL;break; }
        (*env)->SetObjectArrayElement(env,result,(jsize)i,item);(*env)->DeleteLocalRef(env,item);
        if((*env)->ExceptionCheck(env)) {result=NULL;break;}
    }
    dispose(&s);return result;
}
static ssize_t find(struct snapshot *s,const char *name,jint index) {
    for(ssize_t i=0;i<s->count;i++)
        if((name==NULL || strncmp(s->links[i].name,name,32)==0) && (index<0 || s->links[i].id==(uint64_t)index)) return i;
    return -1;
}
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByName0(JNIEnv *env,jclass cls,jstring name) {
    const char *text=JNU_GetStringPlatformCharsStrict(env,name,NULL);CHECK_NULL_RETURN(text,NULL);
    struct snapshot s;jobject result=NULL;
    if(collect(env,&s)) {ssize_t i=find(&s,text,-1);if(i>=0)result=create(env,&s,i);}
    dispose(&s);JNU_ReleaseStringPlatformChars(env,name,text);return result;
}
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByIndex0(JNIEnv *env,jclass cls,jint index) {
    struct snapshot s;jobject result=NULL;
    if(collect(env,&s)) {ssize_t i=find(&s,NULL,index);if(i>=0)result=create(env,&s,i);}
    dispose(&s);return result;
}
JNIEXPORT jobject JNICALL Java_java_net_NetworkInterface_getByInetAddress0(JNIEnv *env,jclass cls,jobject address) {
    jbyteArray bytes=(*env)->CallObjectMethod(env,address,address_bytes);JNU_CHECK_EXCEPTION_RETURN(env,NULL);
    CHECK_NULL_RETURN(bytes,NULL);
    if((*env)->GetArrayLength(env,bytes)!=4) {(*env)->DeleteLocalRef(env,bytes);return NULL;}
    uint8_t raw[4];(*env)->GetByteArrayRegion(env,bytes,0,4,(jbyte*)raw);(*env)->DeleteLocalRef(env,bytes);JNU_CHECK_EXCEPTION_RETURN(env,NULL);
    struct snapshot s;jobject result=NULL;
    if(collect(env,&s)) for(ssize_t i=0;i<s.count;i++) {
        struct scarlet_network_ipv4_v2 *ip=configuration(&s,s.links[i].name);
        if(ip!=NULL && ip->has_address && memcmp(ip->address,raw,4)==0) {result=create(env,&s,i);break;}
    }
    dispose(&s);return result;
}
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_boundInetAddress0(JNIEnv *env,jclass cls,jobject address) {
    jobject result=Java_java_net_NetworkInterface_getByInetAddress0(env,cls,address);
    if(result==NULL) return JNI_FALSE;
    (*env)->DeleteLocalRef(env,result);return JNI_TRUE;
}
static int property(JNIEnv *env,jstring name,jint index,int which) {
    struct snapshot s;int result=-1;
    const char *text=JNU_GetStringPlatformCharsStrict(env,name,NULL);CHECK_NULL_RETURN(text,-1);
    if(collect(env,&s)) {
        ssize_t i=find(&s,text,index);
        if(i<0) JNU_ThrowByName(env,"java/net/SocketException","Network interface no longer exists");
        else if(which==0) result=s.links[i].state==SCARLET_NETWORK_LINK_UP;
        else if(which==1) result=(int)s.links[i].mtu;
        else result=0; // The Native registry contains Ethernet/Wi-Fi links, not loopback/P2P.
    }
    dispose(&s);JNU_ReleaseStringPlatformChars(env,name,text);return result;
}
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isUp0(JNIEnv *env,jclass cls,jstring name,jint index) { return property(env,name,index,0)>0; }
JNIEXPORT jint JNICALL Java_java_net_NetworkInterface_getMTU0(JNIEnv *env,jclass cls,jstring name,jint index) { return property(env,name,index,1); }
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isLoopback0(JNIEnv *env,jclass cls,jstring name,jint index) { return property(env,name,index,2)>0; }
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_isP2P0(JNIEnv *env,jclass cls,jstring name,jint index) { return property(env,name,index,2)>0; }
JNIEXPORT jboolean JNICALL Java_java_net_NetworkInterface_supportsMulticast0(JNIEnv *env,jclass cls,jstring name,jint index) {
    // Native multicast membership operations are not available in this backend.
    property(env,name,index,2);return JNI_FALSE;
}
JNIEXPORT jbyteArray JNICALL Java_java_net_NetworkInterface_getMacAddr0(JNIEnv *env,jclass cls,jbyteArray inaddr,jstring name,jint index) {
    struct snapshot s;jbyteArray result=NULL;
    const char *text=JNU_GetStringPlatformCharsStrict(env,name,NULL);CHECK_NULL_RETURN(text,NULL);
    if(collect(env,&s)) {
        ssize_t i=find(&s,text,index);
        if(i<0) JNU_ThrowByName(env,"java/net/SocketException","Network interface no longer exists");
        else {
            static const uint8_t zero[6]={0};
            if(memcmp(s.links[i].mac_address,zero,6)!=0) {
                result=(*env)->NewByteArray(env,6);
                if(result!=NULL)(*env)->SetByteArrayRegion(env,result,0,6,(jbyte*)s.links[i].mac_address);
            }
        }
    }
    dispose(&s);JNU_ReleaseStringPlatformChars(env,name,text);return result;
}
