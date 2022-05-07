
################################################################################

cmake_minimum_required(VERSION 3.7.1)

################################################################################

project(ZTools)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)




####################
# GIT TAG AND HASH

find_package(Git)
if(GIT_FOUND)
    execute_process(COMMAND git --git-dir=${CMAKE_SOURCE_DIR}/.git --work-tree=${CMAKE_SOURCE_DIR} describe --tags --always --long --abbrev=40 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE GIT_RESULT OUTPUT_VARIABLE GIT_COMMIT_TAG_HASH ERROR_VARIABLE GIT_ERROR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if( NOT GIT_RESULT STREQUAL "0" )
		set(GIT_COMMIT_TAG_HASH "unknown")
	endif()
	message( "Build Version: " ${GIT_COMMIT_TAG_HASH} )

    execute_process(COMMAND git --git-dir=${CMAKE_SOURCE_DIR}/.git --work-tree=${CMAKE_SOURCE_DIR} describe --tags --always --abbrev=0 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE GIT_RESULT_TAG OUTPUT_VARIABLE GIT_COMMIT_TAG ERROR_VARIABLE GIT_ERROR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if( NOT GIT_RESULT_TAG STREQUAL "0" )
		set(GIT_COMMIT_TAG "unknown")
	endif()
	message( "Build Tag: " ${GIT_COMMIT_TAG} )

    execute_process(COMMAND git --git-dir=${CMAKE_SOURCE_DIR}/.git --work-tree=${CMAKE_SOURCE_DIR} describe --always --abbrev=40 --exclude "*" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE GIT_RESULT_HASH OUTPUT_VARIABLE GIT_COMMIT_HASH ERROR_VARIABLE GIT_ERROR OUTPUT_STRIP_TRAILING_WHITESPACE)
	if( NOT GIT_RESULT_HASH STREQUAL "0" )
		set(GIT_COMMIT_HASH "unknown")
	endif()
	message( "Build Hash: " ${GIT_COMMIT_HASH} )
endif()


####################
# VISUAL C MT MD CONFIGURATION

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

	set(CMAKE_CXX_FLAGS_MDDEBUG   "${CMAKE_CXX_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_MDRELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	STRING(REPLACE "/MD"  "/MT"  CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")


	set(CMAKE_EXE_LINKER_FLAGS_MDRELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
	set(CMAKE_EXE_LINKER_FLAGS_MDDEBUG      "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")

else()

	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

endif()


####################
# PROJECTS

add_subdirectory(FileGen)
add_subdirectory(DupeScanner)
add_subdirectory(CopyByteRange)

################################################################################
