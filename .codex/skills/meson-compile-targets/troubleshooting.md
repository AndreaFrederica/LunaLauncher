# Meson Compile Targets — Troubleshooting

## "No rule to make target"

**Symptom:** `ninja: error: unknown target 'myapp'`

**Cause:** Target name typo or not defined in meson.build

**Solution:**
```bash
# List available targets
meson introspect build/ --targets

# Check exact spelling
grep "executable\|library" meson.build | head

# If not there, target wasn't declared
```

## "Unresolved dependency"

**Symptom:** `ERROR: Dependency X not found during parse`

**Cause:** Linked library or subproject not available

**Solution:**
```bash
# Check if library is installed
pkg-config --list-all | grep mylib

# Or check for subproject
ls subprojects/mylib.wrap

# Reconfigure
meson setup build --reconfigure

# Or force fallback
meson setup build --reconfigure --force-fallback-for=mylib
```

## "Compilation failed"

**Symptom:** `error: identifier 'foo' not found`

**Cause:** Missing include, typo in header path, or header not installed

**Solution:**
```bash
# Check include_directories in meson.build
grep include_directories meson.build

# Verify header exists
find . -name "myheader.h"

# Try verbose compile to see -I flags
meson compile -C build -v | grep "\-I"

# Or check compiler error closely
meson compile -C build 2>&1 | head -20
```

## "Linking failed"

**Symptom:** `error: undefined reference to 'function_name'`

**Cause:** Function defined in library but not linked, or symbol not exported

**Solution:**
```bash
# Check what's linked
meson compile -C build -v | grep "\.a\|\.so" | head -5

# Identify the actual built artifact first
meson introspect build/ --targets

# Then inspect the library inside the build tree, not a system path
# Replace builddir/path/to/library.so with the target path from introspection
nm builddir/path/to/library.so | grep function_name

# Add missing library to meson.build
# dependencies: [zlib_dep, openssl_dep],  <- add here

# Reconfigure and rebuild
meson setup build --reconfigure
meson compile -C build
```

## "Stale build, not rebuilding"

**Symptom:** Changes to source don't trigger recompile

**Cause:** Ninja database corrupted or meson.build change not detected

**Solution:**
```bash
# Force rebuild
meson compile -C build --clean

# Unix only: remove ninja's incremental state to force a full rebuild on the next compile.
# Prefer meson compile --clean above; only use this when --clean alone is insufficient.
rm build/.ninja_state

# Or full reconfigure
meson setup build --reconfigure

# Or wipe and rebuild
meson setup build --wipe
```

## "Target built but not installed"

**Symptom:** `meson install` doesn't copy target

**Cause:** `install: true` not set, or install_dir wrong

**Solution:**

```bash
# Check if install is set
grep -A5 "executable\|library" meson.build | grep install
```

```meson
# Add to meson.build
myapp = executable('myapp', ..., install: true)

# Or specify install_dir
myapp = executable('myapp', ..., install: true, install_dir: 'bin')
```

```bash
# Reconfigure and reinstall
meson setup build --reconfigure
meson install -C build
```

## "Compile too slow"

**Symptom:** Takes minutes to compile small change

**Cause:** Linking is sequential, or parallel jobs not used

**Solution:**
```bash
# Use more parallel jobs
meson compile -C build -j 8

# Or check if rebuild is actually needed
ninja -C build -d explain myapp

# If unnecessary rebuild, check meson.build for always-update triggers
grep -i "always_build\|always_update" meson.build

# Disable if present (usually in custom_target)
```

## "Wrong optimization level"

**Symptom:** Binary is too slow or too large

**Cause:** Buildtype doesn't match intent (debug vs release)

**Solution:**
```bash
# Check current buildtype
meson introspect build/ --buildoptions | grep buildtype

# Reconfigure with release
meson setup build --buildtype=release
meson compile -C build

# Or check machine file
grep -i optimization meson.build
```

## "Symbol visibility issues"

**Symptom:** Library compiles but symbols not visible to other code

**Cause:** Symbols not exported (default hidden on some platforms)

**Solution:**

```bash
# Check visibility in header
grep __attribute__.*visibility myheader.h
```

```meson
# Or use -fvisibility=default
myapp = executable('myapp', ...,
  c_args: ['-fvisibility=default']
)

# For libraries, explicitly export
c_args: ['-DMYLIB_EXPORT=__attribute__((visibility("default")))']
```

## "Subproject target not found"

**Symptom:** `ERROR: Object subproj:libtarget has no method get_variable()`

**Cause:** Subproject target not declared with `get_variable()`

**Solution:**

```meson
# In subproject meson.build, declare as variable
mylib_dep = library('mylib', ...)

# Then in parent:
subproj = subproject('mylib')
mylib_dep = subproj.get_variable('mylib_dep')

# Not:
mylib_dep = subproj.get_variable('mylib')  # Wrong!
```

## "Cross-compilation target mismatch"

**Symptom:** Binary won't run on target: `wrong architecture`

**Cause:** Machine file cpu_family or cpu doesn't match target

**Solution:**
```bash
# Check what meson thinks the target is
meson introspect build/ --compilers

# Verify machine file
cat arm-linux.ini | grep cpu

# Verify output binary arch
file build/myapp
# Should say "ELF 32-bit LSB executable, ARM, EABI5"

# If wrong, reconfigure with correct machine file
meson setup build --cross-file correct-arm-linux.ini --reconfigure
```


