cd ext
git clone git@github.com:imzhenyu/googletest.git
cd ..

cd codegen
git clone git@github.com:imzhenyu/protobuf.git
mkdir build

cd build
cmake ..
make -j 10
cp protobuf/cmake/protoc ../../bin/tools/bin/
cd ..

cd ..
