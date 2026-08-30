# SimpleFrpPanel

FRP 客户端面板（脚手架）。基于 **Qt 5.14.2 + ElaWidgetTools（FluentUI 风格组件库）**，
采用 CMake 构建，ElaWidgetTools 以 **git submodule** 方式链接官方仓库。

## 环境

| 组件 | 版本 | 路径 |
| --- | --- | --- |
| Qt | 5.14.2 (mingw73_64) | `F:\Software\Qt5\Qt5.14.2\5.14.2\mingw73_64` |
| 编译器 | MinGW GCC 7.3 (64 位) | `F:\Software\Qt5\Qt5.14.2\Tools\mingw730_64\bin` |
| CMake | >= 3.12 | — |

## 目录结构

```
SimpleFrpPanel/
├── CMakeLists.txt              # 顶层构建脚本
├── .gitmodules                 # 子模块声明（ElaWidgetTools）
├── include/                    # 引用的第三方库（子模块，指向官方仓库）
│   └── ElaWidgetTools/         #  https://github.com/Liniyous/ElaWidgetTools
└── src/                        # 项目源码（.h / .cpp / .ui）
    ├── main.cpp                # 入口：QApplication + eApp->init()
    ├── MainWindow.h/.cpp       # 主窗口：MainWindow : ElaWindow（导航框架）
    └── HomePage.h/.cpp/.ui     # 中央页面：普通 QWidget 绑定 .ui（Designer 编辑）
```

## 架构说明（Ela 主窗口 + ui 页面）

1. **主窗口**：`MainWindow` 继承 `ElaWindow`（无边框、自绘标题栏、左侧导航栏），
   导航页面在代码中通过 `addPageNode()` 注册。
2. **中央页面**：每个页面是**普通 `QWidget` 子类**（如 `HomePage`），界面由对应的
   `HomePage.ui` 描述，构造时 `m_Ui->setupUi(this)` 绑定。
3. **提升法使用 Ela 控件**：`ElaWindow` 自身不适合在 Designer 中编辑，所以把 Ela
   控件放在页面 `.ui` 中，用 Designer 的"提升（Promote）"功能把普通控件提升为
   Ela 类。编译时 `uic` 生成的代码会 `#include` 提升类的头文件（已通过
   `Ela::WidgetTools` 目标的 include 路径解析），链接时实例化真实的 Ela 类。

## Qt Designer 可视化编辑（提升法）

官方仓库 main 分支**不再提供 Designer 插件**，因此用"提升法"替代：

1. 用 Qt Designer 打开页面：`F:\Software\Qt5\Qt5.14.2\5.14.2\mingw73_64\bin\designer.exe src\HomePage.ui`
2. 从左侧控件栏拖入基础控件（如 `QPushButton`、`QLineEdit`、`QComboBox`…），
   右键 → **提升为 (Promote to)**：
   - 提升类名：`ElaPushButton`（或其他 Ela 类）
   - 头文件：`ElaPushButton.h`（即 `include/ElaWidgetTools/ElaWidgetTools/ElaPushButton.h`）
   - 点击 **添加** → **提升**
3. 保存后重新构建即可。运行时这些控件就是真实的 Ela 组件。

> 说明：未安装插件时，Designer 画布中提升后的控件仍按普通控件显示（无法预览
> FluentUI 外观、不能编辑 Ela 专属属性），但布局与基础属性可正常编辑，运行效果
> 不受影响。常用可提升类见 `include/ElaWidgetTools/ElaWidgetTools/` 下同名头文件，
> 例如：ElaPushButton / ElaLineEdit / ElaComboBox / ElaSwitchButton / ElaText /
> ElaProgressBar / ElaScrollArea / ElaTableView / ElaTreeView 等。

## 构建

```powershell
# 1. 拉取子模块（首次克隆仓库后执行）
git submodule update --init --recursive

# 2. 配置（MinGW 7.3 + Qt 5.14.2）
$env:PATH = "F:\Software\Qt5\Qt5.14.2\Tools\mingw730_64\bin;" +
            "F:\Software\Qt5\Qt5.14.2\5.14.2\mingw73_64\bin;" + $env:PATH
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug `
      -DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64

# 3. 编译
cmake --build build -j

# 4. 运行（build/ 下生成 SimpleFrpPanel.exe 与 ElaWidgetTools.dll）
build/SimpleFrpPanel.exe
```

> 构建完成后会自动把运行库部署到 exe 同目录（`windeployqt` 复制 Qt 运行库与
> 平台插件，并复制 `ElaWidgetTools.dll` 与 MinGW 运行时），因此从 CLion、
> 命令行或双击运行都不需要额外配置 PATH，不会出现 `0xC0000135`（DLL 缺失）报错。

> 未通过 `-DQT_SDK_DIR` 指定 Qt 时，CMake 会自动回退到本机默认 Qt 套件
> （`F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64`，仅本机路径存在时生效）。
> 配置阶段会打印实际使用的 Qt 版本并做两项硬校验：
> - **版本**：必须是 5.12 ~ 6.7.0（防止 PATH 里残留的旧 Qt 如 5.9.1 被误选，报错会给出明确提示）
> - **ABI**：MinGW 编译器必须搭配 MinGW 版 Qt，MSVC 同理

## CLion 使用

1. 打开项目（`File → Open`，选择根目录的 `CMakeLists.txt`）
2. 工具链：`Settings → Build, Execution, Deployment → Toolchains`，新建 MinGW
   工具链，编译器指向 `F:\Software\Qt5\Qt5.14.2\Tools\mingw730_64\bin\gcc.exe`
3. CMake 配置：`Settings → Build, Execution, Deployment → CMake`，选择该工具链
   的 profile，**无需**额外传参（Qt 路径会自动回退到本机默认套件）；
   若想显式指定，可在 "CMake options" 中添加：
   `-DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64`
4. 直接点构建/运行即可；运行时会自动带上 Qt 与 ElaWidgetTools 的 DLL 路径

## 子模块说明

`include/ElaWidgetTools` 是 git submodule，指向
<https://github.com/Liniyous/ElaWidgetTools>（MIT 协议）。更新上游：

```powershell
git submodule update --remote include/ElaWidgetTools
```

## 命名规范

- 类名 / 文件名：帕斯卡命名（`MainWindow`、`HomePage`）
- 成员变量：`m_` + 帕斯卡命名（`m_HomePage`、`m_Ui`）
- 局部变量 / 函数参数：小驼峰（`homePage`、`pageTitle`）
- UI 对象名：小驼峰（`mainLayout`）
- 头文件防护宏、文件名与类名保持一致（`MAINWINDOW_H`）
