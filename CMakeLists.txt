cmake_minimum_required(VERSION 2.6)
project(hellolibpq)

## Enable debug symbols by default
#if(CMAKE_BUILD_TYPE STREQUAL "")
#  set(CMAKE_BUILD_TYPE Debug)
#endif()
## (you can also set it on the command line: -DCMAKE_BUILD_TYPE=Release)

## Set version information in a config.h file
#set(myproject_VERSION_MAJOR 1)
#set(myproject_VERSION_MINOR 0)
#configure_file(
#  "${PROJECT_SOURCE_DIR}/config.h.in"
#  "${PROJECT_BINARY_DIR}/config.h"
#  )
#include_directories("${PROJECT_BINARY_DIR}")

## Define sources and executable

add_executable(hellolibpq hellolibpq.cpp)



SET(Boost_USE_MULTITHREAD OFF)   ## from http://stackoverflow.com/q/7855174
## see www.cmake.org/cmake/help/git-master/module/FindBoost.html

## These two lines may be useful if CMake is unable to find Boost:
# SET(Boost_DEBUG ON)
# SET(Boost_DETAILED_FAILURE_MSG ON)

IF( WIN32  AND  (NOT CYGWIN) )
  SET(Boost_USE_STATIC_LIBS ON) ## there's also Boost_USE_STATIC_RUNTIME, which does.... something else?
  message( "Note: attempting to use static linking for Boost" )
endif()

SET(Boost_MIN_VERSION "1.54.0")
FIND_PACKAGE(Boost ${Boost_MIN_VERSION} REQUIRED) #  COMPONENTS  program_options)
#SET(Boost_EXACT_VERSION "1.55.0")
#FIND_PACKAGE(Boost ${Boost_EXACT_VERSION} EXACT REQUIRED  COMPONENTS  program_options)
  ## if this fails, CMake will print an error message and then bail
## Things like static_assert and type_traits aren't 'components', only libraries with binaries:
# system filesystem program_options


## Detect and add libpq
# set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
find_package(PostgreSQL REQUIRED ) # pq) ## no way to just ask for libpq?????
if(PostgreSQL_FOUND)
  include_directories(${PostgreSQL_INCLUDE_DIRS})
  target_link_libraries(hellolibpq  ${PostgreSQL_LIBRARIES}) ###### TODO can we pare this down? no need to go overboard just for libpq
endif()


## Install target
install(TARGETS hellolibpq  DESTINATION bin)


## CPack packaging
include(InstallRequiredSystemLibraries)
# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "${myproject_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${myproject_VERSION_MINOR}")
include(CPack)

