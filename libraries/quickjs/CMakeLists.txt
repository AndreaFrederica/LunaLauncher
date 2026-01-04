cmake_minimum_required(VERSION 3.13)
project(quickjs LANGUAGES C)

# Read version from .quickjs-version file (renamed to avoid conflict with C++20 <version> header)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/.quickjs-version" QUICKJS_VERSION)
string(STRIP "${QUICKJS_VERSION}" QUICKJS_VERSION)
# Filter out comment lines (starting with #)
string(REPLACE "
" ";" QUICKJS_VERSION_LINES "${QUICKJS_VERSION}")
set(QUICKJS_VERSION_CLEAN)
foreach(line IN LISTS QUICKJS_VERSION_LINES)
    string(STRIP "${line}" line_stripped)
    if(NOT line_stripped MATCHES "^#" AND NOT line_stripped STREQUAL "")
        set(QUICKJS_VERSION_CLEAN "${line_stripped}")
        break()
    endif()
endforeach()
if(QUICKJS_VERSION_CLEAN)
    set(QUICKJS_VERSION "${QUICKJS_VERSION_CLEAN}")
endif()

# QuickJS核心库 - 只包含源文件，头文件会自动处理
add_library(quickjs STATIC
    quickjs.c
    dtoa.c
    cutils.c
    libregexp.c
    libunicode.c
)

# QuickJS标准库（文件系统、网络等）
add_library(quickjs-libc STATIC
    quickjs-libc.c
)

target_link_libraries(quickjs-libc PUBLIC quickjs)

# 配置头文件包含路径
# 关键：使用 PUBLIC 使得链接 quickjs 的目标可以找到头文件
# 但这会导致整个目录被暴露，包括 version 文件
# 解决方案：重命名或移除会冲突的文件，或者创建单独的 include 目录
target_include_directories(quickjs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(quickjs-libc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# 为外部使用者创建一个interface库
add_library(quickjs-interface INTERFACE)
target_link_libraries(quickjs-interface INTERFACE quickjs quickjs-libc)

# 配置编译选项
if(WIN32)
    # Windows特定配置
    target_compile_definitions(quickjs PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _GNU_SOURCE
        __USE_MINGW_ANSI_STDIO
        CONFIG_VERSION="${QUICKJS_VERSION}"
    )
    target_compile_definitions(quickjs-libc PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _GNU_SOURCE
        __USE_MINGW_ANSI_STDIO
        CONFIG_VERSION="${QUICKJS_VERSION}"
    )
elseif(UNIX)
    # Unix/Linux特定配置
    target_compile_definitions(quickjs PRIVATE
        _GNU_SOURCE
        CONFIG_VERSION="${QUICKJS_VERSION}"
    )
    target_compile_definitions(quickjs-libc PRIVATE
        _GNU_SOURCE
        CONFIG_VERSION="${QUICKJS_VERSION}"
    )
    # 链接数学库和动态链接库
    target_link_libraries(quickjs PUBLIC m)
    if(NOT APPLE)
        target_link_libraries(quickjs PUBLIC dl pthread)
    endif()
endif()

# 配置C标准
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

set_target_properties(quickjs-libc PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
