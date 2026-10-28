/*
 * Copyright (c) 1998, 2024, Oracle and/or its affiliates. All rights reserved.
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

/* Scarlet has a C locale and UTF-8 paths, independent of the build host.
 * As in OpenJDK's Unix C/POSIX locale mapping, Java's locale is en_US.
 * Java -D properties may override these platform defaults. */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>
#include "java_props.h"

static java_props_t props = {
    .os_name = "Scarlet", .os_version = "unknown", .os_arch = "aarch64",
    .tmp_dir = "/tmp", .file_separator = "/", .path_separator = ":",
    .line_separator = "\n", .user_name = "unknown", .user_home = "/",
    .format_language = "en", .display_language = "en",
    .format_country = "US", .display_country = "US",
    .encoding = "UTF-8", .sun_jnu_encoding = "UTF-8",
    .stdout_encoding = "UTF-8", .stderr_encoding = "UTF-8",
    .unicode_encoding = "UnicodeLittle", .cpu_endian = "little",
    .data_model = "64", .patch_level = "unknown"
};
static pthread_once_t props_once = PTHREAD_ONCE_INIT;
static char cwd[PATH_MAX];
static char passwd_buffer[4096];
static int cwd_error;

static void init_properties(void) {
    struct passwd entry, *result = NULL;
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cwd_error = errno;
        return;
    }
    props.user_dir = cwd;
    /* Resolve only the guest identity. No configure-time username/home. */
    if (getpwuid_r(getuid(), &entry, passwd_buffer, sizeof(passwd_buffer),
                   &result) == 0 && result != NULL) {
        if (result->pw_name != NULL) props.user_name = result->pw_name;
        if (result->pw_dir != NULL) props.user_home = result->pw_dir;
    }
}

java_props_t *GetJavaProperties(JNIEnv *env) {
    int error = pthread_once(&props_once, init_properties);
    if (error != 0 || cwd_error != 0) {
        errno = error != 0 ? error : cwd_error;
        JNU_ThrowByName(env, "java/lang/Error",
                       "Cannot determine the Scarlet working directory");
        return NULL;
    }
    return &props;
}

jstring GetStringPlatform(JNIEnv *env, nchar *str) {
    return JNU_NewStringPlatform(env, str);
}
