# Building

## netcdf
cmake -DCMAKE_INSTALL_PREFIX=../.external -DCMAKE_PREFIX_PATH=../.external/hdf5 -B build .
cmake -DCMAKE_INSTALL_PREFIX=../.external -DCMAKE_PREFIX_PATH=../.external/hdf5 -D"BUILD_SHARED_LIBS=ON" -B build .