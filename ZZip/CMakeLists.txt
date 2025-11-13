cmake_minimum_required(VERSION 3.29.0)

project(ZZip)

include(${CMAKE_CURRENT_LIST_DIR}/../CMakeLists_Common.txt)

####################
# ZZip

set(ZZIP_SOURCES 
	main.cpp 
	ZZipAPI.h ZZipAPI.cpp 
	ZipJob.h ZipJob.cpp 
	ZipHeaders.h ZipHeaders.cpp 
	ZZipTrackers.h 
	ZZipHelpers.h
	zlibAPI.h zlibAPI.cpp
	)


list(APPEND COMMON_FILES 
../Common/helpers/Crc32Fast.h ../Common/helpers/Crc32Fast.cpp
../Common/helpers/ZZFileAPI.h ../Common/helpers/ZZFile_PC.h ../Common/helpers/ZZFile_PC.cpp 
../Common/helpers/FNMatch.h ../Common/helpers/FNMatch.cpp 
../Common/helpers/HTTPCache.h ../Common/helpers/HTTPCache.cpp
../Common/helpers/ThreadPool.h
../Common/zlib-1.2.11/deflate.c 
../Common/zlib-1.2.11/inflate.c 
../Common/zlib-1.2.11/adler32.c 
../Common/zlib-1.2.11/zutil.c 
../Common/zlib-1.2.11/crc32.c 
../Common/zlib-1.2.11/trees.c 
../Common/zlib-1.2.11/inftrees.c 
../Common/zlib-1.2.11/inffast.c)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common ../Common/curl/include ../Common/zlib-1.2.11 )
list(APPEND LINK_DIRS ../Common/curl/lib )


####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${ZZIP_HEADERS} ${COMMON_HEADERS})
set(SOURCES ${ZZIP_SOURCES} ${COMMON_FILES})


####################
# GUI groups

#source_group(ZZip FILES ${ZZIP_SOURCES} ${ZZIP_HEADERS})
source_group(Common FILES ${COMMON_FILES})


####################
# EXTRA FLAGS

if(MSVC)
    # ignore pdb not found
    set(EXTRA_FLAGS "/WX")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
    add_compile_definitions(ENABLE_CLM ENABLE_CLE ENABLE_COMMAND_HISTORY ENABLE_HTTP ENABLE_CURL)  

else()
    set(EXTRA_FLAGS "-Wall -Werror -march=x86-64 -pthread")
    set(EXTRA_CXX_FLAGS "-std=c++17")
    if( SYMBOLS ) 
        set(EXTRA_FLAGS "-g ${EXTRA_FLAGS}")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS} ${EXTRA_CXX_FLAGS}")

####################
# ADDITIONAL LIBRARIES
list(APPEND LINK_LIBS libcurl.lib)
if(MSVC)
	list(APPEND LINK_LIBS Version.lib)
endif()
####################
# PROJECT

link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(ZZip ${SOURCES} ${INCLUDES})
target_link_libraries(ZZip ${LINK_LIBS})

if(MSVC)    
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/../Common/curl/bin/libcurl.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/../Common/libssh2/libssh2.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/../Common/openssl/libssl-1_1-x64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/../Common/openssl/libcrypto-1_1-x64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()