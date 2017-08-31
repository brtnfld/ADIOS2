# Client maintainer: chuck.atkins@kitware.com
set(CTEST_SITE "CircleCI")
set(CTEST_BUILD_NAME "$ENV{CIRCLE_BRANCH}_$ENV{CIRCLE_JOB}")
set(CTEST_BUILD_CONFIGURATION Release)
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_BUILD_FLAGS "-k -j4")
set(CTEST_TEST_ARGS PARALLEL_LEVEL 4)

set(dashboard_model Experimental)
set(dashboard_binary_name "build_$ENV{CIRCLE_JOB}")

set(CTEST_SOURCE_DIRECTORY "$ENV{CIRCLE_WORKING_DIRECTORY}/source")
set(CTEST_DASHBOARD_ROOT "$ENV{HOME}")

include(${CMAKE_CURRENT_LIST_DIR}/EnvironmentModules.cmake)
module(purge)
module(load gnu7)
module(load openmpi)
module(load phdf5)
module(load python)

set(ENV{CC}  gcc)
set(ENV{CXX} g++)
set(ENV{FC}  gfortran)

set(dashboard_cache "
ADIOS2_USE_ADIOS1:STRING=ON
ADIOS2_USE_BZip2:STRING=ON
ADIOS2_USE_DataMan:STRING=ON
ADIOS2_USE_Fortran:STRING=ON
ADIOS2_USE_HDF5:STRING=ON
ADIOS2_USE_MPI:STRING=ON
ADIOS2_USE_Python:STRING=OFF
ADIOS2_USE_ZFP:STRING=ON
ADIOS2_USE_ZeroMQ:STRING=ON
ZFP_ROOT_DIR:PATH=/opt/zfp/install
ADIOS1_ROOT:PATH=/opt/adios1/1.12.0/gnu7_openmpi
")

include(${CMAKE_CURRENT_LIST_DIR}/../dashboard/adios_common.cmake)