# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)

# Portable compilation flags.
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG") 

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Configure crash reporting
set(RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in release builds")
set(NON_RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in developer builds")

if(RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DLL_SEND_CRASH_REPORTS=1")
endif()

if(NON_RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DLL_SEND_CRASH_REPORTS=1")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DLL_SEND_CRASH_REPORTS=1")
endif()  

# Don't bother with a MinSizeRel build.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
    "Supported build types." FORCE)


# Platform-specific compilation flags.

if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  # for "backwards compatibility", cmake sneaks in the Zm1000 option which royally
  # screws incredibuild. this hack disables it.
  # for details see: http://connect.microsoft.com/VisualStudio/feedback/details/368107/clxx-fatal-error-c1027-inconsistent-values-for-ym-between-creation-and-use-of-precompiled-headers
  # http://www.ogre3d.org/forums/viewtopic.php?f=2&t=60015
  # http://www.cmake.org/pipermail/cmake/2009-September/032143.html
  string(REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP -D_SCL_SECURE_NO_WARNINGS=1"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /Zo /MD /MP /Ob0 -D_SECURE_STL=0"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /O2 /Zi /Zo /MD /MP /Ob2 -D_SECURE_STL=0 -D_HAS_ITERATOR_DEBUGGING=0"
      CACHE STRING "C++ compiler release options" FORCE)
  # zlib has assembly-language object files incompatible with SAFESEH
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE /SAFESEH:NO /NODEFAULTLIB:LIBCMT")

  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

  add_definitions(
      /DLL_WINDOWS=1
      /DNOMINMAX
#      /DDOM_DYNAMIC            # For shared library colladadom
      /DUNICODE
      /D_UNICODE 
      /GS
      /TP
      /W3
      /c
      /Zc:forScope
      /nologo
      /Oy-
      /Zc:wchar_t-
      /arch:SSE2
      /fp:fast
      )
     
  # Are we using the crummy Visual Studio KDU build workaround?
  if (NOT VS_DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT VS_DISABLE_FATAL_WARNINGS)

  # configure win32 API for windows XP+ compatibility
  set(WINVER "0x0501" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")
endif (WINDOWS)


if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)

  # Here's a giant hack for Fedora 8, where we can't use
  # _FORTIFY_SOURCE if we're using a compiler older than gcc 4.1.

  find_program(GXX g++)
  mark_as_advanced(GXX)

  if (GXX)
    execute_process(
        COMMAND ${GXX} --version
        COMMAND sed "s/^[gc+ ]*//"
        COMMAND head -1
        OUTPUT_VARIABLE GXX_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
  else (GXX)
    set(GXX_VERSION x)
  endif (GXX)

  # The quoting hack here is necessary in case we're using distcc or
  # ccache as our compiler.  CMake doesn't pass the command line
  # through the shell by default, so we end up trying to run "distcc"
  # " g++" - notice the leading space.  Ugh.

  execute_process(
      COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version"
      COMMAND sed "s/^[gc+ ]*//"
      COMMAND head -1
      OUTPUT_VARIABLE CXX_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  if (${GXX_VERSION} STREQUAL ${CXX_VERSION})
    add_definitions(-D_FORTIFY_SOURCE=2)
  else (${GXX_VERSION} STREQUAL ${CXX_VERSION})
    if (NOT ${GXX_VERSION} MATCHES " 4.1.*Red Hat")
      add_definitions(-D_FORTIFY_SOURCE=2)
    endif (NOT ${GXX_VERSION} MATCHES " 4.1.*Red Hat")
  endif (${GXX_VERSION} STREQUAL ${CXX_VERSION})

  # Let's actually get a numerical version of gxx's version
  STRING(REGEX REPLACE ".* ([0-9])\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CXX_VERSION_NUMBER ${CXX_VERSION})

  # Hacks to work around gcc 4.1 TC build pool machines which can't process pragma warning disables
  # This is pure rubbish; I wish there was another way.
  #
  set(CMAKE_CXX_FLAGS "-Wno-unused-variable -Wno-unused-function -Wno-unused-local-typedefs -Wno-maybe-uninitialized")

  if(${CXX_VERSION_NUMBER} LESS 420)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wno-uninitialized ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} LESS 420)

  if(${CXX_VERSION_NUMBER} GREATER 459)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wno-unused-but-set-variable ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 459)

  # gcc 4.3 and above don't like the LL boost and also
  # cause warnings due to our use of deprecated headers
  if(${CXX_VERSION_NUMBER} GREATER 429)
    add_definitions(-Wno-parentheses)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 429)

  # End of hacks.

  add_definitions(
      -DLL_LINUX=1
      -D_REENTRANT
      -fexceptions
      -fno-math-errno
      -fno-strict-aliasing
      -fsigned-char
      -g
      -msse2
      -mfpmath=sse
      -pthread
      )

  # force this platform to accept TOS via external browser
  add_definitions(-DEXTERNAL_TOS)

  add_definitions(-DAPPID=secondlife)
  add_definitions(-fvisibility=hidden)
  # don't catch SIGCHLD in our base application class for the viewer - some of our 3rd party libs may need their *own* SIGCHLD handler to work.  Sigh!  The viewer doesn't need to catch SIGCHLD anyway.
  add_definitions(-DLL_IGNORE_SIGCHLD)
  if (WORD_SIZE EQUAL 32)
    add_definitions(-march=pentium4)
  endif (WORD_SIZE EQUAL 32)
  add_definitions(-mfpmath=sse)
  #add_definitions(-ftree-vectorize) # THIS CRASHES GCC 3.1-3.2
  if (NOT USESYSTEMLIBS)
    # this stops us requiring a really recent glibc at runtime
    add_definitions(-fno-stack-protector)
    # linking can be very memory-hungry, especially the final viewer link
    set(CMAKE_CXX_LINK_FLAGS "-Wl,--no-keep-memory")
  endif (NOT USESYSTEMLIBS)

  set(CMAKE_CXX_FLAGS_DEBUG "-fno-inline ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")
endif (LINUX)


if (DARWIN)
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-no_compact_unwind -Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-g -Wno-unused-local-typedef -Wno-deprecated-declarations")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  set(ENABLE_SIGNING TRUE)
  set(SIGNING_IDENTITY "Developer ID Application: Linden Research, Inc.")
endif (DARWIN)


if (LINUX OR DARWIN)
  if (CMAKE_CXX_COMPILER MATCHES ".*clang")
    set(CMAKE_COMPILER_IS_CLANGXX 1)
  endif (CMAKE_CXX_COMPILER MATCHES ".*clang")

  if (CMAKE_COMPILER_IS_GNUCXX)
    set(GCC_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs")
  elseif (CMAKE_COMPILER_IS_CLANGXX)
    set(GCC_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs")
  endif()

  if (NOT GCC_DISABLE_FATAL_WARNINGS)
    set(GCC_WARNINGS "${GCC_WARNINGS} -Werror")
  endif (NOT GCC_DISABLE_FATAL_WARNINGS)

  set(GCC_CXX_WARNINGS "${GCC_WARNINGS} -Wno-reorder -Wno-non-virtual-dtor")

  set(CMAKE_C_FLAGS "${GCC_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${GCC_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")

  if (WORD_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (WORD_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (WORD_SIZE EQUAL 32)
endif (LINUX OR DARWIN)


if (USESYSTEMLIBS)
  add_definitions(-DLL_USESYSTEMLIBS=1)

  if (LINUX AND ${ARCH} STREQUAL "i686")
    add_definitions(-march=pentiumpro)
  endif (LINUX AND ${ARCH} STREQUAL "i686")

else (USESYSTEMLIBS)
  set(${ARCH}_linux_INCLUDES
      ELFIO
      atk-1.0
      glib-2.0
      gstreamer-0.10
      gtk-2.0
      pango-1.0
      )
endif (USESYSTEMLIBS)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
