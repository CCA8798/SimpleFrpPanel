cmake_minimum_required(VERSION 3.12)

project(SimpleFrpPanel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Qt SDK 路径。示例：
#   cmake -S . -B build -DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64
# 也可以直接使用 -DCMAKE_PREFIX_PATH=<Qt 目录>
# ---------------------------------------------------------------------------
set(QT_SDK_DIR "" CACHE PATH "Qt SDK directory (e.g. F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64)")
if(QT_SDK_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${QT_SDK_DIR}")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Windows SDK 版本宏：ElaWidgetTools 的 ElaWinShadowHelper 在 Qt 设置 WINVER 之前
# 就包含了 <windows.h>，MinGW 默认 WINVER 过低会导致 SM_CXPADDEDBORDER /
# USER_DEFAULT_SCREEN_DPI 等常量缺失（与 Qt 5.14 自身取值保持一致）
if(WIN32)
    add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601)
endif()

# ---------------------------------------------------------------------------
# ElaWidgetTools：以 git submodule 方式链接到官方仓库 include/ElaWidgetTools
# 链接目标：Ela::WidgetTools（共享库，DLL 输出到构建目录）
# ---------------------------------------------------------------------------
add_subdirectory(include/ElaWidgetTools/ElaWidgetTools)

add_executable(SimpleFrpPanel WIN32
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/HomePage.h
    src/HomePage.cpp
    src/HomePage.ui
)

# 使 uic 能解析 .ui 中"提升"的 Ela 控件头文件（ElaPushButton.h 等）
target_include_directories(SimpleFrpPanel PRIVATE src)

target_link_libraries(SimpleFrpPanel PRIVATE Ela::WidgetTools)

if(MSVC)
    target_compile_options(SimpleFrpPanel PRIVATE /utf-8)
endif()
