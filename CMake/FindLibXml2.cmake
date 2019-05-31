# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindLibXml2
-----------

Find the XML processing library (libxml2).

IMPORTED Targets
^^^^^^^^^^^^^^^^

This module defines :prop_tgt:`IMPORTED` target ``LibXml2::LibXml2``, if
libxml2 has been found.

Result variables
^^^^^^^^^^^^^^^^

This module will set the following variables in your project:

``LibXml2_FOUND``
  true if libxml2 headers and libraries were found
``LibXml2_INCLUDE_DIR``
  the directory containing LibXml2 headers
``LibXml2_INCLUDE_DIRS``
  list of the include directories needed to use LibXml2
``LibXml2_LIBRARIES``
  LibXml2 libraries to be linked
``LibXml2_VERSION_STRING``
  the version of LibXml2 found (since CMake 2.8.8)

Cache variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``LibXml2_INCLUDE_DIR``
  the directory containing LibXml2 headers
``LibXml2_LIBRARY``
  path to the LibXml2 library
#]=======================================================================]

find_path(LibXml2_INCLUDE_DIR NAMES libxml/xpath.h
   HINTS
   PATH_SUFFIXES libxml2
   )

message("LibXml2_INCLUDE_DIR --> ${LibXml2_INCLUDE_DIR}")

if(LibXml2_INCLUDE_DIR AND EXISTS "${LibXml2_INCLUDE_DIR}/libxml/xmlversion.h")
    file(STRINGS "${LibXml2_INCLUDE_DIR}/libxml/xmlversion.h" libxml2_version_str
         REGEX "^#define[\t ]+LIBXML_DOTTED_VERSION[\t ]+\".*\"")

    string(REGEX REPLACE "^#define[\t ]+LIBXML_DOTTED_VERSION[\t ]+\"([^\"]*)\".*" "\\1"
           LibXml2_VERSION_STRING "${libxml2_version_str}")
    unset(libxml2_version_str)
endif()

message("LibXml2_VERSION_STRING --> ${LibXml2_VERSION_STRING}")

set(LibXml2_NAMES ${LibXml2_NAMES} libxml2)
foreach(name ${LibXml2_NAMES})
  list(APPEND LibXml2_NAMES_DEBUG "${name}d")
endforeach()

if(NOT LibXml2_LIBRARY)
  find_library(LibXml2_LIBRARY_RELEASE NAMES ${LibXml2_NAMES})
  find_library(LibXml2_LIBRARY_DEBUG NAMES ${LibXml2_NAMES_DEBUG})
  include(SelectLibraryConfigurations)
  select_library_configurations(LibXml2)
  mark_as_advanced(LibXml2_LIBRARY_RELEASE LibXml2_LIBRARY_DEBUG)
endif(NOT LibXml2_LIBRARY)
unset(LibXml2_NAMES)
unset(LibXml2_NAMES_DEBUG)

message("LibXml2_LIBRARY --> ${LibXml2_LIBRARY}")

set(LibXml2_INCLUDE_DIRS ${LibXml2_INCLUDE_DIR})
set(LibXml2_LIBRARIES ${LibXml2_LIBRARY})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibXml2
                                  REQUIRED_VARS LibXml2_LIBRARY LibXml2_INCLUDE_DIR
                                  VERSION_VAR LibXml2_VERSION_STRING)

mark_as_advanced(LibXml2_INCLUDE_DIR LibXml2_LIBRARY)

if(LibXml2_FOUND AND NOT TARGET LibXml2::LibXml2)
  add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
  set_target_properties(LibXml2::LibXml2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LibXml2_INCLUDE_DIRS}")
  set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY IMPORTED_LOCATION "${LibXml2_LIBRARY}")
  if(EXISTS "${LibXml2_LIBRARY_RELEASE}")
    set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
      IMPORTED_LOCATION_RELEASE "${LibXml2_LIBRARY_RELEASE}")
  endif()
  if(EXISTS "${LibXml2_LIBRARY_DEBUG}")
    set_property(TARGET LibXml2::LibXml2 APPEND PROPERTY
      IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
      IMPORTED_LOCATION_DEBUG "${LibXml2_LIBRARY_DEBUG}")
  endif()
endif()
