if(WITH_OPENGL)
	add_definitions(-DWITH_OPENGL)
	add_definitions(-DWITH_HISTORY)
endif(WITH_OPENGL)
if(WITH_OCV)
	add_definitions(-DWITH_OCV)
	add_definitions(-DWITH_HISTORY)
endif(WITH_OCV)
add_library(tracker Tracker.cpp Tracker2.cpp)
#install(TARGETS tracker LIBRARY DESTINATION lib)
