cmake_minimum_required(VERSION 3.13.5)

set(CMAKE_CXX_STANDARD 17)
project(Cxx-Heterogeneous-Maps)
if(TARGET Boost::boost)
	message(STATUS "hmaps: Boost found upstream")
else()
	find_package(Boost 1.73)
endif()

set(HMAP_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(HMAP_LIBRARY_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

add_library(static-hmap INTERFACE)
target_include_directories(static-hmap INTERFACE ${HMAP_INCLUDE_DIRECTORY})

add_subdirectory(src)
add_executable(test-hmap example/test-hmap.cc)
target_link_libraries(test-hmap LINK_PUBLIC dynamic-hmap)
