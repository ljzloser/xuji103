@echo off
call "E:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cd /d D:\project\C++\xuji103\build
cmake .. -G Ninja
ninja