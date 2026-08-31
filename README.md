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
    ├── DatabaseManager.h/.cpp  # 账号数据库管理（data/*.db：SHA256 文件名 + users/settings/tunnels 表）
    ├── UserEditDialog.h/.cpp   # 新增/修改用户对话框
    ├── FrpsManager.h/.cpp      # frps 进程管理（配置生成 / 启停 / 日志转发）
    ├── TunnelEditDialog.h/.cpp # 新增/修改隧道对话框
    ├── StatusDotDelegate.h/.cpp  # 隧道运行状况状态灯委托（绿=运行中/灰=未运行/橙=已禁用）
    ├── PanelApiServer.h/.cpp   # 面板 API 服务端（TCP + JSON 行协议，token 认证，配额强制）
    ├── PanelClient.h/.cpp      # 面板 API 客户端（登录/隧道 CRUD/启停，请求序号关联）
    ├── TrafficMonitor.h/.cpp   # 流量监控（轮询 frps 仪表盘 API，增量写入流量记录表）
    ├── TrafficPage.h/.cpp/.ui  # 服务端 · 流量统计（按用户/隧道/日期区间查询，含已删除）
    ├── HomePage.h/.cpp/.ui     # 首页
    ├── ServerTunnelPage.h/.cpp/.ui  # 服务端 · 隧道管理（库/用户联动 + 配额 + frps 控制 + 面板服务 + 日志）
    ├── ServerUserPage.h/.cpp/.ui    # 服务端 · 用户管理（db 文件管理 + 用户 CRUD + 公网 IP/端口设置）
    └── ClientTunnelPage.h/.cpp/.ui   # 客户端 · 隧道管理（服务器登录 + 我的隧道启停/增删查改 + 配额显示 + 日志）
```

> 账号数据库文件存放在 **`<程序运行目录>/data`** 下（运行时生成，已加入 .gitignore），
> 每个文件以随机 SHA256 前 10 位十六进制命名，内含 `users` 表（用户名/加盐密码摘要/
> 备注/启用状态/到期时间/创建时间/**端口配额**）、`tunnels` 表（每个用户的隧道：名称/协议
> tcp|udp|http|https/远端端口/目标 IP/目标端口/自定义域名/启用状态/备注，删除用户时级联删除）
> 与 `settings` 表（键值设置：`public_ip`、`public_port`、`frps_bind_port`、`frps_token`）。
> 构建依赖 Qt 的 **Sql** 模块（QSQLITE 驱动由 windeployqt 自动部署）。

> **端口配额模型**：服务端为每个用户界定**远端端口范围**、**本地端口范围**与**最大可用端口数**
> （默认 10000-60000 / 1024-65535 / 10，可在服务端隧道页按用户修改）。**具体使用哪个端口由
> 客户端在范围内自选**；隧道新增/修改时由数据库层统一强制校验（端口越界、tcp/udp 数量超限
> 直接拒绝，禁用中的隧道不占用配额，http/https 走域名不计入数量）。frps 的 `allowPorts`
> 白名单按各用户的远端端口范围生成，服务端进程层面兜底。

> **frps 集成**：服务端隧道页可选择本机 `frps.exe` 并启动/停止，面板自动生成
> `data/<库名>.frps.toml`（bindPort + auth.token + 各用户远端端口范围白名单 allowPorts），
> 配额或隧道变更后若 frps 在运行会自动热重启；运行日志实时显示在页面底部。
> frps.exe 路径持久化在程序目录 `config.ini`（QSettings）。

> **客户端-服务端集成**：服务端隧道页的"面板服务"在**公网端口**（用户管理页设置）上提供
> TCP + JSON 行协议 API（`PanelApiServer`）：登录（用户名/密码，禁用/过期账号拒绝，签发
> token）、隧道列表（含运行状况）、增删改、启停；所有隧道操作在服务端做配额与**归属校验**
> （只能操作自己的隧道）。客户端隧道页通过 `PanelClient` 连接登录后，在配额范围内自选端口
> 管理自己的隧道（行内开关启停、状态灯显示、配额用量实时显示）。**客户端同时集成 frpc**：
> 选择本机 `frpc.exe` 后自动生成 `frpc.toml`（服务器地址/端口/token 由登录响应下发，
> proxies 来自启用的隧道），启动/停止/状态灯/日志一应俱全，隧道变更时自动热重启——
> 至此 内网服务（frpc）↔ 公网服务器（frps）↔ 外网访问 全链路可真实打通。
> **流量记录**：frps.toml 自动启用仪表盘 `webServer`（端口 7500，密码随机生成存于数据库
> 设置）；`TrafficMonitor` 每 10 秒轮询 `GET /api/proxy/{type}/{name}`（Basic 认证），
> 取 `todayTrafficIn/Out` 增量，按 **用户+隧道+日期** 累加写入 `traffic_records` 表
> （用户/隧道名快照，**删除后记录仍保留**）。服务端 · 流量统计页支持按 用户/隧道
> （含已删除）/日期区间 查询接收/发送/合计流量，"全部时间"即历史总流量，表格 5 秒自动刷新。
> 构建依赖 Qt 的 **Sql + Network** 模块。

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
