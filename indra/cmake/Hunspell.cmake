# -*- cmake -*-
include(Prebuilt)

set(HUNSPELL_FIND_QUIETLY ON)
set(HUNSPELL_FIND_REQUIRED ON)

if (STANDALONE)
  include(FindHUNSPELL)
else (STANDALONE)
  use_prebuilt_binary(libhunspell)
  if (WINDOWS)
    set(HUNSPELL_LIBRARY libhunspell)
    set(HUNSPELL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hunspell)
  elseif(DARWIN)
    set(HUNSPELL_LIBRARY hunspell-1.3)
    set(HUNSPELL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hunspell)
  else()
    set(HUNSPELL_LIBRARY hunspell-1.3)
    set(HUNSPELL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/hunspell)
  endif()
endif (STANDALONE)
