cmake_minimum_required(VERSION 3.20)
project(QtStudy)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.7.3/mingw_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
qt_standard_project_setup()

qt_add_executable(MediaHttpServer
	main.cpp
	MediaHttpServer.h
	MediaHttpServer.cpp
)

target_link_libraries(MediaHttpServer
    PRIVATE
        Qt6::Core Qt6::Network
)