# Meson Freestanding Projects — Reference

## Machine File Structure for `system = 'none'`

### `[host_machine]`

```ini
[host_machine]
system = 'none'
cpu_family = 'x86'       # x86, x86_64, arm, aarch64, riscv32, riscv64
cpu = 'i686'             # Architecture variant
endian = 'little'
```

### `[binaries]`

Freestanding toolchains use `-elf` or `-none-eabi` triplets:

```ini
[binaries]
c = 'i686-elf-gcc'
cpp = 'i686-elf-g++'
ar = 'i686-elf-ar'
c_ld = 'i686-elf-ld'
strip = 'i686-elf-strip'
objcopy = 'i686-elf-objcopy'
```

### `[built-in options]`

Standard freestanding flags — use this section, not `[properties]`, for compiler/linker args:

```ini
[built-in options]
c_args = ['-ffreestanding', '-fno-pie', '-O2']
c_link_args = ['-nostdlib', '-Wl,--gc-sections', '-Wl,-n']
c_std = 'c17'
cpp_std = 'c++20'
```

### `[properties]`

Use only for custom metadata and C stdlib declaration:

```ini
[properties]
freestanding_c_stdlib = 'picolibc'   # or 'newlib', 'none'
kernel_base = 0xC0000000
page_size = 4096
```

Access in meson.build:

```meson
c_stdlib = meson.get_external_property('freestanding_c_stdlib', 'none')
if c_stdlib == 'picolibc'
  picolibc_dep = subproject('picolibc').get_variable('picolibc_lib')
endif
```

## Toolchain Presets

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

### arm-none-eabi (ARM Cortex-M4)

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
c_link_args = ['-nostdlib', '-Wl,--gc-sections', '-T', 'linker.ld']
```

### aarch64-elf (ARM 64-bit bare-metal)

```ini
[host_machine]
system = 'none'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[binaries]
c = 'aarch64-elf-gcc'
cpp = 'aarch64-elf-g++'
ar = 'aarch64-elf-ar'

[built-in options]
c_args = ['-ffreestanding', '-O2']
c_link_args = ['-nostdlib', '-Wl,--gc-sections']
```

## Detecting Freestanding Build in Code

```meson
if host_machine.system() == 'none'
  message('Building for bare-metal/freestanding target')
  add_project_arguments('-DKERNEL', language: 'c')
endif
```

## Compiler Flag Reference

| Flag | Purpose | Required? |
|------|---------|-----------|
| `-ffreestanding` | Disable hosted libc, enable freestanding mode | Yes |
| `-nostdlib` | Do not link standard startup or libc | Yes (link) |
| `-fno-pie` | No position-independent executable | Often |
| `-fno-pic` | No position-independent code | Often |
| `-fno-stack-protector` | Disable stack protection (no libc) | Common |
| `-fno-builtin` | Don't recognize built-in functions | Optional |
| `-mgeneral-regs-only` | Don't use SIMD/FP registers (kernel safety) | Kernel safety |
| `-Wl,--gc-sections` | Discard unused sections | Recommended |
| `-Wl,-n` | Set text segment default (no page align) | Sometimes |
| `-Wl,-T,linker.ld` | Custom linker script | Usually |

## Linker Script Structure

Complete linker script for a Multiboot kernel with constructor/destructor support:

```ld
ENTRY(_start)

SECTIONS {
    . = 1M;  /* Load address */

    .multiboot : {
        KEEP(*(.multiboot))
    }

    .text : {
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(COMMON)
        *(.bss)
    }

    .init_array : {
        __init_array_start = .;
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    .fini_array : {
        __fini_array_start = .;
        KEEP(*(.fini_array))
        __fini_array_end = .;
    }
}
```

## Section Attributes

```c
/* Multiboot header */
__attribute__((section(".multiboot")))
const unsigned multiboot_header[] = { 0x1BADB002, 0x00, -(0x1BADB002 + 0x00) };

/* Constructor table entry */
__attribute__((section(".init_array")))
void (*init_func)(void) = &my_init;

/* Page-aligned BSS section */
__attribute__((section(".bss"), aligned(4096)))
char kernel_stack[16384];
```

## Installing Freestanding Toolchains

```bash
# i686-elf — build from source via crosstool-NG or use prebuilt tools
# riscv64-unknown-elf — apt install gcc-riscv64-unknown-elf (some distros)
# arm-none-eabi — apt install gcc-arm-none-eabi
# aarch64-elf — apt install gcc-aarch64-linux-gnu (Linux-targeting; bare-metal needs source build)
```

## Building ISO Images

Meson does not invoke a shell for `custom_target` commands, so multi-step operations must be split into separate targets rather than joined with `&&`. Stage the ISO directory tree first, then invoke `grub-mkrescue` on the staged root:

```meson
# In meson.build
kernel_elf = executable('kernel.elf', ...)

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
  input: kernel_elf,
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

GRUB config (`boot/grub/grub.cfg`):
```cfg
set timeout=5
set default=0
menuentry "My Kernel" {
    multiboot /boot/kernel.elf
    boot
}
```

Verify ISO contents:
```bash
isoinfo -l -i build/kernel.iso
# Should show /boot/kernel.elf and /boot/grub/grub.cfg
```


## Language Standard Selection for Freestanding Targets

Freestanding toolchains (e.g. `i686-elf-gcc`) support the same `c_std` / `cpp_std` options as hosted compilers. Set them in the machine file's `[built-in options]` alongside the freestanding flags. The presets above use `c_std=c17` and `cpp_std=c++20` — match these to your toolchain's default unless the project requires GNU extensions for low-level operations.
