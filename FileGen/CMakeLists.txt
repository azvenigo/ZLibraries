
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

set(FILEGEN_SOURCES FileGen.cpp ../Common/helpers/CommandLineParser.cpp)
set(FILEGEN_HEADERS ../Common/helpers/CommandLineParser.h)


####################
# common source
list(APPEND INCLUDE_DIRS ../Common)



####################
# PROJECT DEPENDENCY AND INCLUDES


####################
# source and include sets

set(INCLUDES ${INCLUDES} ${FILEGEN_HEADERS} ${COMMON_HEADERS} )
set(SOURCES ${SOURCES} ${FILEGEN_SOURCES} )


####################
# GUI groups

source_group(FileGen FILES ${FILEGEN_SOURCES} ${FILEGEN_HEADERS})
source_group(common FILES ${COMMON_HEADERS})


####################
# EXTRA FLAGS

if(MSVC)
    # ignore pdb not found
    set(EXTRA_FLAGS "/WX")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
else()
    set(EXTRA_FLAGS "-Wall -Wextra -Werror -march=x86-64")
    set(EXTRA_CXX_FLAGS "-std=c++11")
    if( SYMBOLS ) 
        set(EXTRA_FLAGS "-g ${EXTRA_FLAGS}")
    endif()
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_FLAGS} ${EXTRA_CXX_FLAGS}")


####################
# CLANG SANITIZER

if(USE_SANITIZER)
    add_definitions(-fsanitize=${USE_SANITIZER} -g -fno-omit-frame-pointer)
    list(APPEND LINK_LIBS -fsanitize=${USE_SANITIZER})
endif(USE_SANITIZER)


####################
# GIT TAG AND HASH

add_definitions("-DGIT_COMMIT_TAG_HASH=\"${GIT_COMMIT_TAG_HASH}\"")


####################
# ADDITIONAL LIBRARIES

if(MSVC)

elseif(APPLE)

else()
	list(APPEND LINK_LIBS pthread)
endif()


####################
# PROJECT

link_directories(${LINK_DIRS})
include_directories(${INCLUDE_DIRS})

add_executable(FileGen ${SOURCES} ${INCLUDES})
target_link_libraries(FileGen ${LINK_LIBS})

################################################################################

if (CMAKE_BUILD_TYPE STREQUAL Release)
    # copy at build time to propagate any changes
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND copy "$(TargetPath)" "$(SolutionDir)Common\\Tools\\" )
endif()
