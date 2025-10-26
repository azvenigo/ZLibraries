cmake_minimum_required(VERSION 3.7.1)

project(ZTools)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMN_SOURCE ${PROJECT_SOURCE_DIR}/Common)

####################
# VISUAL C MT MD CONFIGURATION

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(ENABLE_CLM ENABLE_CLE ENABLE_COMMAND_HISTORY)  
	set(CMAKE_CXX_FLAGS_MDDEBUG   "${CMAKE_CXX_FLAGS_DEBUG}")
	set(CMAKE_CXX_FLAGS_MDRELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	STRING(REPLACE "/MD"  "/MT"  CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")


	set(CMAKE_EXE_LINKER_FLAGS_MDRELEASE    "${CMAKE_EXE_LINKER_FLAGS_RELEASE}")
	set(CMAKE_EXE_LINKER_FLAGS_MDDEBUG      "${CMAKE_EXE_LINKER_FLAGS_DEBUG}")

else()
	set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()


####################
# common source
list(APPEND INCLUDE_DIRS ../Common)

list (APPEND COMMON_FILES 
    ${CMN_SOURCE}/helpers/CommandLineParser.h   ${CMN_SOURCE}/helpers/CommandLineParser.cpp    
    ${CMN_SOURCE}/helpers/LoggingHelpers.h      ${CMN_SOURCE}/helpers/LoggingHelpers.cpp       
    ${CMN_SOURCE}/helpers/StringHelpers.h       ${CMN_SOURCE}/helpers/StringHelpers.cpp     
    ${CMN_SOURCE}/helpers/CommandLineEditor.h   ${CMN_SOURCE}/helpers/CommandLineEditor.cpp 
    ${CMN_SOURCE}/helpers/CommandLineMonitor.h 	${CMN_SOURCE}/helpers/CommandLineMonitor.cpp 
    ${CMN_SOURCE}/helpers/CommandLineCommon.h 	${CMN_SOURCE}/helpers/CommandLineCommon.cpp	
    ${CMN_SOURCE}/helpers/StringHelpers.h       ${CMN_SOURCE}/helpers/StringHelpers.cpp 
    ${CMN_SOURCE}/helpers/FileHelpers.h         ${CMN_SOURCE}/helpers/FileHelpers.cpp )
    


####################
# PROJECTS

add_subdirectory(FileGen)
add_subdirectory(DupeScanner)
add_subdirectory(BinTool)
add_subdirectory(ZZip)
add_subdirectory(ZPackager)
add_subdirectory(CLMonitorTest)
add_subdirectory(Reader)

################################################################################
