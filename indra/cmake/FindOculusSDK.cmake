# -*- cmake -*-

# - Find Oculus SDK
# This module defines
#  OCULUS_INCLUDE_DIRS - the OculusSDK include directory
#  OCULUS_LIBRARIES - Link these to use OculusSDK
#  OCULUS_FOUND - If false, don't try to use Oculus

FIND_PATH(OCULUS_INCLUDE_DIR
  NAMES
    OVR.h
    OVRVersion.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
  )

FIND_PATH(OCULUS_LIBRARY_DIR
  NAMES
    libovr.a
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
  )

FIND_LIBRARY(OCULUS_LIBRARY
  NAMES
    ovr
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
  )

if (OCULUS_LIBRARY)
  SET(OCULUS_FOUND TRUE)
endif (OCULUS_LIBRARY)

SET(OCULUS_INCLUDE_DIRS
  ${OCULUS_INCLUDE_DIR}
  )

if (OCULUS_FOUND)
  SET(OCULUS_LIBRARY
    ${OCULUS_LIBRARIES}
    ${OCULUS_LIBRARY}
    )
endif (OCULUS_FOUND)

if (OCULUS_INCLUDE_DIRS AND OCULUS_LIBRARY)
  SET(OCULUS_FOUND "YES")
else (OCULUS_INCLUDE_DIRS AND OCULUS_LIBRARY)
  SET(OCULUS_FOUND "NO"
endif (OCULUS_INCLUDE_DIRS AND OCULUS_LIBRARY)

if (OCULUS_FOUND)
  if (NOT OCULUS_FIND_QUIETLY)
    MESSAGE(STATUS "Found OculusSDK: ${OCULUS_LIBRARY}")
  endif (NOT OCULUS_FIND_QUIETLY)
else (OCULUS_FOUND)
  if (OCULUS_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find Oculus SDK")
  endif (OCULUS_FIND_REQUIRED)
endif (OCULUS_FOUND)

  # show the OCULUS_INCLUDE_DIRS and OCULUS_LIBRARY variables only in the advanced view
MARK_AS_ADVANCED(
  OCULUS_INCLUDE_DIRS
  OCULUS_LIBRARY
  )
