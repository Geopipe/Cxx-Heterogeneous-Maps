set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${HMAP_LIBRARY_DIRECTORY})
add_library(dynamic-hmap SHARED dynamic-hmap.cc)
target_include_directories(dynamic-hmap
        PUBLIC ${HMAP_INCLUDE_DIRECTORY}
	       Boost::boost
        )
