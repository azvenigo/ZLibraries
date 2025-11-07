
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(ZPackager)

include(${CMAKE_SOURCE_DIR}/../CMakeLists_Common.txt)

####################
# ZPackager

set(ZPACKAGER_SOURCES 
main.cpp 
../ZZip/ZZipAPI.h ../ZZip/ZZipAPI.cpp 
../ZZip/ZipJob.h ../ZZip/ZipJob.cpp 
../ZZip/ZipHeaders.h ../ZZip/ZipHeaders.cpp 
../ZZip/ZZipTrackers.h 
../ZZip/zlibAPI.h ../ZZip/zlibAPI.cpp)

list(APPEND COMMON_FILES 
../Common/helpers/Crc32Fast.h 
../Common/helpers/Crc32Fast.cpp
../Common/helpers/ZZFileAPI.h 
../Common/helpers/ZZFile_PC.cpp 
../Common/helpers/Registry.h 
../Common/helpers/Registry.cpp
)

list(APPEND COMMON_FILES ../Common/helpers/FNMatch.h ../Common/helpers/FNMatch.cpp ../Common/helpers/HTTPCache.h ../Common/helpers/HTTPCache.cpp)
list(APPEND COMMON_FILES ../Common/helpers/StringHelpers.h ../Common/helpers/StringHelpers.cpp ../Common/helpers/ThreadPool.h)
list(APPEND COMMON_FILES  ../Common/zlib-1.2.11/deflate.c  ../Common/zlib-1.2.11/inflate.c ../Common/zlib-1.2.11/adler32.c ../Common/zlib-1.2.11/zutil.c ../Common/zlib-1.2.11/crc32.c ../Common/zlib-1.2.11/trees.c ../Common/zlib-1.2.11/inftrees.c ../Common/zlib-1.2.11/inffast.c)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common ../Common/curl/include ../Common/zlib-1.2.11 ../Common/json )
list(APPEND LINK_DIRS ../Common/curl/lib )


####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${ZPACKAGER_HEADERS} ${COMMON_HEADERS})
set(SOURCES ${ZPACKAGER_SOURCES} ${COMMON_FILES})


####################
# GUI groups

source_group(Common FILES ${COMMON_FILES})


####################
# EXTRA FLAGS

if(MSVC)
    # ignore pdb not found
    set(EXTRA_FLAGS "/WX")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
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

add_executable(ZPackager ${SOURCES} ${INCLUDES})
target_link_libraries(ZPackager ${LINK_LIBS})
