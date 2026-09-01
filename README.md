# SimpleFrpPanel

基于 **Qt 5.14.2 + [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools)（FluentUI 风格组件库）** 的 FRP 可视化管理系统。
在一个界面里同时管理 **frp 服务端（frps）** 与 **frp 客户端（frpc）**：用户账号、隧道配置、
端口配额、流量统计一应俱全，支持 Windows 系统托盘后台运行。

![License: MIT](https://img.shields.io/badge/license-MIT-green)

## 功能特性

- **服务端 · 用户管理**
  - 账号数据库文件管理（`data/*.db`，随机 SHA256 前 10 位命名，新建/删除/选择）
  - 用户增删查改：加盐 SHA-256 密码摘要、启用状态、**到期时间**
  - 公网 IP / 端口设置（客户端登录地址）
- **服务端 · 隧道管理**
  - 按用户管理隧道（tcp / udp / http / https，域名隧道）
  - **端口配额模型**：服务端为每个用户界定远端端口范围、本地端口范围与最大端口数；
    具体端口由客户端在范围内自选；隧道增改时**双重端口检测**（库内唯一 + 本机实际占用）
  - 集成 **frps**：选择 frps.exe 一键启停、自动生成 `frps.toml`（bindPort / token /
    allowPorts 范围白名单 / webServer 仪表盘）、运行状态灯、实时日志、变更热重启
  - **面板服务**：公网端口上提供 TCP + JSON API，供客户端登录与隧道管理
- **服务端 · 流量统计**
  - 通过 frps 仪表盘 API 每 10 秒采样，按 **用户+隧道+日期** 累加流量记录
  - 按用户 / 隧道（**含已删除**）/ 日期区间查询接收、发送、合计流量，"全部时间"即历史总流量
- **客户端 · 隧道管理**
  - 服务器地址 / 用户名 / 密码登录（禁用、过期账号自动拒绝），登录后显示端口配额
  - 我的隧道：行内开关启停、增删查改、运行状态灯、配额用量实时显示
  - 集成 **frpc**：选择 frpc.exe 一键启停、自动生成 `frpc.toml`、状态灯、日志、变更热重启
- **通用**
  - Ela 深色/浅色主题跟随；点关闭按钮隐藏到**系统托盘**后台运行，右键托盘"退出"才真正退出
  - 各面板切换即刷新 + 轮询自动同步；隧道编辑对话框填写快照保存/还原

## 系统要求

| 组件 | 说明 |
| --- | --- |
| 操作系统 | Windows 7 及以上（x64） |
| frps.exe | frp 服务端程序，从 [frp Releases](https://github.com/fatedier/frp/releases) 下载（服务端使用） |
| frpc.exe | frp 客户端程序，同上（客户端使用） |

> 面板自身**不捆绑** frps/frpc，使用时在界面上选择本机已下载的 exe 即可。

## 快速开始

1. 下载 Release 包并解压，运行 `SimpleFrpPanel.exe`
2. **服务端**：
   - 服务端 · 用户管理：`新建` 一个数据库 → `新增用户` → 填写**公网 IP 与端口**并保存
   - 服务端 · 隧道管理：选择数据库/用户 → 设置端口配额 → 选择 `frps.exe` → `启动`；
     点"面板服务"的 `启动服务`（公网端口）
   - 新增隧道（如 tcp：远端端口 15001 → 内网 192.168.1.10:80）
3. **客户端**（可为本机或另一台机器）：
   - 客户端 · 隧道管理：填写服务器地址/端口（= 服务端公网端口）与账号密码 → `登录`
   - 选择 `frpc.exe` → `启动`，隧道即开始转发
4. 访问 `公网IP:远端端口` 即可到达内网服务；流量自动累计，可在服务端 · 流量统计查看

## 从源码构建

### 环境

| 组件 | 版本 | 备注 |
| --- | --- | --- |
| Qt | 5.14.2 (mingw73_64) | 需包含 Sql、Network 模块（官方安装包自带） |
| 编译器 | MinGW GCC 7.3（64 位） | 与 Qt 套件配套 |
| CMake | >= 3.12 | — |

### 步骤

```powershell
# 1. 克隆并拉取子模块（ElaWidgetTools）
git clone --recursive https://github.com/CCA8798/SimpleFrpPanel.git
cd SimpleFrpPanel

# 2. 配置（把 <QT_INSTALL> 替换为你本机的 Qt 安装根目录，见下方"如何找到 Qt 路径"）
$env:PATH = "<QT_INSTALL>\Tools\mingw730_64\bin;" +
            "<QT_INSTALL>\5.14.2\mingw73_64\bin;" + $env:PATH
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
      -DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64

# 3. 编译（完成后自动 windeployqt 部署运行库到 exe 目录）
cmake --build build -j

# 4. 运行
build/SimpleFrpPanel.exe
```

**如何找到 Qt 路径**：安装 Qt 时安装器会让你选择安装目录（默认形如 `C:\Qt\Qt5.14.2`），
它就是 `<QT_INSTALL>`。安装完成后该目录下应同时存在两类子目录：

- `<QT_INSTALL>\5.14.2\mingw73_64\` —— **Qt 套件根目录**（`QT_SDK_DIR` 指向这里，内含 `bin\qmake.exe`）
- `<QT_INSTALL>\Tools\mingw730_64\` —— 安装 Qt 时勾选的**配套 MinGW 工具链**（内含 `bin\gcc.exe`）

如果安装时没勾选 MinGW 工具链，请重新运行安装器补装（组件列表选择对应版本的
"MinGW 7.3.0 64-bit"）。不确定路径时，也可直接使用开始菜单中的
**"Qt 5.14.2 (MinGW 7.3.0 64-bit)"** 命令提示符——它已自动配置好 PATH，进入项目目录
直接执行 `cmake` 即可，无需手动设置 `$env:PATH`。

> - 未传 `-DQT_SDK_DIR` 时，CMake 会回退到本机默认 Qt 套件；配置阶段会打印实际使用的
>   Qt 版本，并校验版本（5.12 ~ 6.7.0）与编译器/ABI 匹配，出错会给出明确提示。
> - 构建产物自包含：exe 同目录已包含 Qt 运行库、平台插件、`ElaWidgetTools.dll` 与
>   MinGW 运行时，拷贝整个目录即可分发，无需配置 PATH。

### CLion 使用

1. `File → Open` 选择根目录 `CMakeLists.txt`
2. `Settings → Build, Execution, Deployment → Toolchains` 新建 MinGW 工具链，
   编译器指向你的 MinGW 安装路径下的 `bin\gcc.exe`（即上文的
   `<QT_INSTALL>\Tools\mingw730_64\bin\gcc.exe`；在 Qt 安装目录的 `Tools\` 文件夹中查找）
3. CMake profile 无需额外传参（自动回退本机 Qt），也可显式加
   `-DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64`（指向你的 Qt 套件根目录）
4. 构建运行即可（运行库自动部署）

## 架构说明

```
SimpleFrpPanel/
├── CMakeLists.txt              # 顶层构建脚本（AUTOMOC/AUTOUIC/AUTORCC + 自动部署）
├── include/ElaWidgetTools/     # git submodule：ElaWidgetTools（MIT）
└── src/
    ├── main.cpp                # 入口：QApplication + ElaApplication 初始化
    ├── MainWindow.h/.cpp       # MainWindow : ElaWindow（导航框架 + 系统托盘）
    ├── DatabaseManager.h/.cpp  # SQLite 数据层（users/tunnels/settings/traffic_records）
    ├── UserEditDialog.h/.cpp   # 用户编辑对话框（Ela 组件）
    ├── TunnelEditDialog.h/.cpp # 隧道编辑对话框（协议联动 + 填写快照）
    ├── PortChecker.h/.cpp      # 端口占用检测（TCP/UDP 试绑定）
    ├── FrpsManager.h/.cpp      # frp 进程管理（frps/frpc 共用：配置生成/启停/日志）
    ├── PanelApiServer.h/.cpp   # 面板 API 服务端（TCP + JSON 行协议，token 认证）
    ├── PanelClient.h/.cpp      # 面板 API 客户端
    ├── TrafficMonitor.h/.cpp   # 流量采样（frps 仪表盘 API → 流量记录）
    ├── StatusLight.h/.cpp      # 状态灯控件
    ├── StatusDotDelegate.h/.cpp# 表格状态灯委托
    └── *Page.h/.cpp/.ui        # 各管理页面（Designer 可编辑）
```

**页面与 .ui 的关系**：`ElaWindow` 主窗口在代码中搭建；各页面是普通 `QWidget` + 独立 `.ui`，
可在 **Qt Designer** 中直接编辑。Ela 控件通过**提升法**放入 .ui（如把 `QPushButton` 提升为
`ElaPushButton`，头文件 `ElaPushButton.h`），编译时自动解析、运行时为真实 Ela 组件。
官方 ElaWidgetTools main 分支不提供 Designer 插件，画布中以普通控件显示、运行效果不受影响。

## 数据与配置

| 位置 | 内容 |
| --- | --- |
| `<程序目录>/data/*.db` | 账号数据库（随机 SHA256 前 10 位命名，SQLite，WAL 模式） |
| `<程序目录>/data/<库名>.frps.toml` | 自动生成的 frps 配置 |
| `<程序目录>/frpc.toml` | 自动生成的 frpc 配置（客户端） |
| `<程序目录>/config.ini` | frps/frpc 路径、客户端上次登录信息、隧道草稿快照 |

数据库表：`users`（账号+配额）、`tunnels`（隧道，删除用户级联）、`settings`（键值）、
`traffic_records`（流量记录，用户/隧道删除后仍保留）。

## 常见问题

- **frps 启动报 `bind: Only one usage of each socket address`**：端口被占用，通常是上次
  异常退出残留的 frps 进程。结束占用进程，或在界面修改端口（绑定端口 / Web 仪表盘端口）后重试。
- **流量统计查不到数据**：先看服务端隧道页日志区——出现"流量采样已连接 frps 仪表盘"表示
  链路正常，需等 >20 秒（首轮采样为基准）且有流量经过；出现"流量采样失败"表示 frps 未运行
  或 webServer 未生效（请停止后重新启动 frps 以重新生成配置）。面板直连本机 API，不受系统代理影响。
- **客户端登录按钮置灰**：输入服务器地址/端口/账号密码后点击登录（按钮在未登录时始终可点）。
- **切换页面后客户端掉线**：面板服务仅在数据库或端口真正变化时才重启，普通切页不会踢客户端。

## 技术要点

- 面板 API：TCP + JSON 行协议，登录签发随机 token，隧道操作带**归属校验**与**配额强制**
- SQLite 多连接并发：WAL 模式 + busy_timeout=5000，流量写入与查询互不阻塞
- 界面流畅性：轮询采用状态签名变化检测，数据未变零开销；表格增量更新保持滚动与选择

## 许可证

[MIT](LICENSE) © 2025 CCA8798

ElaWidgetTools 组件库基于 [MIT](https://github.com/Liniyous/ElaWidgetTools) 协议，使用请保留其版权声明。
frp 基于 [Apache-2.0](https://github.com/fatedier/frp) 协议，由 [fatedier/frp](https://github.com/fatedier/frp) 提供。
