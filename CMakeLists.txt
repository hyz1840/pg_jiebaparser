cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)

set(CMAKE_COLOR_MAKEFILE ON)

set(PG_JIEBA_PROJECT_NAME "jiebaparser")
set(PG_JIEBA_PROJECT_ID "jiebaparser")
project("${PG_JIEBA_PROJECT_ID}")

message(STATUS "Setting ${CMAKE_PROJECT_NAME} build type - ${CMAKE_BUILD_TYPE}")

find_package(PostgreSQL REQUIRED)
if(NOT PostgreSQL_FOUND)
    message(FATAL_ERROR " Please check your PostgreSQL installation.")
endif(NOT PostgreSQL_FOUND)


include_directories(${PostgreSQL_INCLUDE_DIRS})
link_directories(${PostgreSQL_LIBRARY_DIRS})

set(Boost_INCLUDE_DIR /usr/local/src/boost_1_73_0)
set(Boost_LIBRARY_DIR /usr/local/src/boost_1_73_0/stage/lib)
find_package(Boost COMPONENTS system filesystem REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})



set(SOURCE_FILES jbparser.c jbparserImpl.cpp)

set(PG_JIEBA_POSTGRESQL_DIR "${CMAKE_INSTALL_PREFIX}"
        CACHE PATH "PostgreSQL binary directory")
set(PG_JIEBA_POSTGRESQL_VERSION "unknown"
        CACHE STRING "PostgreSQL version")

set(PG_JIEBA_DLL_NAME "jiebaparser")
set(PG_JIEBA_LIBRARY_NAME "${PG_JIEBA_DLL_NAME}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -pthread -lrt -std=c++11")

if(DEBUG_MODE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")
endif(DEBUG_MODE)

find_program(PostgreSQL_PG_CONFIG NAMES pg_config
        PATHS
        /usr/lib/postgresql/*/bin/)

execute_process(COMMAND ${PostgreSQL_PG_CONFIG} --cflags
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE PG_CFLAGS)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PG_CFLAGS}")

execute_process(COMMAND ${PostgreSQL_PG_CONFIG} --cppflags
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE PG_CPPFLAGS)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PG_CPPFLAGS}")

execute_process(
        COMMAND ${PostgreSQL_PG_CONFIG} --pkglibdir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE PG_JIEBA_EXTENSION_DIR
        RESULT_VARIABLE d)
if(NOT PG_JIEBA_EXTENSION_DIR)
    message(FATAL_ERROR "pg_config --pkglibdir failed to return a value. Please check your PostgreSQL installation!")
endif()
execute_process(
        COMMAND ${PostgreSQL_PG_CONFIG} --sharedir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE PG_SHARE_DIR)
if(NOT PG_SHARE_DIR)
    message(FATAL_ERROR "pg_config --sharedir failed to return a value. Please check your PostgreSQL installation!")
endif()
set(PG_JIEBA_EXTENSION_DATA_DIR "${PG_SHARE_DIR}/extension")
set(PG_JIEBA_SEARCH_DATA_DIR "${PG_SHARE_DIR}/tsearch_data/jieba")

execute_process(COMMAND ${PostgreSQL_PG_CONFIG} --bindir
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE T_POSTGRESQL_BIN)


# Checking POSTGRESQL_EXECUTABLE in all the dir (*) - implies that
find_program(PostgreSQL_EXECUTABLE NAMES postgres
             PATHS
             ${T_POSTGRESQL_BIN})
message(STATUS "POSTGRESQL_EXECUTABLE is " ${PostgreSQL_EXECUTABLE})

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${PG_JIEBA_PROJECT_ID}.control"
        PG_JIEBA_CONTROL)

string(REGEX REPLACE ".*default_version = '([0-9.]+)'.*" "\\1"
        PG_JIEBA_VERSION "${PG_JIEBA_CONTROL}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1"
        PG_JIEBA_VERSION_MAJOR "${PG_JIEBA_VERSION}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\2"
        PG_JIEBA_VERSION_MINOR "${PG_JIEBA_VERSION}")
string(REGEX REPLACE "([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\3"
        PG_JIEBA_VERSION_MICRO "${PG_JIEBA_VERSION}")

message(STATUS "PG_JIEBA_VERSION_MICRO: ${PG_JIEBA_VERSION_MICRO}")

if(APPLE)
    set(LIBRARY_MODE_TARGET "MODULE")
else(APPLE)
    set(LIBRARY_MODE_TARGET "SHARED")
endif(APPLE)

add_library(${PG_JIEBA_LIBRARY_NAME} ${LIBRARY_MODE_TARGET} ${SOURCE_FILES})
set_target_properties("${PG_JIEBA_LIBRARY_NAME}" PROPERTIES
                      OUTPUT_NAME "${PG_JIEBA_DLL_NAME}"
                      CXX_STANDARD 11
                      CXX_STANDARD_REQUIRED YES
                      CXX_EXTENSIONS NO
                      POSITION_INDEPENDENT_CODE ON
                      PREFIX "")
if(APPLE)
    set_target_properties(${PG_JIEBA_LIBRARY_NAME}
                          PROPERTIES
                          LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(WIN32 AND MSVC)
    set_target_properties(${PG_JIEBA_LIBRARY_NAME} PROPERTIES PREFIX "lib")
endif(WIN32 AND MSVC)

install(TARGETS "${PG_JIEBA_LIBRARY_NAME}"
        DESTINATION "${PG_JIEBA_EXTENSION_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/${PG_JIEBA_PROJECT_ID}.control"
        DESTINATION "${PG_JIEBA_EXTENSION_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/${PG_JIEBA_PROJECT_ID}.sql"
        RENAME "${PG_JIEBA_PROJECT_ID}--${PG_JIEBA_VERSION}.sql"
        DESTINATION "${PG_JIEBA_EXTENSION_DATA_DIR}")
file(GLOB PG_JIEBA_UPGRADE_SQL_FILES "${PG_JIEBA_PROJECT_ID}--*--*.sql")
install(FILES
        ${PG_JIEBA_UPGRADE_SQL_FILES}
        DESTINATION "${PG_JIEBA_EXTENSION_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/dict/jieba.dict.utf8"
        RENAME "jieba.dict.utf8"
        DESTINATION "${PG_JIEBA_SEARCH_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/dict/hmm_model.utf8"
        RENAME "hmm_model.utf8"
        DESTINATION "${PG_JIEBA_SEARCH_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/dict/user.dict.utf8"
        RENAME "user.dict.utf8"
        DESTINATION "${PG_JIEBA_SEARCH_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/dict/stop_words.utf8"
        RENAME "stop_words.utf8"
        DESTINATION "${PG_JIEBA_SEARCH_DATA_DIR}")
install(FILES
        "${PROJECT_SOURCE_DIR}/dict/idf.utf8"
        RENAME "idf.utf8"
        DESTINATION "${PG_JIEBA_SEARCH_DATA_DIR}")