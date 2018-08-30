echo off
mkdir Local
pushd Local
cmake ../ -G "Visual Studio 15 2017 Win64"
popd