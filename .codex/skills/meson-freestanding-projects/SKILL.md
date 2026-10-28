---
name: meson-freestanding-projects
description: Build freestanding, bare-metal, kernel, and OSDev projects with Meson. Machine files for i686-elf, riscv64-unknown-elf, ARM Cortex-M; -ffreestanding/-nostdlib flags; kernel.elf executables; linker scripts; GRUB/Multiboot; custom ELF sections; freestanding C library substitution with picolibc/newlib.
---

# Meson Freestanding Projects

Freestanding projects (kernels, bootloaders, embedded bare-metal) have no operating system. Meson supports these through custom machine files, compiler flags, and linker scripts.

> For standard OS-backed targets (Linux, Windows, macOS), use the `meson-cross-compilation` skill. This skill is for `system = 'none'` targets only.

## Machine File Presets

All freestanding machine files set `system = 'none'` in `[host_machine]` and use `[built-in options]` for compiler/linker args.

### i686-elf (32-bit x86)

```ini
[host_machine]
system = 'none'
cpu_family = 'x86'
cpu = 'i686'
endian = 'little'

[binaries]
c = 'i686-elf-gcc'
cpp = 'i686-elf-g++'
ar = 'i686-elf-ar'
c_ld = 'i686-elf-ld'

[built-in options]
c_args = ['-ffreestanding', '-fno-pie', '-O2']
c_link_args = ['-nostdlib', '-Wl,--gc-sections', '-Wl,-n']
```

### riscv64-unknown-elf (RISC-V 64-bit)

```ini
[host_machine]
system = 'none'
cpu_family = 'riscv64'
cpu = 'riscv64'
endian = 'little'

[binaries]
c = 'riscv64-unknown-elf-gcc'
cpp = 'riscv64-unknown-elf-g++'
ar = 'riscv64-unknown-elf-ar'

[built-in options]
c_args = ['-ffreestanding', '-march=rv64i', '-mabi=lp64']
c_link_args = ['-nostdlib', '-Wl,--gc-sections']
```

### ARM Cortex-M4 (bare-metal embedded)

```ini
[host_machine]
system = 'none'
cpu_family = 'arm'
cpu = 'cortex-m4'
endian = 'little'

[binaries]
c = 'arm-none-eabi-gcc'
cpp = 'arm-none-eabi-g++'
ar = 'arm-none-eabi-ar'

[built-in options]
c_args = ['-ffreestanding', '-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv4-sp-d16']
c_link_args = ['-nostdlib', '-Wl,--gc-sections', '-Wl,-T,linker.ld']
```

A complete aarch64-elf preset is in [reference.md](reference.md).

## Building a Kernel Executable

```meson
kernel = executable('kernel.elf',
  'src/entry.s',          # Assembly entry point (_start)
  'src/main.c',
  c_args: ['-ffreestanding', '-O2', '-mgeneral-regs-only'],
  link_args: ['-nostdlib', '-Wl,-T,' + meson.current_source_dir() + '/linker.ld'],
  name_suffix: 'elf',
  install: true,
)
```

Key points:
- `-ffreestanding` on all C sources; `-nostdlib` at link time
- Entry point assembly file listed first (link order)
- Linker script path via `meson.current_source_dir()`

## Linker Script Integration

Reference the script in meson.build:

```meson
link_args: ['-Wl,-T,' + meson.current_source_dir() + '/arch/linker.ld']
```

Minimal linker script for a Multiboot kernel:

```ld
ENTRY(_start)

SECTIONS {
    . = 1M;               /* Conventional Multiboot load address */

    .multiboot : {
        KEEP(*(.multiboot))   /* Multiboot header (must be within first 8 KB) */
    }

    .text : {
        *(.text)
    }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(COMMON) *(.bss) }
}
```

For the full linker script with `init_array`/`fini_array`, see [reference.md](reference.md).

## GRUB / Multiboot ISO

Place the Multiboot header in a `.multiboot` section in assembly (must be within first 8 KB of the binary):

```asm
.section .multiboot
.long 0x1BADB002       # Magic (Multiboot 1)
.long 0x00             # Flags (0 = no page-alignment or memory-map requests)
.long -(0x1BADB002 + 0x00)  # Checksum (magic + flags + checksum == 0)
```

Build a bootable ISO with chained `custom_target`s. Meson does not invoke a shell for `custom_target` commands, so multi-step operations must be split across separate targets rather than joined with `&&`:

```meson
exe_cp = find_program('cp')

# Stage grub.cfg into isodir/boot/grub/ inside the build tree.
# TODO: In Meson 1.12+, replace with fs.copyfile(... build_subdir: ...)
grub_cfg = custom_target(
  'grub_cfg',
  input: 'boot/grub/grub.cfg',
  output: 'grub.cfg',
  build_subdir: 'isodir/boot/grub',
  command: [exe_cp, '@INPUT@', '@OUTPUT@'],
)

# Stage kernel.elf into isodir/boot/ inside the build tree.
# TODO: In Meson 1.12+, replace with fs.copyfile(... build_subdir: ...)
kernel_copy = custom_target(
  'kernel_copy',
  input: kernel,
  output: 'kernel.elf',
  build_subdir: 'isodir/boot',
  command: [exe_cp, '@INPUT@', '@OUTPUT@'],
)

exe_grub_mkrescue = find_program('grub-mkrescue')
build_iso = custom_target(
  'iso',
  input: [grub_cfg, kernel_copy],
  output: 'kernel.iso',
  command: [exe_grub_mkrescue, '-o', '@OUTPUT@', '@BUILD_ROOT@/isodir'],
  build_by_default: true,
)
```

The two staging targets depend on `kernel` implicitly through `kernel_copy`'s `input:`, so `build_iso` will not run until both the kernel and the GRUB config are in place.

## Custom ELF Sections

Declare custom sections for kernel features:

```c
// Constructor table entry
__attribute__((section(".init_array")))
void (*__init_array_start)(void) = (void*)0;

// Page-aligned stack
__attribute__((section(".bss"), aligned(4096)))
char kernel_stack[16384];
```

Common kernel sections: `.multiboot`, `.init_array`/`.fini_array` (constructors/destructors), `.ctors`/`.dtors` (legacy), `.got`, `.stack`.

## Freestanding C Library

Freestanding targets have no libc. Picolibc is recommended (lightweight, Meson-native):

```ini
# subprojects/picolibc.wrap
[wrap-git]
url = https://github.com/picolibc/picolibc.git
revision = main
[provide]
picolibc = picolibc_lib
```

```meson
picolibc_dep = subproject('picolibc').get_variable('picolibc_lib')

kernel = executable('kernel.elf', 'main.c',
  dependencies: picolibc_dep,
  link_args: ['-nostdlib'],
)
```

Declare the C stdlib choice in the machine file via `[properties]`:

```ini
[properties]
freestanding_c_stdlib = 'picolibc'  # or 'newlib', 'none'
```

## Build Configuration

Export kernel parameters via `configure_file`:

```meson
config = configuration_data()
config.set('KERNEL_BASE', 0xC0000000)
config.set('KERNEL_STACK_SIZE', 16384)
configure_file(input: 'config.h.in', output: 'config.h', configuration: config)
```

## Compile and Debug

```bash
# Build
meson compile -C build
# Inspect ELF sections
readelf -S build/kernel.elf
# Disassemble
objdump -d build/kernel.elf | head -30
# Test in emulator
qemu-system-i386 -kernel build/kernel.elf
```

See [reference.md](reference.md) for the complete machine file reference, C flag table, full linker script, and grub.cfg setup. See [troubleshooting.md](troubleshooting.md) for common freestanding build issues.


## Language Standard Selection for Freestanding Targets

Freestanding toolchains (e.g. `i686-elf-gcc`) support the same `c_std` / `cpp_std` options as hosted compilers. Set them in the machine file's `[built-in options]` alongside the freestanding flags:

```ini
[built-in options]
c_std = 'c17'
cpp_std = 'c++20'
c_args = ['-ffreestanding', '-fno-pie', '-O2']
c_link_args = ['-nostdlib', '-Wl,--gc-sections']
```

For kernel code that must avoid compiler-generated runtime calls (e.g. `memcpy` inlining), consider `c_std=gnu11` or `gnu17` if your toolchain's GNU extensions are needed and the kernel provides the required helpers. When in doubt, use the standard values that match the toolchain's default (commonly `c17` / `c++20` for modern `-elf` toolchains).
