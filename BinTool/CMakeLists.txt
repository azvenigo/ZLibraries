################################################################################
cmake_minimum_required(VERSION 3.7.1)
################################################################################

project(BinTool)

include(${CMAKE_CURRENT_LIST_DIR}/../CMakeLists_Common.txt)

####################
# BinTool

set(BINTOOL_SOURCES main.cpp)

list(APPEND COMMON_FILES 
../Common/helpers/sha256.h 
../Common/helpers/sha256.cpp 
../Common/helpers/Crc32Fast.h 
../Common/helpers/Crc32Fast.cpp 
)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common)

####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${BINTOOL_HEADERS} ${COMMON_HEADERS} )
set(SOURCES ${BINTOOL_SOURCES} ${COMMON_FILES})


####################
# GUI groups

#source_group(BinTool FILES ${BINTOOL_SOURCES} ${BINTOOL_HEADERS})
source_group(Common FILES ${COMMON_FILES})


####################
# EXTRA FLAGS

if(MSVC)
    # ignore pdb not found
    set(EXTRA_FLAGS "/WX")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
else()
    set(EXTRA_FLAGS "-Wall -Wextra -Werror -march=x86-64")
    if( SYMBOLS ) 
        set(EXTRA_FLAGS "-g ${EXTRA_FLAGS}")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS} ${EXTRA_CXX_FLAGS}")


####################
# ADDITIONAL LIBRARIES
if(MSVC)
	list(APPEND LINK_LIBS Version.lib)
endif()

####################
# PROJECT

link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(BinTool ${SOURCES} ${INCLUDES})
target_link_libraries(BinTool ${LINK_LIBS})
    