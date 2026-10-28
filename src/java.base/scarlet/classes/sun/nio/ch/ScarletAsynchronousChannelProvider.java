/*
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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

package sun.nio.ch;

import java.nio.channels.AsynchronousChannelGroup;
import java.nio.channels.AsynchronousServerSocketChannel;
import java.nio.channels.AsynchronousSocketChannel;
import java.nio.channels.spi.AsynchronousChannelProvider;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.ThreadFactory;

/**
 * Scarlet has no asynchronous socket event port yet. Keep provider lookup
 * usable, and reject operations before allocating threads or descriptors.
 * Asynchronous files use SimpleAsynchronousFileChannelImpl independently.
 */
public class ScarletAsynchronousChannelProvider extends AsynchronousChannelProvider {
    public ScarletAsynchronousChannelProvider() { }

    private static UnsupportedOperationException unsupported() {
        return new UnsupportedOperationException(
            "Asynchronous socket channels are not supported on Scarlet");
    }

    @Override
    public AsynchronousChannelGroup openAsynchronousChannelGroup(
            int nThreads, ThreadFactory factory) {
        throw unsupported();
    }

    @Override
    public AsynchronousChannelGroup openAsynchronousChannelGroup(
            ExecutorService executor, int initialSize) {
        throw unsupported();
    }

    @Override
    public AsynchronousServerSocketChannel openAsynchronousServerSocketChannel(
            AsynchronousChannelGroup group) {
        throw unsupported();
    }

    @Override
    public AsynchronousSocketChannel openAsynchronousSocketChannel(
            AsynchronousChannelGroup group) {
        throw unsupported();
    }
}
