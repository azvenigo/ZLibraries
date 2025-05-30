################################################################################
cmake_minimum_required(VERSION 3.7.1)
################################################################################

project(FileGen)

####################
# CLMonitorTest
set(SM_SOURCES 
main.cpp

)




####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${SM_HEADERS} ${COMMON_HEADERS} )
set(SOURCES ${SM_SOURCES} ${COMMON_FILES} )


####################
# GUI groups
source_group(Common FILES ${COMMON_FILES} )


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

add_executable(CLMonitorTest ${SOURCES} ${INCLUDES})
target_link_libraries(CLMonitorTest ${LINK_LIBS})
    