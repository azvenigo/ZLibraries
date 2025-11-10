
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(DupeScanner)

include(${CMAKE_CURRENT_LIST_DIR}/../CMakeLists_Common.txt)


####################
# DupeScanner

set(DUPESCANNER_SOURCES BlockScanner.cpp DupeScanner.cpp)
list(APPEND COMMON_FILES 
../Common/helpers/sha256.h 
../Common/helpers/sha256.cpp 
)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common)



####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${DUPESCANNER_HEADERS} ${COMMON_HEADERS})
set(SOURCES ${DUPESCANNER_SOURCES} ${COMMON_FILES})


####################
# GUI groups

#source_group(DupeScanner FILES ${DUPESCANNER_SOURCES} ${DUPESCANNER_HEADERS})
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
if(MSVC)
	list(APPEND LINK_LIBS Version.lib)
endif()

####################
# PROJECT

link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(DupeScanner ${SOURCES} ${INCLUDES})
target_link_libraries(DupeScanner ${LINK_LIBS})
    