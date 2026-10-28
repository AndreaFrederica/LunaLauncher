# Windows and MSVC

> **Note:** This is a **native/MSVC-ABI** example, not a cross-compilation recipe. It builds for the host platform using the Microsoft Visual C++ toolchain and lives here as a contrast case for Windows toolchain setup. For actual cross-compilation examples, see `examples/cross-compilation/`.

Shows a Windows-friendly DLL export pattern and a consumer executable.

On Windows, set up the compiler environment before configuring the build. On Visual Studio builds, Meson can use the VS backend or a native Ninja backend depending on the workflow.
