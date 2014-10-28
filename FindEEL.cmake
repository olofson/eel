# Locate EEL library
#
# This module defines:
#  EEL_FOUND, true if EEL was found
#  EEL_INCLUDE_DIR, where the C headers are found
#  EEL_LIBRARY, where the library is found
#
# Created by David Olofson

FIND_PATH(EEL_INCLUDE_DIR EEL.h
	PATH_SUFFIXES include
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/include/EEL
	/usr/include/EEL
	/sw/include/EEL
	/opt/local/include/EEL
	/opt/csw/include/EEL
	/opt/include/EEL
)

FIND_LIBRARY(EEL_LIBRARY
	NAMES eel
	PATH_SUFFIXES lib64 lib
	PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local
	/usr
	/sw
	/opt/local
	/opt/csw
	/opt
)

SET(EEL_FOUND "NO")
IF(EEL_LIBRARY AND EEL_INCLUDE_DIR)
	SET(EEL_FOUND "YES")
ENDIF(EEL_LIBRARY AND EEL_INCLUDE_DIR)
