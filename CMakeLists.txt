cmake_minimum_required( VERSION 3.8 )
project( PDF2Text )

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable ( PDF2Text main.cpp )

if(APPLE)
	find_library( ApplicationServices ApplicationServices )
	target_link_libraries( PDF2Text ${ApplicationServices} )
endif()
