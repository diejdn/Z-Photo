cmake_minimum_required(VERSION 3.20)
project(ImageIndexer)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定Qt路径
set(CMAKE_PREFIX_PATH "D:/Qt/6.7.3/mingw_64")

# Qt自动MOC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# 查找Qt模块
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets)

# 可执行程序
add_executable(ImageIndexer
    ImageIndexer_statis.cpp
)

target_include_directories(ImageIndexer PRIVATE
    ${PROJECT_SOURCE_DIR}/sqliteApi
)
# 库搜索路径 target_link_libraries 写 sqlite3，CMake 自动识别 libsqlite3.a；
target_link_directories(ImageIndexer PRIVATE
    ${PROJECT_SOURCE_DIR}/sqliteApi
)

# 链接库
target_link_libraries(ImageIndexer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
    Qt6::Multimedia Qt6::MultimediaWidgets
	sqlite3
)