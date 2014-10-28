# For stand-alone build, linking to the system EEL and Eelium libraries
#find_package(EEL REQUIRED)
#include_directories(${EEL_INCLUDE_DIR})
#find_package(EELIUM REQUIRED)
#include_directories(${EELIUM_INCLUDE_DIR})

# For local build, not relying on an installed library
include_directories(${EEL_SOURCE_DIR}/include)
link_directories(${EEL_BINARY_DIR})

link_directories(${EELIUM_BINARY_DIR})

include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR})
include_directories(${EEL_SOURCE_DIR}/src/core)
if(BUILD_EELIUM)
	include_directories(${EEL_SOURCE_DIR}/src/eelium)
endif(BUILD_EELIUM)

add_executable(eel WIN32 MACOSX_BUNDLE eel.c)
target_link_libraries(eel libeel)

if(BUILD_EELIUM)
	target_link_libraries(eel libeelium)
endif(BUILD_EELIUM)

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

install(TARGETS eel DESTINATION bin)
