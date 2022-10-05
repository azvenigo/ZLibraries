
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(ZZip)

####################
# CHECK IS 64BIT BUILD

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
else()
	message(FATAL_ERROR "32bit Unsupported")
endif()


####################
# ZZip

set(ZZIP_SOURCES main.cpp ZZipAPI.h ZZipAPI.cpp ZipJob.h ZipJob.cpp ZipHeaders.h ZipHeaders.cpp ZZFileAPI.h ZZFileAPI.cpp ZZipTrackers.h zlibAPI.h zlibAPI.cpp)
set(COMMON_FILES ../Common/helpers/CommandLineParser.cpp ../Common/helpers/CommandLineParser.h ../Common/helpers/Crc32Fast.h ../Common/helpers/Crc32Fast.cpp)
list(APPEND COMMON_FILES ../Common/helpers/FNMatch.h ../Common/helpers/FNMatch.cpp ../Common/helpers/HTTPCache.h ../Common/helpers/HTTPCache.cpp)
list(APPEND COMMON_FILES ../Common/helpers/StringHelpers.h ../Common/helpers/StringHelpers.cpp ../Common/helpers/ThreadPool.h)
list(APPEND COMMON_FILES  ../Common/zlib-1.2.11/deflate.c  ../Common/zlib-1.2.11/inflate.c ../Common/zlib-1.2.11/adler32.c ../Common/zlib-1.2.11/zutil.c ../Common/zlib-1.2.11/crc32.c ../Common/zlib-1.2.11/trees.c ../Common/zlib-1.2.11/inftrees.c ../Common/zlib-1.2.11/inffast.c)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common ../Common/curl/include ../Common/zlib-1.2.11 )
list(APPEND LINK_DIRS ../Common/curl/lib )


####################
# PROJECT DEPENDENCY AND INCLUDES
list(APPEND LINK_LIBS libcurl.lib )


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
else()
    set(EXTRA_FLAGS "-Wall -Wextra -Werror -march=x86-64 -pthread")
    set(EXTRA_CXX_FLAGS "-std=c++17")
    if( SYMBOLS ) 
        set(EXTRA_FLAGS "-g ${EXTRA_FLAGS}")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS} ${EXTRA_CXX_FLAGS}")

####################
# ADDITIONAL LIBRARIES

####################
# PROJECT

link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(ZZip ${SOURCES} ${INCLUDES})
target_link_libraries(ZZip ${LINK_LIBS})
    
if(MSVC)
   # Copy build to tools folder if release
   add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND cmd.exe /c if "$(Configuration)" == "Release" "${CMAKE_COMMAND}" -E copy_if_different  "$(TargetPath)" "$(SolutionDir)Common\\Tools\\" )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$(SolutionDir)Common/curl/bin/libcurl.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$(SolutionDir)Common/libssh2/libssh2.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$(SolutionDir)Common/openssl/libssl-1_1-x64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "$(SolutionDir)Common/openssl/libcrypto-1_1-x64.dll" $<TARGET_FILE_DIR:${PROJECT_NAME}>)
