file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/kv/*.c")

include_directories(${COMPONENTS_DIR}/kv/include)

add_library(kv STATIC ${LIBSOURCE})