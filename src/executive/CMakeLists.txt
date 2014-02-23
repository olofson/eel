# For stand-alone build, linking to the system EEL library
#find_package(EEL REQUIRED)
#include_directories(${EEL_INCLUDE_DIR})

# For local build, not relying on an installed library
include_directories(${EEL_SOURCE_DIR}/include)
link_directories(${EEL_BINARY_DIR})

link_directories(${EELBOX_BINARY_DIR})

include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR})
include_directories(${EEL_SOURCE_DIR}/src/core)
if(BUILD_EELBOX)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox/audiality2)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox/midi)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox/net2)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox/zeedraw)
	include_directories(${EEL_SOURCE_DIR}/src/eelbox/zeespace)
endif(BUILD_EELBOX)

add_executable(eel WIN32 MACOSX_BUNDLE eel.c)
target_link_libraries(eel libeel)

#find_package(SDL)
#if(SDL_FOUND)
#	include_directories(${SDL_INCLUDE_DIR})
#	link_libraries(${SDL_LIBRARY})
#	target_link_libraries(eel libeelbox)
#	set(EEL_HAVE_EELBOX ON)
#else(SDL_FOUND)
#	set(EEL_HAVE_EELBOX OFF)
#endif(SDL_FOUND)
if(BUILD_EELBOX)
	target_link_libraries(eel libeelbox)
endif(BUILD_EELBOX)

# Release build: full optimization, no debug features, no debug info
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Maintainer build: No optimizations, lots of warnings, fail on warnings
set(f "-O1 -g -DDEBUG -Wall -Werror -Wwrite-strings -Wcast-align")
set(f "${f} -Wbad-function-cast -Waggregate-return")
set(f "${f} -Wstrict-prototypes -fno-builtin -Wshadow")
set(f "${f} -Wdeclaration-after-statement -Wmissing-prototypes")
set(f "${f} -Wmissing-declarations -Wdisabled-optimization")
set(CMAKE_C_FLAGS_MAINTAINER ${f})

# Debug build: Maintainer + extra debug features, don't fail on warnings
set(f "${f} -DDEBUG")
set(CMAKE_C_FLAGS_DEBUG ${f})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include e_config.h")

install(TARGETS eel DESTINATION bin)
