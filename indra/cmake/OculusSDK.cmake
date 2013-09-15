# -*- cmake -*-

include(Prebuilt)

set(OCULUSSDK_FIND_QUIETLY ON)
set(OCULUSSDK_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindOculusSDK)
else (STANDALONE)
  use_prebuilt_binary(OVR)
  if (WINDOWS)
    set(OCULUS_LIBRARY
      debug libovrd.lib
      optimized libovr.lib)
  elseif (DARWIN)
    set(OCULUS_LIBRARY libovr.a)
  elseif (LINUX)
    set(OCULUS_LIBRARY libovr.a)
  endif (WINDOWS)
  set(OCULUS_INCLUDE_DIRS 
    "${LIBS_PREBUILT_DIR}/include"
	"${LIBS_PREBUILT_DIR}/include/OVR"
	"${LIBS_PREBUILT_DIR}/include/OVR/Kernel"
	"${LIBS_PREBUILT_DIR}/include/OVR/Util")
endif (STANDALONE)
