cmake_minimum_required(VERSION 3.29.0)

# CMake file for building a single SLN with all

project(ZLibraries)

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
