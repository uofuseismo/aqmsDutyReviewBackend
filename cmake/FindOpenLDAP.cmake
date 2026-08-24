#.rst:
# FindOpenLDAP
# ------------
# 
# Finds the OpenLDAP library and include.
#
# Imported targets
# ^^^^^^^^^^^^^^^^
#
# ``OpenLDAP_FOUND``
#   True indicates OpenLDAP was found.
# ``OpenLDAP::OpenLDAP``
#   The OpenLDAP library, if found. 

# Already in cache, be silent
if (OpenLDAP_INCLUDE_DIR AND LBER_INCLUDE_DIR AND
    OpenLDAP_LIBRARY AND LBER_LIBRARY)
    set(OpenLDAP_FIND_QUIETLY TRUE)
endif()

# Find the include directory
find_path(OpenLDAP_INCLUDE_DIR
          NAMES ldap.h
          PATHS /usr/local/include
                /usr/include
                "$ENV{OpenLDAP_ROOT}/include"
                "$ENV{OpenLDAP_ROOT}")
find_path(LBER_INCLUDE_DIR
          NAMES lber.h
          PATHS /usr/local/include
                /usr/include
                "$ENV{OpenLDAP_ROOT}/include"
                "$ENV{OpenLDAP_ROOT}")


# Find the library components
if (BUILD_SHARED_LIBS)
   message("Looking for OpenLDAP shared library")
   find_library(OpenLDAP_LIBRARY
                NAMES ldap
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{OpenLDAP_ROOT}/lib/"
                      "$ENV{OpenLDAP_ROOT}/"
                )
   find_library(LBER_LIBRARY
                NAMES lber
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{OpenLDAP_ROOT}/lib/"
                      "$ENV{OpenLDAP_ROOT}/"
                )
else()
   message("Looking for OpenLDAP static library")
   find_library(OpenLDAP_LIBRARY
                NAMES ldap
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{OpenLDAP_ROOT}/lib/"
                      "$ENV{OpenLDAP_ROOT}/"
               )
   find_library(LBER_LIBRARY
                NAMES lber
                PATHS /usr/local/lib
                      /usr/local/lib64
                      "$ENV{OpenLDAP_ROOT}/lib/"
                      "$ENV{OpenLDAP_ROOT}/"
                )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenLDAP
                                  FOUND_VAR OpenLDAP_FOUND
                                  REQUIRED_VARS OpenLDAP_INCLUDE_DIR LBER_INCLUDE_DIR OpenLDAP_LIBRARY LBER_LIBRARY)
if (OpenLDAP_FOUND AND NOT TARGET OpenLDAP::ldap)
   message("Adding OpenLDAP::ldap")
   add_library(OpenLDAP::ldap UNKNOWN IMPORTED)
   set_target_properties(OpenLDAP::ldap PROPERTIES
                         IMPORTED_LOCATION "${OpenLDAP_LIBRARY}"
                         INTERFACE_INCLUDE_DIRECTORIES "${OpenLDAP_INCLUDE_DIR}")
endif()
if (OpenLDAP_FOUND AND NOT TARGET OpenLDAP::lber)
   message("Adding OpenLDAP::lber")
   add_library(OpenLDAP::lber UNKNOWN IMPORTED)
   set_target_properties(OpenLDAP::lber PROPERTIES
                         IMPORTED_LOCATION "${LBER_LIBRARY}"
                         INTERFACE_INCLUDE_DIRECTORIES "${LBER_INCLUDE_DIR}")
endif()
mark_as_advanced(OpenLDAP_INCLUDE_DIR LBER_INCLUDE_DIR OpenLDAP_LIBRARY LBER_LIBRARY)

