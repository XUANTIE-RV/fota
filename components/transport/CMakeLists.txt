file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/transport/*.c")

include_directories(${COMPONENTS_DIR}/transport/include)

add_library(transport STATIC ${LIBSOURCE})