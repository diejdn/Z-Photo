cmake_minimum_required(VERSION 3.20)
project(QtStudy)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.7.3/mingw_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets)
qt_standard_project_setup()

# 定义变量，统一管理可执行文件名
set(TARGET_NAME Z-Photo)
set(MAIN_CPP z-photo.cpp)

qt_add_executable(${TARGET_NAME}
	${MAIN_CPP}
	sqliteApi/sqlite_api.cpp
	MediaLabel/MediaLabel.cpp
	ImagePlayer/ImageGraphicsView.cpp
	ImagePlayer/ImagePlayer.cpp
	VideoPlayer/VideoPlayer.cpp
	ZLens/ZLens.cpp
	ImageGridWidget/ImageGridWidget.cpp
	AddTagWindow/AddTagWindow.cpp
	DateTimeUtils/DateTimeUtils.cpp
	AppConfig/AppConfig.cpp
	AppConfig/StartupDialog.cpp
	resources.rc
)

# 添加资源，在qt_add_executable之后
qt_add_resources(RESOURCES style.qrc)
target_sources(${TARGET_NAME} PRIVATE ${RESOURCES})

# ========= sqlite3 静态库配置 =========
# 头文件目录 指定 .h 路径，外部代码 #include "sqlite3.h" 正常找到。
# PROJECT_SOURCE_DIR是CMakeList.txt所在的目录
# PROJECT_BINARY_DIR：构建输出目录（build 目录），不要和上面混淆。
# libsqlite3.a和sqlite3.h放在PROJECT_SOURCE_DIR/sqlite3中
#  包含目录：同时包含 sqliteApi 用来找 sqlite_api.h 和 sqlite3.h
target_include_directories(${TARGET_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/sqliteApi
	${PROJECT_SOURCE_DIR}/MediaLabel
	${PROJECT_SOURCE_DIR}/ImagePlayer
	${PROJECT_SOURCE_DIR}/VideoPlayer
	${PROJECT_SOURCE_DIR}/ZLens
	${PROJECT_SOURCE_DIR}/ImageGridWidget
	${PROJECT_SOURCE_DIR}/AddTagWindow
	${PROJECT_SOURCE_DIR}/DateTimeUtils
	${PROJECT_SOURCE_DIR}/AppConfig
	${PROJECT_SOURCE_DIR}/limitRandom
)
# 库搜索路径 target_link_libraries 写 sqlite3，CMake 自动识别 libsqlite3.a；
target_link_directories(${TARGET_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/sqliteApi
)
# 链接静态库 + Windows上sqlite3依赖ws2_32
target_link_libraries(${TARGET_NAME}
    PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network
		Qt6::Multimedia Qt6::MultimediaWidgets
        sqlite3
		ws2_32
		pthread
)