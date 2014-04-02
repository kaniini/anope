# Find the header files, libs, and executables for gettext
if(NOT WIN32)
  find_path(GETTEXT_INCLUDE libintl.h /usr/include /usr/local/include ${EXTRA_INCLUDE})
  find_library(GETTEXT_LIBRARY intl PATHS /usr/lib /usr/lib64 ${EXTRA_LIBS})
  find_library(ICONV_LIBRARY iconv PATHS /usr/lib /usr/lib64 ${EXTRA_LIBS})
  find_program(GETTEXT_MSGFMT msgfmt PATHS /usr/bin/ /usr/local/bin ${EXTRA_INCLUDE})
  if(GETTEXT_INCLUDE AND GETTEXT_MSGFMT)
    set(GETTEXT_FOUND TRUE)
  endif(GETTEXT_INCLUDE AND GETTEXT_MSGFMT)
else(NOT WIN32)
  find_path(GETTEXT_INCLUDE libintl.h $ENV{VCINSTALLDIR}/include gettext/include ${EXTRA_INCLUDE})
  find_library(GETTEXT_LIBRARY libintl PATHS $ENV{VCINSTALLDIR}/lib gettext/lib ${EXTRA_LIBS})
  find_library(ICONV_LIBRARY libiconv PATHS $ENV{VCINSTALLDIR}/lib gettext/lib ${EXTRA_LIBS})
  find_library(MINGWEX_LIBRARY libmingwex PATHS $ENV{VCINSTALLDIR}/lib gettext/lib ${EXTRA_LIBS})
  find_library(GCC_LIBRARY libgcc PATHS $ENV{VCINSTALLDIR}/lib gettext/lib ${EXTRA_LIBS})
  find_program(GETTEXT_MSGFMT msgfmt PATHS $ENV{VCINSTALLDIR}/bin gettext/bin ${EXTRA_INCLUDE})
  if(GETTEXT_INCLUDE AND GETTEXT_MSGFMT AND ICONV_LIBRARY AND MINGWEX_LIBRARY AND GCC_LIBRARY)
    set(GETTEXT_FOUND TRUE)
  endif(GETTEXT_INCLUDE AND GETTEXT_MSGFMT AND ICONV_LIBRARY AND MINGWEX_LIBRARY AND GCC_LIBRARY)
endif(NOT WIN32)

# If we found everything we need set variables correctly for lang/CMakeLists.txt to use
if(GETTEXT_FOUND)
  include_directories("${GETTEXT_INCLUDE}")
  set(GETTEXT_MSGFMT_EXECUTABLE ${GETTEXT_MSGFMT})

  if(WIN32)
    set(GETTEXT_LIBRARIES libiconv libintl libmingwex libgcc)
  else(WIN32)
    if(GETTEXT_LIBRARY)
      set(GETTEXT_LIBRARIES ${GETTEXT_LIBRARY} ${ICONV_LIBRARY})
    endif(GETTEXT_LIBRARY)
  endif(WIN32)
endif(GETTEXT_FOUND)
