idf_build_get_property(build_dir BUILD_DIR)

set(FRONTEND_SRC_DIR ${COMPONENT_DIR}/frontend)
set(FRONTEND_GZ_DIR ${build_dir}/frontend_gz)
set(GZIP_SCRIPT ${COMPONENT_DIR}/gzip_frontend.py)

# Gzip frontend files at configure time so they're available for EMBED_FILES
execute_process(
    COMMAND python3 ${GZIP_SCRIPT} ${FRONTEND_SRC_DIR} ${FRONTEND_GZ_DIR}
    RESULT_VARIABLE GZIP_RESULT
)
if(NOT GZIP_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to gzip frontend files")
endif()

set(requires spiffs freertos openthread esp_http_server)

# For IDF < 6.0, use IDF's built-in json component; for IDF >= 6.0 use managed cJSON component through idf_component.yml
if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    list(APPEND requires json)
endif()

idf_component_register(
    SRC_DIRS src
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS private_include
    REQUIRES network firmware_manager
    PRIV_REQUIRES ${requires}
    EMBED_FILES "favicon.ico"
)

# Everything below is skipped in IDF's script mode (dependency analysis stops at idf_component_register)

# Collect source files for build-time dependency tracking
file(GLOB_RECURSE FRONTEND_SOURCES
    "${FRONTEND_SRC_DIR}/*.html"
    "${FRONTEND_SRC_DIR}/*.js"
    "${FRONTEND_SRC_DIR}/*.css"
)

# Re-gzip at build time when source files change
add_custom_command(
    OUTPUT ${FRONTEND_GZ_DIR}/.stamp
    COMMAND python3 ${GZIP_SCRIPT} ${FRONTEND_SRC_DIR} ${FRONTEND_GZ_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${FRONTEND_GZ_DIR}/.stamp
    DEPENDS ${FRONTEND_SOURCES} ${GZIP_SCRIPT}
    COMMENT "Compressing frontend files with gzip"
)
add_custom_target(compress_frontend DEPENDS ${FRONTEND_GZ_DIR}/.stamp)

add_dependencies(${COMPONENT_LIB} compress_frontend)

spiffs_create_partition_image(spiffs ${FRONTEND_GZ_DIR} FLASH_IN_PROJECT DEPENDS compress_frontend)
