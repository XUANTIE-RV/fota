file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/aos_port/*.c")

include_directories(${COMPONENTS_DIR}/aos_port/include)

add_library(aos_port STATIC ${LIBSOURCE})