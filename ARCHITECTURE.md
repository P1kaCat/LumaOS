# LumaOS Architecture

LumaOS is a small, experimental x86_64 operating system designed around a **gaming-first** philosophy. It is built from scratch with a freestanding C kernel and x86-64 assembly, booted through UEFI.

## High-Level Boot Flow

```text
UEFI / OVMF
    │
    ▼
BOOTX64.EFI
    │
    ├── Initialize framebuffer
    ├── Read UEFI memory map
    ├── Load kernel.elf
    ├── Parse ELF
    ├── Build handoff structure
    └── ExitBootServices()
    │
    ▼
Kernel @ 0x100000
    │
    ├── CPU tables
    ├── Paging
    ├── Physical page allocator
    ├── Kernel heap
    ├── Interrupts / timer / keyboard
    ├── Scheduler
    └── User processes
            │
            ▼
        Ring 3 shell
```

## Kernel

The kernel is a freestanding x86_64 binary linked at `0x100000`. It does not depend on a standard C library or hosted operating-system runtime.

Core responsibilities include CPU and interrupt setup, virtual memory, physical page allocation, the kernel heap, processes, scheduling, system calls, keyboard input and exception handling.

## CPU & Privilege Levels

LumaOS uses the x86_64 privilege model:

- **Ring 0** — kernel code
- **Ring 3** — user programs

The kernel uses the GDT, IDT, TSS, dedicated kernel stacks, `iretq` for privilege transitions and `int 0x80` for system calls.

The current user code runs with `CS = 0x1B` and `SS = 0x23`. The TSS provides the kernel stack used when entering Ring 0 from user mode.

## Memory Architecture

LumaOS currently uses 4-level x86_64 paging with a fixed-address kernel layout.

### User address space

```text
0x00800000  User code
0x00A00000  User stack region
0x00C00000  End of stack region
0x01000000  User heap
```

Kernel pages are supervisor-only. User pages are mapped as user-accessible.

Each user process owns its own page tables and CR3 value, providing process isolation.

## Physical Page Allocator

Physical memory is managed in 4 KB pages:

```c
alloc_page();
free_page(page);
count_free_pages();
```

The allocator supports page reuse and is validated by QEMU regression tests.

## Virtual Memory

Dynamic mappings are provided through:

```c
map_page(virtual, physical);
unmap_page(virtual);
```

Page table levels are allocated dynamically when required, and `invlpg` is used after mapping changes.

User page faults distinguish between:

1. **Not-present pages** — potentially valid lazy allocations.
2. **Protection violations** — invalid access that terminates the process.

## User Heap & Stack

The user heap starts at `0x1000000`. The `sbrk` syscall changes the heap break and physical pages are allocated lazily when touched.

The user stack occupies the `0xA00000–0xC00000` region and can grow through page faults.

When a user process terminates, its user pages and dynamically allocated page-table pages are released. Regression tests verify that free-page counts return to their expected values.

## Processes

A process contains state such as:

```text
PID
RSP
CR3
Kernel RSP
Task state
User/kernel flag
```

Processes can be created, scheduled and terminated independently. Kernel tasks and user processes are explicitly distinguished so identical numeric PIDs cannot terminate the wrong task.

## Scheduler

The scheduler is a preemptive round-robin scheduler driven by a 50 Hz PIT interrupt.

Task states:

```text
READY
RUNNING
SLEEPING
TERMINATED
```

Supported scheduling operations include preemption, `sleep(ticks)`, `yield()` and automatic wake-up of sleeping tasks. Context switching is implemented in `isr.S`.

## System Calls

The syscall ABI uses `int 0x80` with vector 128 and DPL 3.

| ID | Name | Purpose |
|---:|---|---|
| 0 | `write` | Write data to serial output |
| 1 | `exit` | Terminate the current process |
| 2 | `getpid` | Return the current PID |
| 3 | `sbrk` | Adjust the user heap break |
| 4 | `read` | Read keyboard input without blocking |
| 5 | `sleep` | Sleep for a number of timer ticks |
| 6 | `yield` | Yield the CPU |
| 7 | `getpages` | Return the number of free physical pages |

Unknown syscalls return `-1`.

## Keyboard Input

The keyboard driver uses IRQ1, Scancode Set 1, a 256-byte ring buffer, native French AZERTY mapping and Shift state tracking. The layout is implemented by LumaOS itself and is independent of the host Windows keyboard layout.

## Userland Shell

The current shell is a position-independent assembly program running in Ring 3.

```text
help
pid
mem
sleep N
exit
```

It handles interactive input, character echo, backspace, command parsing and numeric conversion.

## Source Layout

```text
boot/efi/       UEFI bootloader
include/        Shared kernel/bootloader definitions
kernel/         Kernel, scheduler, memory manager and userland
tools/ovmf/     UEFI firmware files
```

## Design Philosophy

LumaOS deliberately avoids becoming a traditional general-purpose desktop operating system.

The long-term architecture is centered around low overhead, fast startup, direct hardware control, controller-friendly interaction, native gaming APIs, a console-like user experience and a small, understandable system.

Every major subsystem should ultimately serve the gaming experience.
