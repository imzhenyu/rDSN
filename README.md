### (如何安装) How to build and install
```bash
# install pre-requirments
sudo yum install build-essential
sudo yum install git
sudo yum install cmake
sudo yum install libaio-dev
sudo yum install php-cli

# install rDSN
git clone ... rdsn
cd rdsn
./download_ext.sh
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=../release -DCMAKE_BUILD_TYPE=Debug
make -j 10 
ctest -V
make install
cd ..

# install protobuf
cd codegen/build
sudo make install
sudo ldconfig # refresh shared library cache.
cd ..
cd ..
```

### (如何使用) How to build applications 
```bash
cd tutorial
../release/bin/dsn.cg.sh counter.proto cpp ct
cd ct
mkdir build
cd build
cmake ..
make -j 10
./counter.multiapp bin/counter.multiapp.config.ini
```
