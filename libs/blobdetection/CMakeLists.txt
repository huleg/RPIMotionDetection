# Properties for standalone cmake script:
#cmake_minimum_required(VERSION 2.8)
#project(Bloblib)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fpic -O3" )
#set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -g -O0 -fmax-errors=3 -w" )

set(THRESH_SOURCES blob.c threshtree.c tree.c threshtree_old.c )
add_library(threshtree SHARED ${THRESH_SOURCES} )

set(DEPTH_SOURCES blob.c depthtree.c tree.c )
add_library(depthtree SHARED ${DEPTH_SOURCES} )


install(TARGETS threshtree depthtree 
	LIBRARY DESTINATION lib
	)


#add_executable(example1 example1.c )
#target_link_libraries(example1 threshtree ) 
#
#add_executable( example2 example2.c )
#target_link_libraries(example2 depthtree ) 
#
#
#install(TARGETS example1 example2
#	RUNTIME DESTINATION bin
#	)
