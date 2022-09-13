file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/mbedtls/library/*.c")
file(GLOB_RECURSE LIBSOURCE2 "${COMPONENTS_DIR}/mbedtls/platform/yoc/*.c")

include_directories(${COMPONENTS_DIR}/mbedtls/include)
include_directories(${COMPONENTS_DIR}/mbedtls/platform/yoc/include)

add_library(mbedtls STATIC ${LIBSOURCE} ${LIBSOURCE2})