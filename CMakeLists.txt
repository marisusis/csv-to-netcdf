project(csv-to-netcdf
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_PREFIX_PATH "./.external/")

find_package(HDF5 REQUIRED)
find_package(netCDF REQUIRED)

add_subdirectory(CLI11)

add_executable(csv-to-netcdf
  src/csv-to-netcdf.cpp
)
target_link_libraries(csv-to-netcdf PRIVATE netCDF::netcdf)
