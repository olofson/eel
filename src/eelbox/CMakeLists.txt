include(CheckIncludeFiles)
include(CheckFunctionExists)

include_directories(${EEL_SOURCE_DIR}/include)
include_directories(${EEL_SOURCE_DIR}/src/eelbox)
include_directories(${EEL_SOURCE_DIR}/src/eelbox/audiality2)
include_directories(${EEL_SOURCE_DIR}/src/eelbox/midi)
include_directories(${EEL_SOURCE_DIR}/src/eelbox/net2)
include_directories(${EEL_SOURCE_DIR}/src/eelbox/zeedraw)
include_directories(${EEL_SOURCE_DIR}/src/eelbox/zeespace)

# FIXME: eel_physics.c needs to access some internals of the vector types...
include_directories(${EEL_SOURCE_DIR}/src/core)

# eelbox modules
set(sources
	eelbox.c
	eb_image.c
	eb_net.c
	eb_opengl.c
	eb_sdl.c
	eel_physics.c
	IMG_savepng.c
	sfifo.c

	audiality2/eel_audiality2.c

	midi/eel_midi.c

	net2/fastevents.c
	net2/net2.c
	net2/net2sets.c
	net2/trace.c

	zeedraw/zeedraw.c

	zeespace/eel_zeespace.c
	zeespace/zeespace.c
#	zeespace/zs_path.c
	zeespace/zs_pipe.c
	zeespace/zs_region.c
)


find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})
link_libraries(${SDL_LIBRARY})

find_package(SDL_net REQUIRED)
include_directories(${SDLNET_INCLUDE_DIR})
link_libraries(${SDLNET_LIBRARY})

find_package(SDL_image REQUIRED)
include_directories(${SDLIMAGE_INCLUDE_DIR})
link_libraries(${SDLIMAGE_LIBRARY})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
link_libraries(${PNG_LIBRARY})

#find_package(AUDIALITY2 REQUIRED)
#include_directories(${AUDIALITY2_INCLUDE_DIR})
#link_libraries(${AUDIALITY2_LIBRARY})

find_package(PkgConfig)
pkg_check_modules(AUDIALITY2 REQUIRED audiality2)
include_directories(${AUDIALITY2_INCLUDE_DIRS})
link_libraries(${AUDIALITY2_LIBRARIES})

#CHECK_INCLUDE_FILES(jack/jack.h HAVE_JACK_H)
#if(HAVE_JACK_H)
#	add_definitions(-DA2_HAVE_JACK)
#endif(HAVE_JACK_H)

find_package(ALSA)
if(ALSA_FOUND)
	add_definitions(-DA2_HAVE_ALSA)
endif(ALSA_FOUND)


#add_library(libeelbox SHARED ${sources})
add_library(libeelbox STATIC ${sources})


# Until 1.0, we assume there's no ABI compatibility across releases!
set_target_properties(libeelbox PROPERTIES VERSION ${VERSION_STRING}
	SOVERSION ${VERSION_STRING})

set_target_properties(libeelbox PROPERTIES OUTPUT_NAME "eelbox")

#add_library(libeelbox STATIC ${sources})
#target_link_libraries(libeelbox m)
#if(WIN32)
#	add_library(libeelbox STATIC ${sources})
#	target_link_libraries(libeelbox m ${SDL_LIBRARY} -lwinmm -ldxguid)
#else(WIN32)
#	add_library(libeelbox SHARED ${sources})
#	target_link_libraries(libeelbox m ${SDL_LIBRARY})
#endif(WIN32)

#install(TARGETS libeelbox DESTINATION lib)
