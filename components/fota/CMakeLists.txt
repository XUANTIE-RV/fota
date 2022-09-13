file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/fota/*.c")

ADD_DEFINITIONS(-DCONFIG_FOTA_USE_HTTPC=1)

include_directories(${COMPONENTS_DIR}/fota/include)
include_directories(${COMPONENTS_DIR}/fota/http)
include_directories(${COMPONENTS_DIR}/fota)

add_library(fota STATIC ${LIBSOURCE})