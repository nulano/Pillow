#!/usr/bin/env bash
# install raqm


archive=raqm-cmake-99300ff3
directory=libraqm-cmake-99300ff3dd408b8593e321fa2c62df97d74ff128

./download-and-extract.sh $archive https://raw.githubusercontent.com/python-pillow/pillow-depends/master/$archive.tar.gz

pushd $directory

mkdir build
cd build
cmake ..
make && sudo make install
cd ..

popd

