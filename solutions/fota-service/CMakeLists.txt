##
 # Copyright (C) 2018-2021 Alibaba Group Holding Limited
##

cmake_minimum_required(VERSION 2.8)

set(PROJ_NAME fota-service)
set(TOPDIR  ../../)
project(${PROJ_NAME} C CXX)

set(CMAKE_BUILD_TYPE "Debug")
ADD_DEFINITIONS(-D_GNU_SOURCE
                -Wall
                -DCONFIG_TCPIP
                -DCONFIG_USING_TLS
                -DCONFIG_FOTA_BUFFER_SIZE=262144
                -DCONFIG_DL_FINISH_FLAG_POWSAVE
                -DUBI_NOT_SUPPORT_INTERRUPTED_UPDATE
                -DCONFIG_NV_PATH="/data/kv/kv"
                -Wno-format-security)

# 添加 .c .cpp  文件
include_directories(porting)
include_directories(libubi)
aux_source_directory(porting PORTING_SRC)
aux_source_directory(libubi LIBUBI_SRC)
add_executable(${PROJ_NAME} ${PORTING_SRC} ${LIBUBI_SRC} main.c)

list(APPEND COMPONENT_LIST
            fota
            fota_service
            ulog
            cjson
            kv
            aos_port
            httpclient
            transport
            mbedtls
            )

list(APPEND SDK_LIBS_LIST
            dbus-1)

list(APPEND TOOLCHAIN_LIBS_LIST
            pthread
            rt)

include(${TOPDIR}/project.cmake)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/install)
install(TARGETS fota-service
        RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fota-dbus.conf
        DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/dbus-1/system.d/
)

