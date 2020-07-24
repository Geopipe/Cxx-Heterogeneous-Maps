cmake_minimum_required(VERSION 3.13.5)

set(CMAKE_CXX_STANDARD 17)
set(HMAP_INCLUDE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(HMAP_LIBRARY_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

add_subdirectory(src)
add_executable(test-hmap example/test-hmap.cc)
target_include_directories(test-hmap
        PUBLIC ${HMAP_INCLUDE_DIRECTORY}
	       Boost::boost
        )
target_link_libraries(test-hmap LINK_PUBLIC dynamic-hmap)
