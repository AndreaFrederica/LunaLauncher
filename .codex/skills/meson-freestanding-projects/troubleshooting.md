# Meson Freestanding Projects — Troubleshooting

## `Compiler not found` for freestanding target

**Solution:** The cross-compiler is not installed. Build with crosstool-NG or use a prebuilt toolchain.

```bash
ct-ng i686-elf
ct-ng build
# Prebuilt: https://github.com/lordmilko/i686-elf-tools
```

For ARM: `apt install gcc-arm-none-eabi`.

## `Undefined reference to '_start'`

**Solution:** Entry point symbol is missing or link order is wrong.

```bash
# Check ENTRY() directive in linker script
grep ENTRY linker.ld
# Ensure entry assembly file is listed first in executable()
# executable('kernel.elf', 'src/entry.s', 'src/main.c', ...)
```

## `Multiple definition of _start`

**Solution:** Compiler provides default CRT files. Add `-nostartfiles` to link args.

```ini
[built-in options]
c_link_args = ['-nostdlib', '-nostartfiles', '-Wl,--gc-sections']
```

## `Undefined reference` to libc functions (memcpy, memset, printf)

**Solution:** Freestanding targets have no libc. Provide your own or use picolibc.

```meson
picolibc_dep = subproject('picolibc').get_variable('picolibc_lib')
kernel = executable('kernel.elf', ..., dependencies: picolibc_dep)
```

## `Cannot open linker script`

**Solution:** Use an absolute path via `meson.current_source_dir()`.

```meson
link_args: ['-Wl,-T,' + meson.current_source_dir() + '/linker.ld']
# NOT: link_args: ['-Wl,-T,linker.ld']  (relative to build dir)
```

## ELF file not bootable (GRUB/QEMU won't load it)

**Solution:** Multiboot header missing or not within the first 8 KB of the binary.

```bash
# Check header presence and offset
readelf -S build/kernel.elf | grep multiboot
objdump -d build/kernel.elf | head -20
```

Ensure `.multiboot` is the first section in the linker script:

```ld
SECTIONS {
    . = 1M;
    .multiboot : { KEEP(*(.multiboot)) }   # MUST be first and retained
    .text : { *(.text) }
    ...
}
```

## Linker discards needed sections

**Solution:** `--gc-sections` removes sections not explicitly kept. Wrap critical sections with `KEEP()`.

```ld
.multiboot : { KEEP(*(.multiboot)) }
.init_array : {
    __init_array_start = .;
    KEEP(*(.init_array))
    __init_array_end = .;
}
```

## Page fault immediately after boot

**Solution:** Memory layout mismatch. Check the kernel link address and ensure valid stack before first C call.

```bash
readelf -l build/kernel.elf | grep LOAD
# Verify identity mapping of lower memory for early boot
# Ensure stack pointer is valid before jumping to main()
```

## Code works in QEMU but crashes on real hardware

**Solution:** Hardware-specific initialization differs (APIC vs PIC, HPET vs PIT, PCI enumeration).

```bash
# Try different QEMU machine models
qemu-system-i386 -machine pc -kernel build/kernel.elf
qemu-system-i386 -machine q35 -kernel build/kernel.elf
```

## GRUB can't find kernel on ISO

**Solution:** ISO directory structure is wrong.

```bash
# Expected layout:
# iso/boot/kernel.elf
# iso/boot/grub/grub.cfg
isoinfo -l -i kernel.iso
```

## Subproject picolibc fails to build

**Solution:** Picolibc may need a native file for its code generators.

```bash
meson setup build \
  --cross-file i686-elf.ini \
  --native-file native.ini
```

Or build picolibc separately and link as an external library.

## Custom target for ISO is stale after kernel changes

**Solution:** Ensure the custom target's `input:` references the kernel executable.

```meson
custom_target('kernel.iso',
  input: kernel,          # Depend on kernel target
  output: 'kernel.iso',
  command: [...],
  build_by_default: true,
)
```

## Section not placed in expected region

**Solution:** The linker script doesn't cover all compiler-emitted sections. Add wildcard patterns.

```ld
.text : { *(.text) *(.text.*) }
.rodata : { *(.rodata) *(.rodata.*) }
```

See what sections the compiler produces:

```bash
readelf -S build/kernel.elf
```


## Language Standard Mismatch for Freestanding Target

Freestanding toolchains may not support all hosted-mode `c_std` values. If the compiler rejects a standard flag, check what the toolchain's GCC defaults to (`gcc -v` or the `--std=` flags it actually implements) and set `c_std` / `cpp_std` accordingly in the machine file's `[built-in options]`. Modern `-elf` toolchains commonly default to `c17` / `c++20`.
