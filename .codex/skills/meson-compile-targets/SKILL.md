---
name: meson-compile-targets
description: Compile Meson projects with target selection, parallel jobs, and backend-specific arguments. Use this skill whenever the user wants to build, compile, or speed up a Meson project — even if they just say "how do I build only X", "my build is slow", "build a specific target", or ask about ninja flags, parallel jobs, or verbose compiler output.
---

# Meson Compile & Building Targets

`meson compile` command builds targets using the configured backend (ninja, Visual Studio, Xcode). It supports target selection, parallelism, verbosity control, and backend-specific flags.

## Quick Start

**Build everything:**
```bash
meson compile -C build
```

**Build specific target:**
```bash
meson compile -C build target_name
meson compile -C build path/to/target:executable
```

**Use all CPU cores:**
```bash
meson compile -C build -j 0  # 0 = detect CPU count
meson compile -C build -j 8  # 8 parallel jobs
```

**Verbose output (show commands):**
```bash
meson compile -C build -v
```

**Clean before rebuild:**
```bash
meson compile -C build --clean
```

## meson compile: Reference

### Basic Syntax

```bash
meson compile [options] [targets...]
```

### Common Options

| Option | Purpose | Example |
|--------|---------|---------|
| `-C BUILDDIR` | Build directory | `-C build` |
| `-j JOBS` | Parallel jobs (0=detect CPU) | `-j 0` or `-j 4` |
| `-l LOAD` | Target system load average | `-l 2.0` |
| `--clean` | Clean build directory first | `--clean` |
| `-v, --verbose` | Show full build commands | `-v` |
| `--ninja-args ARGS` | Pass arguments to ninja | `--ninja-args="-k 0"` |
| `--vs-args ARGS` | Pass arguments to msbuild | `--vs-args="/p:Configuration=Release"` |
| `--xcode-args ARGS` | Pass arguments to xcodebuild | `--xcode-args="-quiet"` |

### Target Selection

Targets can be specified by name or path:

```bash
# Build specific executable
meson compile -C build myexe

# Build specific library
meson compile -C build mylib

# Build with path (useful if names conflict)
meson compile -C build src/bin/myexe:executable
meson compile -C build src/lib/mylib:shared_library

# Build multiple targets
meson compile -C build myexe mylib

# Build all (default)
meson compile -C build
```

#### Target Naming Convention

Meson target selectors are usually `PATH/NAME.SUFFIX[:TYPE]`, where `:TYPE` is optional.
- `:executable` — executable
- `:shared_library` — shared lib (.so, .dll, .dylib)
- `:static_library` — static lib (.a, .lib)
- `:custom` — custom target
- `:run` — run target (like "make run")

Query available targets:
```bash
meson introspect build/ --targets
```

### Parallelism

| Option | Behavior |
|--------|---------|
| `-j 0` | Auto-detect CPU cores (recommended) |
| `-j 1` | Sequential (slow, useful for debugging) |
| `-j 4` | 4 parallel jobs |
| `-l 2.0` | Keep system load average ≤ 2.0 |

Default: Backend's default parallelism (ninja auto-detects, VS uses all cores by default).

```bash
# Fast build on modern machine
meson compile -C build -j 0

# Slow sequential build (good for debugging linker errors)
meson compile -C build -j 1

# Specific job count
meson compile -C build -j 4
```

### Verbosity & Debugging

| Option | Shows |
|--------|------|
| (default) | Summary: [X/Y] Compiling, [X/Y] Linking, etc. |
| `-v` | Full compiler and linker commands |
| `--ninja-args="-d explain"` | Why tasks are rebuilt (ninja only) |
| `--ninja-args="-d stats"` | Build statistics (ninja only) |
| `--ninja-args="-k 0"` | Keep going on first error, compile all (ninja only) |

Examples:

```bash
# See all compiler invocations
meson compile -C build -v

# Understand why things are rebuilding (ninja)
meson compile -C build --ninja-args="-d explain"

# Don't stop on first error (compile as much as possible)
meson compile -C build --ninja-args="-k 0"

# Show build statistics
meson compile -C build --ninja-args="-d stats"
```

### Clean Rebuild

```bash
# Clean and rebuild (fastest clean-rebuild method)
meson compile -C build --clean
meson compile -C build

# Full wipe and reconfigure
rm -rf build && meson setup build && meson compile -C build
```

## Backend-Specific Arguments

### Ninja (default backend)

```bash
# Fail fast, then show summary
meson compile -C build --ninja-args="-k 1"

# Keep going despite errors
meson compile -C build --ninja-args="-k 0"

# Show why rules are executing
meson compile -C build --ninja-args="-d explain"

# Build statistics
meson compile -C build --ninja-args="-d stats"

# Reduce parallelism dynamically
meson compile -C build -j 4 --ninja-args="-l 4.0"
```

### Visual Studio (--backend=vs)

```bash
# Release configuration
meson compile -C build --vs-args="/p:Configuration=Release"

# Quiet output
meson compile -C build --vs-args="/v:quiet"

# Use 8 parallel jobs
meson compile -C build --vs-args="/m:8"
```

### Xcode (--backend=xcode)

```bash
# Quiet output
meson compile -C build --xcode-args="-quiet"

# Specific configuration
meson compile -C build --xcode-args="-configuration Release"
```

## Finding Target Names

```bash
# List all targets
meson introspect build/ --targets

# Grep for specific target
meson introspect build/ --targets | grep mylib
```

Example output:
```
myexe (executable)
mylib (shared_library)
mylib_static (static_library)
run_tests (run)
iso_generator (custom)
```

## Performance Tips

| Tip | Command | Benefit |
|-----|---------|---------|
| Use all cores | `-j 0` | Fastest builds |
| Load limiting | `-l 4.0` | Don't overwhelm system |
| Incremental | (default) | Only rebuild changed deps |
| Verbose on error | `-v` when debugging | See actual compiler errors |

## Workflow: Fast Iteration

```bash
# Initial build (one-time, slow)
meson setup build
meson compile -C build -j 0

# Fast rebuilds after edits
meson compile -C build -j 0

# Rebuild one target only
meson compile -C build myexe

# Full rebuild if something's wrong
meson compile -C build --clean && meson compile -C build -j 0

# Debug a linker error
meson compile -C build -j 1 -v
```

See [reference.md](reference.md) for exhaustive command reference.

See [troubleshooting.md](troubleshooting.md) for build failure diagnosis. For systematic diagnosis spanning multiple areas, see the `meson-debugging-troubleshooting` skill.

Runnable examples live under this skill's `examples/` directory.


## Common mistakes

- confusing a target name with a source file path
- rebuilding the whole tree when only one target is needed
- using `-j` as a substitute for fixing dependency bottlenecks
- assuming backend-specific flags behave identically across Ninja, Visual Studio, and Xcode
- treating `-v` as a performance tool instead of a debugging tool


