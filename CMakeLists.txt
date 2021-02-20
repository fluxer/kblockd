cmake_minimum_required(VERSION 3.0.3 FATAL_ERROR)

project(katana CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# required because Katie does
if(NOT CMAKE_VERSION VERSION_LESS "3.1.0")
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_STANDARD 11)
endif()

if(NOT CMAKE_VERSION VERSION_LESS "3.3.0")
    cmake_policy(SET CMP0063 NEW)
endif()

include(FeatureSummary)
include(GNUInstallDirs)

find_package(Katie)
set_package_properties(Katie PROPERTIES
    PURPOSE "The building block of this project"
    DESCRIPTION "C++ toolkit derived from the Qt 4.8 framework"
    URL "http://fluxer.github.io/katie/"
    TYPE REQUIRED
)

find_package(UDev)
set_package_properties(UDev PROPERTIES
    PURPOSE "Disk support"
    DESCRIPTION "System and Service Manager"
    URL "https://www.freedesktop.org/wiki/Software/systemd/"
    TYPE REQUIRED
)

add_definitions(${QT_DEFINITIONS})

include_directories(
    ${QT_INCLUDES}
    ${UDEV_INCLUDES}
    ${CMAKE_SOURCE_DIR}/src
)

## kblockd library
set(kblockd_library_SOURCES
    ${CMAKE_SOURCE_DIR}/src/kdiskmanager.cpp
)

add_library(kblockd_library SHARED ${kblockd_library_SOURCES})
target_link_libraries(kblockd_library
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${UDEV_LIBRARIES}
)
set_target_properties(kblockd_library PROPERTIES OUTPUT_NAME kblockd)

set_target_properties(kblockd_library PROPERTIES
    VERSION 1.0
    SOVERSION 1.0.0
)

install(
    TARGETS kblockd_library
    DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
)

install(
    FILES ${CMAKE_SOURCE_DIR}/src/kdiskmanager.hpp
    DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)

## kblockd
set(kblockd_SOURCES
    ${CMAKE_SOURCE_DIR}/src/kblockd.cpp
)

add_executable(kblockd ${kblockd_SOURCES})
target_link_libraries(kblockd
    ${QT_QTCORE_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    kblockd_library
)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/com.kblockd.Block.service.cmake
    ${CMAKE_BINARY_DIR}/com.kblockd.Block.service
    @ONLY
)

install(
    FILES ${CMAKE_SOURCE_DIR}/src/com.kblockd.Block.xml
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/interfaces
)

install(
    FILES ${CMAKE_SOURCE_DIR}/src/com.kblockd.Block.conf
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/system.d
)

install(
    FILES ${CMAKE_BINARY_DIR}/com.kblockd.Block.service
    DESTINATION ${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/system-services
)

install(
    TARGETS kblockd
    DESTINATION ${CMAKE_INSTALL_FULL_BINDIR}
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
