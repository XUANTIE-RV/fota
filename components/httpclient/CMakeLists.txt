file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/httpclient/src/*.c")

include_directories(${COMPONENTS_DIR}/httpclient/include)

add_library(httpclient STATIC ${LIBSOURCE})