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
# 未显式指定时，回退到本机默认 Qt 套件（CLion/IDE 未传参也能找到正确的 Qt）；
# 其他机器请通过 -DQT_SDK_DIR= 显式指定。
# ---------------------------------------------------------------------------
set(QT_SDK_DIR "" CACHE PATH "Qt SDK directory (e.g. F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64)")
if(NOT QT_SDK_DIR AND EXISTS "F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64/bin/qmake.exe")
    set(QT_SDK_DIR "F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64")
    message(STATUS "QT_SDK_DIR 未指定，回退到本机默认 Qt 套件: ${QT_SDK_DIR}")
endif()
if(QT_SDK_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${QT_SDK_DIR}")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)

if(QT_QMAKE_EXECUTABLE)
    message(STATUS "使用 Qt ${QT_VERSION}（${QT_QMAKE_EXECUTABLE}）")
else()
    message(STATUS "使用 Qt ${QT_VERSION}")
endif()

# Qt 版本校验：ElaWidgetTools 支持范围 5.12 ~ 6.7.0
# （防止像 Qt 5.9.1 这样的旧版本被 PATH 中的环境变量误选中，报错信息明确）
if(QT_VERSION VERSION_LESS 5.12 OR QT_VERSION VERSION_GREATER 6.7.0)
    message(FATAL_ERROR
        "找到的 Qt 版本为 ${QT_VERSION}（${QT_QMAKE_EXECUTABLE}），不在 ElaWidgetTools 支持范围"
        "（5.12 ~ 6.7.0）内。\n"
        "请设置 -DQT_SDK_DIR 指向正确的 Qt 套件后重新配置，例如：\n"
        "  cmake -S . -B build -DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64")
endif()

# 编译器 / Qt ABI 校验：MinGW 编译器必须搭配 MinGW 版 Qt，MSVC 同理
if(QT_QMAKE_EXECUTABLE)
    string(TOLOWER "${QT_QMAKE_EXECUTABLE}" _QT_QMAKE_LOWER)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND _QT_QMAKE_LOWER MATCHES "msvc")
        message(FATAL_ERROR
            "当前编译器为 MinGW，但找到的 Qt 是 MSVC 版（${QT_QMAKE_EXECUTABLE}），"
            "无法链接。\n请设置 -DQT_SDK_DIR 指向 MinGW 版 Qt，例如：\n"
            "  -DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64")
    elseif(MSVC AND _QT_QMAKE_LOWER MATCHES "mingw")
        message(FATAL_ERROR
            "当前编译器为 MSVC，但找到的 Qt 是 MinGW 版（${QT_QMAKE_EXECUTABLE}），"
            "无法链接。\n请设置 -DQT_SDK_DIR 指向 MSVC 版 Qt（如 msvc2022_64）。")
    endif()
endif()

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
    src/ServerTunnelPage.h
    src/ServerTunnelPage.cpp
    src/ServerTunnelPage.ui
    src/ServerUserPage.h
    src/ServerUserPage.cpp
    src/ServerUserPage.ui
    src/ClientTunnelPage.h
    src/ClientTunnelPage.cpp
    src/ClientTunnelPage.ui
    src/DatabaseManager.h
    src/DatabaseManager.cpp
    src/UserEditDialog.h
    src/UserEditDialog.cpp
)

# 使 uic 能解析 .ui 中"提升"的 Ela 控件头文件（ElaPushButton.h 等）
target_include_directories(SimpleFrpPanel PRIVATE src)

target_link_libraries(SimpleFrpPanel PRIVATE Ela::WidgetTools Qt${QT_VERSION_MAJOR}::Sql)

if(MSVC)
    target_compile_options(SimpleFrpPanel PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------------------
# 运行库自动部署（Windows）：
# 1) ElaWidgetTools.dll 复制到可执行文件目录
# 2) windeployqt 复制 Qt 运行库与平台插件（platforms/qwindows.dll 等）
# 保证从 CLion / 命令行 / 双击运行时都不依赖 PATH，避免 0xC0000135 报错
# ---------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET SimpleFrpPanel POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_FILE:ElaWidgetTools>
            $<TARGET_FILE_DIR:SimpleFrpPanel>/ElaWidgetTools.dll
        COMMENT "复制 ElaWidgetTools.dll 到可执行文件目录"
    )

    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS "${QT_SDK_DIR}/bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET SimpleFrpPanel POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E env
                "PATH=${QT_SDK_DIR}/bin;${MINGW_BIN};$ENV{PATH}"
                "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                $<TARGET_FILE:SimpleFrpPanel>
            COMMENT "windeployqt 部署 Qt 运行库"
        )
    else()
        message(WARNING "未找到 windeployqt，跳过 Qt 运行库部署；运行前需将 Qt 的 bin 目录加入 PATH")
    endif()
endif()
