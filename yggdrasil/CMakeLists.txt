find_program(YGGDRASIL_CARGO cargo REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Build the Yggdrasil sidecar natively or disable LAUNCHER_BUILD_YGGDRASIL")
endif()
file(GLOB_RECURSE YGGDRASIL_SOURCES CONFIGURE_DEPENDS "crates/*.rs" "crates/Cargo.toml" "crates/*/Cargo.toml")
set(YGGDRASIL_BINARY "${CMAKE_CURRENT_BINARY_DIR}/yggdrasil-server${CMAKE_EXECUTABLE_SUFFIX}")
add_custom_command(OUTPUT "${YGGDRASIL_BINARY}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/build_sidecar.py"
        "${YGGDRASIL_CARGO}" "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml"
        "${CMAKE_CURRENT_BINARY_DIR}/cargo-target" release "${YGGDRASIL_BINARY}"
    DEPENDS ${YGGDRASIL_SOURCES} Cargo.toml Cargo.lock build_sidecar.py
    VERBATIM USES_TERMINAL)
add_custom_target(yggdrasil-server ALL DEPENDS "${YGGDRASIL_BINARY}")
install(PROGRAMS "${YGGDRASIL_BINARY}" DESTINATION "${BINARY_DEST_DIR}" COMPONENT Runtime)
