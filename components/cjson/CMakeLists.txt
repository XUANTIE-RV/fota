file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/cjson/src/*.c")

include_directories(${COMPONENTS_DIR}/cjson/include)

add_library(cjson STATIC ${LIBSOURCE})