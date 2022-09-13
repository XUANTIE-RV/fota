file(GLOB_RECURSE LIBSOURCE "${COMPONENTS_DIR}/fota_service/*.c")

include_directories(${COMPONENTS_DIR}/fota_service/include)

add_library(fota_service STATIC ${LIBSOURCE})

target_link_libraries(fota_service PUBLIC ${LIBS_LIBRARIES})