cmake_minimum_required(VERSION 3.5)

project(msg_parser LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt5 Widgets
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# 源文件
set(SOURCES
    main.cpp
    MsgParserWindow.cpp
    FrameParser.cpp
)

set(HEADERS
    MsgParserWindow.h
    FrameParser.h
)

# 创建可执行文件
add_executable(msg_parser ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(msg_parser
    lib_xuji103
    Qt5::Core
    Qt5::Widgets
)

# MSVC设置
if(MSVC)
    target_compile_options(msg_parser PRIVATE /utf-8 /W3)
endif()
