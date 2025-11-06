project(csv-to-netcdf
  LANGUAGES C CXX
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
# set(CMAKE_BUILD_TYPE Debug)


set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "./.external/")
list(APPEND CMAKE_PREFIX_PATH "./$HOME/local/")


# find_package(HDF5 REQUIRED)
# find_package(netCDF REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(CLI11)
set(CLI11_PRECOMPILED)

add_executable(csv-to-netcdf
  src/parsing.cpp
  src/csv.cpp
  src/csv-to-netcdf.cpp
)
target_link_libraries(csv-to-netcdf PRIVATE 
  $ENV{HOME}/local/lib/libnetcdf.so
  # $ENV{HOME}/local/lib/libhdf5.so
  # $ENV{HOME}/local/lib/libnetcdf.a
  # $ENV{HOME}/local/lib/libhdf5_hl.a
  # $ENV{HOME}/local/lib/libhdf5.a
  # xml2
  # z
  # curl
  CLI11::CLI11 
  # spdlog::spdlog
)
target_include_directories(csv-to-netcdf PRIVATE 
  indicators/include
  $ENV{HOME}/local/include
)