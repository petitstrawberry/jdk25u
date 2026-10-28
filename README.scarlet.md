# OpenJDK 25 for Scarlet

This branch ports OpenJDK 25.0.5+8 to the Scarlet Native ABI on AArch64.
The upstream base is `fda231468e56663992785ec199a6efb8eca3cc7a` (tag
`jdk-25.0.5+8`). Keep upstream updates on `master`; maintain the native port on
`scarlet-25`.

The runtime supports the AArch64 HotSpot C1 JIT and the portable Zero interpreter,
Serial GC, a static JNI symbol registry, and the Scarlet C/C++ runtime. The
Scarlet build driver selects C1 by default; `--jvm-variant zero` retains Zero.
The target is `aarch64-unknown-scarlet`, with `SCARLET` HotSpot OS selection.

## Build and integration

The pinned toolchain, reproducible build driver, static JNI launcher, and guest
validation live in [Scarlet](https://github.com/petitstrawberry/Scarlet), under
`tools/openjdk/`. The Scarlet driver downloads an exact commit of this fork
and verifies its archive SHA-256. This repository is the source of truth for
the OpenJDK platform changes; no extra source overlay should be needed.

The kernel must supply `GetMemoryInfo` (53), `MemoryQuery` (703), and
`MemoryReadWord` (704), `NativeFaultRegister` (645), `NativeFaultReturn` (646),
and native pthread/futex, TLS, memory protection, and
file operations. `MemoryReadWord` supplies fault-tolerant scalar reads for
HotSpot without POSIX signals. C1 uses per-thread alternate-stack fault delivery
for implicit null checks, polling-page safepoints and stack guards. The kernel
preserves integer, FP/SIMD and TLS state and masks privileged return-state bits.
Protection violations must be rejected by lazy mapping, rather than remapped
with unchanged permissions. The port uses baseline AArch64 FP/Advanced SIMD;
optional CPU features remain disabled. Instruction-cache maintenance uses
CTR_EL0, DC CVAU, IC IVAU, DSB ISH and ISB, permitted by the Scarlet kernel.

## Current execution coverage

AArch64 QEMU guests with both HVF and TCG have executed the real JNI invocation path, `java -version`,
Java Hello, four platform threads with atomics and thread locals, 128 MiB of
transient allocations with Serial GC, and regular-file write/read across the
native 64 KiB I/O boundary. Guest process termination is checked separately
from test output. C1 compilation logs plus `-Xcomp`/`dontinline` validate actual
compilation of arithmetic, field-access and recursive methods; the application
checks results and recovery from NullPointerException and StackOverflowError.
Four concurrent compiled workers also retain live object/array roots across
eight full GCs, 32 null faults and four stack overflows.

This is an initial headless `java.base` port, not a TCK-certified JDK. Other
architectures, C2, virtual threads/continuations, reserved-stack annotations,
desktop modules, dynamically loaded JNI libraries, native
agents, PerfData, NUMA, large pages, OS thread priorities, POSIX signal handlers,
process creation, and native networking/NIO libraries are outside the validated
runtime. Unsupported port entry points return errors or throw exceptions.
Process RSS and per-page residency queries are unavailable; detailed NMT
residency is not supported. Disclaim is a no-op reclamation hint; uncommit
and release still discard pages. The SDK has no stack-protector runtime, so
HotSpot is compiled with -fno-stack-protector. The native C formatter does not
yet print floating-point GC timing fields.

The upstream OpenJDK licensing and copyright notices remain in place. See
`LICENSE`, `ADDITIONAL_LICENSE_INFO`, and the per-file headers.
