cmake_minimum_required(VERSION 3.5)

# set(SYSROOT_PATH "/home/hlb194802/.local/riscv64-toolchain/sysroots")

if(NOT DEFINED ENV{SYSROOT_PATH})
    message(FATAL_ERROR "not defined environment variable:SYSROOT_PATH please `source env.sh`")  
else()
    message($ENV{SYSROOT_PATH})
endif()
message($ENV{SYSROOT_PATH})
set(SYSROOT "$ENV{SYSROOT_PATH}/riscv64-oe-linux")

set(CMAKE_C_COMPILER   "$ENV{SYSROOT_PATH}/x86_64-oesdk-linux/usr/bin/riscv64-linux-gcc")
set(CMAKE_CXX_COMPILER "$ENV{SYSROOT_PATH}/x86_64-oesdk-linux/usr/bin/riscv64-linux-g++")

# 设置 SYSROOT
set(CMAKE_SYSROOT "${SYSROOT}")

# 设置 PKG 环境变量
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/pkgconfig")
set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib:${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS)    # remove -rdynamic

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format -g -O2 -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-format -g -O2 -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-section")

# 使用 PKGCONFIG 查找 gio-2.0
find_package(PkgConfig REQUIRED)
pkg_check_modules (LIBS REQUIRED ${SDK_LIBS_LIST})

set(COMPONENTS_DIR ${TOPDIR}/components)

#### add execuable from sub dir
# execute_process(
#         COMMAND ls ${COMPONENTS_DIR}
#         OUTPUT_VARIABLE dirs)

# string(REPLACE "\n" ";" RPLACE_LIST ${COMPONENT_LIST})
include_directories(${LIBS_INCLUDE_DIRS})
foreach (f ${COMPONENT_LIST})
    message("component :${f}")
    # add_subdirectory(${COMPONENTS_DIR}/${f} out/${f})
    include(${COMPONENTS_DIR}/${f}/CMakeLists.txt)
endforeach (f ${RPLACE_LIST})

target_link_libraries(${PROJ_NAME} PUBLIC 
                        -Wl,--whole-archive
                        ${COMPONENT_LIST} ${LIBS_LIBRARIES} ${TOOLCHAIN_LIBS_LIST}
                        -Wl,--no-whole-archive)
