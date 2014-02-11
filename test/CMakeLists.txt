# For stand-alone build, linking to the system EEL library
#find_package(EEL REQUIRED)
#include_directories(${EEL_INCLUDE_DIR})

# For local build, not relying on an installed library
include_directories(${EEL_SOURCE_DIR}/include)
link_directories(${EEL_BINARY_DIR})

set(EEL_LIBRARY libeel)

add_executable(eeltest WIN32 MACOSX_BUNDLE eeltest.c)
target_link_libraries(eeltest ${EEL_LIBRARY})

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
