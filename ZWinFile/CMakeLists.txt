
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(ZWinFile)

include(${CMAKE_SOURCE_DIR}/../CMakeLists_Common.txt)

####################
# ZWinFile

set(ZWINFILE_SOURCES ZWinFile.cpp)




####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${ZWINFILE_HEADERS} ${COMMON_HEADERS} )
set(SOURCES ${ZWINFILE_SOURCES} ${COMMON_FILES} )


####################
# GUI groups

#source_group(ZWinFile FILES ${ZWINFILE_SOURCES} ${ZWINFILE_HEADERS} )
source_group(Common FILES ${COMMON_FILES} )


####################
# EXTRA FLAGS

if(MSVC)
    # ignore pdb not found
    set(EXTRA_FLAGS "/WX")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
else()
    set(EXTRA_FLAGS "-Wall -Wextra -Werror -march=x86-64")
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

add_executable(ZWinFile ${SOURCES} ${INCLUDES})
target_link_libraries(ZWinFile ${LINK_LIBS})
    