# - Try to find UDev
# Once done this will define
#
#  UDEV_FOUND - system has UDev
#  UDEV_INCLUDES - the UDev include directory
#  UDEV_LIBRARIES - The libraries needed to use UDev
#
# Copyright (c) 2018, Ivailo Monev, <xakepa10@gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.

if(UDEV_INCLUDES AND UDEV_LIBRARIES)
    set(UDEV_FIND_QUIETLY TRUE)
endif()

if(NOT WIN32)
    include(FindPkgConfig)
    pkg_check_modules(PC_UDEV QUIET libudev)
endif()

find_path(UDEV_INCLUDES
    NAMES libudev.h
    HINTS $ENV{UDEVDIR}/include ${PC_UDEV_INCLUDEDIR}
)

find_library(UDEV_LIBRARIES
    NAMES udev
    HINTS $ENV{UDEVDIR}/lib ${PC_UDEV_LIBDIR}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(UDev
    VERSION_VAR PC_UDEV_VERSION
    REQUIRED_VARS UDEV_INCLUDES UDEV_LIBRARIES
)

mark_as_advanced(UDEV_INCLUDES UDEV_LIBRARIES)
