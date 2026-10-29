/*
 * Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2013 SAP SE. All rights reserved.
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

package sun.nio.fs;

import java.io.IOException;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.channels.FileChannel;
import java.nio.channels.SeekableByteChannel;
import java.nio.file.FileStore;
import java.nio.file.OpenOption;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.FileAttribute;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.Set;
import java.util.concurrent.ExecutorService;

/**
 * Scarlet file system provider using the shared Unix implementation.
 */
class ScarletFileSystemProvider extends UnixFileSystemProvider {
    public ScarletFileSystemProvider() {
        super();
    }

    @Override
    ScarletFileSystem newFileSystem(String dir) {
        return new ScarletFileSystem(this, dir);
    }

    @Override
    public void createDirectory(Path path, FileAttribute<?>... attrs) throws IOException {
        // Native Scarlet currently creates directories with mode 0755 and has
        // no process umask. Select the platform default before entering the
        // shared Unix implementation; explicit attributes retain their checks.
        if (attrs.length == 0) {
            super.createDirectory(path, PosixFilePermissions.asFileAttribute(
                    PosixFilePermissions.fromString("rwxr-xr-x")));
        } else {
            super.createDirectory(path, attrs);
        }
    }

    private static void checkOpenOptions(Set<? extends OpenOption> options) {
        // Scarlet's libc has no synchronous-open or direct-I/O flags. Reject
        // these options before the Unix factory can create/truncate a file.
        for (OpenOption option : options) {
            if (option == StandardOpenOption.SYNC ||
                option == StandardOpenOption.DSYNC ||
                ExtendedOptions.DIRECT.matches(option)) {
                throw new UnsupportedOperationException(
                    option + " is not supported on Scarlet");
            }
        }
    }

    @Override
    public FileChannel newFileChannel(Path path,
                                      Set<? extends OpenOption> options,
                                      FileAttribute<?>... attrs) throws IOException {
        checkOpenOptions(options);
        return super.newFileChannel(path, options, attrs);
    }

    @Override
    public SeekableByteChannel newByteChannel(Path path,
                                              Set<? extends OpenOption> options,
                                              FileAttribute<?>... attrs) throws IOException {
        checkOpenOptions(options);
        return super.newByteChannel(path, options, attrs);
    }

    @Override
    public AsynchronousFileChannel newAsynchronousFileChannel(
            Path path, Set<? extends OpenOption> options,
            ExecutorService executor, FileAttribute<?>... attrs) throws IOException {
        checkOpenOptions(options);
        return super.newAsynchronousFileChannel(path, options, executor, attrs);
    }

    @Override
    FileStore getFileStore(UnixPath path) {
        // Do not invent a mount entry or report fabricated capacity/type data.
        throw new UnsupportedOperationException(
            "File store information is not supported on Scarlet");
    }
}
