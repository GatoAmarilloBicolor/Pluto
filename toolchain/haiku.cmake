# Haiku cross-compilation toolchain for macOS/Linux
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/haiku.cmake -B build

set(CMAKE_SYSTEM_NAME Haiku)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compiler (adjust path to your Haiku cross-compiler)
set(CMAKE_C_COMPILER x86_64-unknown-haiku-gcc)
set(CMAKE_CXX_COMPILER x86_64-unknown-haiku-g++)

# Sysroot - path to Haiku SDK
set(CMAKE_SYSROOT /opt/haiku-sdk)

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config for cross-compilation
set(PKG_CONFIG_EXECUTABLE ${CMAKE_SYSROOT}/bin/pkg-config)
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
set(ENV{PKG_CONFIG_LIBDIR} ${CMAKE_SYSROOT}/boot/system/lib/pkgconfig:${CMAKE_SYSROOT}/boot/system/develop/lib/pkgconfig)

# Flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__HAIKU__")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SYSROOT}/boot/system/lib")

# Resource compiler
set(CMAKE_RC_COMPILER ${CMAKE_SYSROOT}/bin/rc)