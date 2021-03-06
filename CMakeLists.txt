# Project information
project(Camellia)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "0")
set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(CMENGINE_VERSION "${PROJECT_VERSION}")

# Required version
cmake_minimum_required(VERSION 2.8.4)
cmake_policy(SET CMP0004 OLD)

include(${CMAKE_SOURCE_DIR}/../scimake/SciInit.cmake)

set(CXX ${CMAKE_CXX_COMPILER})
set(BUILD_FLAGS_VAR  CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE})
set(BUILD_FLAGS_VAL "${${BUILD_FLAGS_VAR}}")
set(CXXFLAGS "${BUILD_FLAGS_VAL} ${CMAKE_CXX_FLAGS}")
set(HAVE_CMAKE 1 CACHE STRING "Whether built with CMake")

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SCIMAKE_DIR}/include
)

set(CMDEPS_STATE REQUIRED)
if (BASIC_DEPS_REQUIRED STREQUAL "False") # If set to false, change to empty
  message(STATUS "Unsetting REQUIRED in find_package calls for dependencies.")
  set(CMDEPS_STATE)
endif ()

# Boost required for use of Trilinos
#if (WIN32)
#  find_package(SciBoost
#    COMPONENTS
#      unit_test_framework
#      program_options filesystem chrono system REQUIRED
#  )
#else ()
 # find_package(SciBoost
 #   COMPONENTS
 #     unit_test_framework
 #     program_options filesystem thread chrono system exception REQUIRED
 # )
#endif ()

#if (BOOST_FOUND)
 # #if (DEBUG_CMAKE)
 #   message(STATUS "Boost found with includes at ${Boost_INCLUDE_DIRS}.")
 # endif ()
#  set(HAVE_BOOST 1 CACHE STRING "Whether boost found")
#else ()
#  message(STATUS "Boost not found.")
#endif ()

#if (WIN32)
#  set(WINENABLE:BOOL OFF)
#endif ()

# Z is not a dependency on Windows
#if (NOT WIN32)
#  find_package(SciZ)
#endif ()

# Find packages
#find_package(SciMumps)
#find_package(SciHdf5 ${CMDEPS_STATE})
#find_package(SciZ)
#find_package(SciSz)
#set(H5_USE_16_API 1)
#if (BOOST_FOUND)
  if (ENABLE_PARALLEL)
   set(TRILINOS_BUILD trilinos-parcommio)
  else ()
   set(TRILINOS_BUILD trilinos-sercommio)
  endif ()
  set(USE_TRILINOS_CONFIG_CMAKE 1)
  find_package(SciTrilinos REQUIRED) # Finds lapack and blas
  if ("${Trilinos_USE_VENDOR_LINALG}" STREQUAL "mkl")
    message(STATUS "Adding -mkl to cxx flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mkl=cluster")
    set(Trilinos_LINALG_LIBRARIES)
  endif()

  if (ENABLE_PARALLEL)
    set(MOAB_COMPONENTS MOAB iMesh)
  else ()
    set(MOAB_COMPONENTS MOAB iMesh)
  endif ()
  find_package(SciMoab REQUIRED)
  ADD_DEFINITIONS(-DHAVE_MOAB)
  #find_package(SciNetcdf REQUIRED)
  
#endif ()

# Set TARGET_SUPPORTS_SHARED_LIBS -- otherwise problems building shared libs on Macs (at least)
SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

option(INCLUDE_DRIVERS_IN_ALL "Include drivers in make all (set to ON for IDE project generation)" OFF)

IF(INCLUDE_DRIVERS_IN_ALL)
  SET(EXCLUDE_DRIVERS_FROM_ALL "")
ELSE()
  SET(EXCLUDE_DRIVERS_FROM_ALL "EXCLUDE_FROM_ALL")
ENDIF(INCLUDE_DRIVERS_IN_ALL)

#IF(APPLE)
#   SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
#       "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -flat_namespace -undefined suppress")
#ENDIF(APPLE)

# Set Camellia directories
set(CAMELLIA_SOURCE_DIR src)
set(DRIVER_DIR drivers)

if (EXTRA_INCLUDE)
  include_directories(${EXTRA_INCLUDE})
endif()

if (OMP_INCLUDE)
  include_directories(${OMP_INCLUDE})
endif()

if(SCALAPACK_LIB)
  link_libraries(${SCALAPACK_LIB})
endif()

IF(NOT BUILD_SHARED_LIBS)
  MESSAGE("Setting BUILD_SHARED_LIBS to ${Trilinos_BUILD_SHARED_LIBS}")
  SET(BUILD_SHARED_LIBS ${Trilinos_BUILD_SHARED_LIBS})
ENDIF()

# Find all library source files
FILE(GLOB_RECURSE LIB_SOURCES "${CAMELLIA_SOURCE_DIR}/*.cpp" "${CAMELLIA_SOURCE_DIR}/include/*.h")
set(HEADERS 
  ${CAMELLIA_SOURCE_DIR}/include
)

include_directories(
  ${Trilinos_TPL_INCLUDE_DIRS}
  ${Trilinos_INCLUDE_DIRS}
  ${Moab_INCLUDE_DIRS}
  ${Mumps_INCLUDE_DIRS}
  ${CAMELLIA_SOURCE_DIR}/include
  TPL/choice
)

add_library(Camellia ${LIB_SOURCES} ${HEADERS})

link_libraries(
 Camellia
  ${Moab_LIBRARIES}
  ${Trilinos_LIBRARIES}
  ${Trilinos_TPL_LIBRARIES}
  ${Trilinos_LINALG_LIBRARIES}
  ${ADDITIONAL_LIBRARIES}
#  # Xdmd
#  # hdf5_cpp
#  #  dmumps
#  #  mumps_common
#  #  pord
)

FILE(GLOB ALL_HEADER_FILES "${HEADERS}/*.h" "${HEADERS}/*.hpp")
install (TARGETS Camellia DESTINATION lib)
install (FILES ${ALL_HEADER_FILES} DESTINATION include/Camellia)

# Enable testing (see unit_tests directory)
include(CTest)
enable_testing()

# Build Drivers
add_subdirectory(drivers ${EXCLUDE_DRIVERS_FROM_ALL})
add_subdirectory(examples)
add_subdirectory(unit_tests)
#add_subdirectory(slow_tests)

# Finish up
set(INSTALL_CONFIG_HEADERS FALSE)
include(${CMAKE_SOURCE_DIR}/../scimake/SciFinalize.cmake)
