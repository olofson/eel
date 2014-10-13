include(CheckIncludeFiles)
include(CheckFunctionExists)

include_directories(${EEL_BINARY_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/eelium)
include_directories(${EEL_SOURCE_DIR}/src/eelium/audiality2)
include_directories(${EEL_SOURCE_DIR}/src/eelium/midi)
include_directories(${EEL_SOURCE_DIR}/src/eelium/net2)
include_directories(${EEL_SOURCE_DIR}/src/eelium/zeedraw)
include_directories(${EEL_SOURCE_DIR}/src/eelium/zeespace)

check_include_files(SDL/SDL_opengl.h HAS_SDL_OPENGL_H)

# FIXME: eel_physics.c needs to access some internals of the vector types...
include_directories(${EEL_SOURCE_DIR}/src/core)

# Eelium modules
set(sources
	eelium.c
	eel_image.c
	eel_net.c
	eel_opengl.c
	eel_sdl.c
	eel_physics.c
	IMG_savepng.c
	sfifo.c

	audiality2/eel_audiality2.c

	midi/eel_midi.c

	net2/fastevents.c
	net2/net2.c
	net2/net2sets.c
	net2/trace.c

#	zeedraw/zeedraw.c

	zeespace/eel_zeespace.c
	zeespace/zeespace.c
#	zeespace/zs_path.c
	zeespace/zs_pipe.c
	zeespace/zs_region.c
)


find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

find_package(SDL_net REQUIRED)
include_directories(${SDLNET_INCLUDE_DIR})

find_package(SDL_image REQUIRED)
include_directories(${SDLIMAGE_INCLUDE_DIR})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})

#find_package(AUDIALITY2 REQUIRED)
#include_directories(${AUDIALITY2_INCLUDE_DIR})
find_package(PkgConfig)
pkg_check_modules(AUDIALITY2 REQUIRED audiality2)
include_directories(${AUDIALITY2_INCLUDE_DIRS})

find_package(ALSA)
if(ALSA_FOUND)
	add_definitions(-DA2_HAVE_ALSA)
endif(ALSA_FOUND)


#add_library(libeelium SHARED ${sources})
add_library(libeelium STATIC ${sources})
target_link_libraries(libeelium ${SDL_LIBRARY} ${SDLNET_LIBRARY}
	${SDLIMAGE_LIBRARY} ${PNG_LIBRARY} ${AUDIALITY2_LIBRARIES})
if(WIN32)
	target_link_libraries(libeelium -lwinmm -ldxguid)
	# FIXME: Why do we have to pull all these in manually now...?
	target_link_libraries(libeelium -lws2_32 -liphlpapi -ljpeg -lz)
endif(WIN32)

# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelium PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

if(HAS_SDL_OPENGL_H)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAS_SDL_OPENGL_H")
endif(HAS_SDL_OPENGL_H)

set_target_properties(libeelium PROPERTIES OUTPUT_NAME "eelium")

#add_library(libeelium STATIC ${sources})
#target_link_libraries(libeelium m)
#if(WIN32)
#	add_library(libeelium STATIC ${sources})
#	target_link_libraries(libeelium m ${SDL_LIBRARY} -lwinmm -ldxguid)
#else(WIN32)
#	add_library(libeelium SHARED ${sources})
#	target_link_libraries(libeelium m ${SDL_LIBRARY})
#endif(WIN32)

#install(TARGETS libeelium DESTINATION lib)
