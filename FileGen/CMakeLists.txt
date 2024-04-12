
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(FileGen)

####################
# CHECK IS 64BIT BUILD

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
else()
	message(FATAL_ERROR "32bit Unsupported")
endif()


####################
# FileGen

set(FILEGEN_SOURCES FileGen.cpp)
set(COMMON_FILES 
    ../Common/helpers/CommandLineEditor.cpp 
    ../Common/helpers/CommandLineEditor.h 
    ../Common/helpers/CommandLineParser.cpp 
    ../Common/helpers/CommandLineParser.h 
    ../Common/helpers/StringHelpers.h 
    ../Common/helpers/StringHelpers.cpp)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common)



####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${FILEGEN_HEADERS} ${COMMON_HEADERS} )
set(SOURCES ${FILEGEN_SOURCES} ${COMMON_FILES} )


####################
# GUI groups

#source_group(FileGen FILES ${FILEGEN_SOURCES} ${FILEGEN_HEADERS} )
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

####################
# PROJECT
link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(FileGen ${SOURCES} ${INCLUDES})
target_link_libraries(FileGen ${LINK_LIBS})
    