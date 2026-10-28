/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "jni.h"
#include "osSupport.hpp"

/**
 * Open a regular file read-only.
 * Return the file descriptor.
 */
jint osSupport::openReadOnly(const char *path) {
    return ::open(path, 0);
}

/**
 * Close a file descriptor.
 */
jint osSupport::close(jint fd) {
    return ::close(fd);
}

/**
 * Return the size of a regular file.
 */
jlong osSupport::size(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) < 0 ||
            (statbuf.st_mode & S_IFREG) != S_IFREG) {
        return -1;
    }
    return (jsize) statbuf.st_size;
}

/**
 * Read nBytes at offset into a buffer.
 */
jlong osSupport::read(jint fd, char *buf, jlong nBytes, jlong offset) {
    // Scarlet bounds one native read to 64 KiB. libjimage's read_at expects
    // the complete regular-file range, so assemble short POSIX reads here.
    jlong done = 0;
    while (done < nBytes) {
        ssize_t result = ::pread(fd, buf + done, (size_t)(nBytes - done), offset + done);
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) return done ? done : -1;
        if (result == 0) break;
        done += result;
    }
    return done;
}

/**
 * Map nBytes at offset into memory and return the address.
 * The system chooses the address.
 */
void* osSupport::map_memory(int fd, const char *filename, size_t file_offset, size_t bytes) {
    // The runtime image is immutable. Prefer a private file mapping; some
    // Scarlet filesystems (including initramfs) cannot map file handles yet.
    void* mapped = mmap(NULL, bytes, PROT_READ, MAP_PRIVATE, fd, file_offset);
    if (mapped != MAP_FAILED) return mapped;
    mapped = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return NULL;
    size_t done = 0;
    while (done < bytes) {
        ssize_t result = pread(fd, (char*)mapped + done, bytes - done, file_offset + done);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) { munmap(mapped, bytes); return NULL; }
        done += (size_t)result;
    }
    if (mprotect(mapped, bytes, PROT_READ) != 0) {
        munmap(mapped, bytes);
        return NULL;
    }
    return mapped;
}

/**
 * Unmap nBytes of memory at address.
 */
int osSupport::unmap_memory(void *addr, size_t bytes) {
    return munmap((char *) addr, bytes) == 0;
}

/**
 * A CriticalSection to protect a small section of code.
 */
void SimpleCriticalSection::enter() {
    pthread_mutex_lock(&mutex);
}

void SimpleCriticalSection::exit() {
    pthread_mutex_unlock(&mutex);

}

SimpleCriticalSection::SimpleCriticalSection() {
    pthread_mutex_init(&mutex, NULL);
}

//SimpleCriticalSection::~SimpleCriticalSection() {
//    pthread_mutex_destroy(&mutex);
//}
