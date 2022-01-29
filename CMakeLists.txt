cmake_minimum_required(VERSION 3.22)

project(
	address-lib-dmp
	LANGUAGES CXX
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
	message(FATAL_ERROR "in-source builds are not allowed")
endif()

add_subdirectory(src)
