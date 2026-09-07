
<div align="center">

[**English**](README.en.md) | **简体中文**

# SimpleFrpPanel

![License: MIT](https://img.shields.io/badge/license-MIT-green)

基于 **Qt 5.14.2 + ElaWidgetTools** 的 FRP 可视化管理面板。

支持在一个界面中管理 `frps` 和 `frpc`，包括用户、隧道、端口配额、流量统计等功能，支持 Windows 系统托盘运行与中英文界面。

</div>

## 功能

### 首页 · 运行总览

- 2×2 自绘总览卡片，实时展示：
  - **运行状态**：frps / 面板服务 / 数据库 / 仪表盘
  - **数据统计**：用户数、隧道数（含启用数）
  - **流量统计**：今日与累计收发流量
  - **客户端会话**：登录状态、frpc 运行情况、我的隧道
- 服务端/客户端页面各自汇总状态至全局信息，首页每秒自动刷新

### 服务端

- 用户管理
  - SQLite 数据库管理
  - 用户增删改查、启用/禁用、到期时间
  - 盐值 + SHA-256 密码摘要
  - 公网 IP / 端口配置
- 隧道管理
  - TCP / UDP / HTTP / HTTPS 与域名隧道
  - 按用户设置端口配额（远端/本地范围 + 最大端口数）
  - 数据库端口冲突检测 + 本机 TCP/UDP 端口占用检测
- frps 管理
  - 使用内置 `frps.exe`（`程序目录\frp\`）或自定义选择
  - 自动生成 `frps.toml`、一键启停、配置变更自动重启
  - 运行状态与实时日志；绑定端口 / Token / 仪表盘端口**编辑即保存**
- 面板服务
  - TCP + JSON API，用户登录认证与隧道管理接口
  - 记忆上次选择的数据库与用户，重启后自动恢复

### 客户端

- 服务器地址/端口/账号密码登录（登录参数**含密码自动记忆**，重开自动填充）
- 显示端口配额与使用情况；隧道增删改查、启停、运行状态
- 使用内置 `frpc.exe`（`程序目录\frp\`）或自定义选择
- 自动生成 `frpc.toml`、一键启停、配置变更自动重启、实时日志

### 流量统计

通过 frps Web API 定时采样流量，按 **用户 + 隧道 + 日期** 保存；支持按用户、隧道、日期区间查询收发流量。已删除的用户/隧道，其历史流量记录仍保留。

### 设置 · 其他

- **首页右上角齿轮进入设置面板**：
  - 关闭窗口时是否询问「后台运行 / 直接退出」（可记忆选择，托盘菜单"退出"始终生效）
  - 界面语言：简体中文 / English（重启生效）
  - frp 更新代理（HTTP 代理，如 `127.0.0.1:7897`，留空走系统代理）
- **内置 frp 与自动更新**：`frpc.exe`/`frps.exe` 随程序放在 `程序目录\frp\`；每次启动联网检查 GitHub 新版本，缺失或发现新版均可**一键下载更新**（带进度）
- ElaWidgetTools 深/浅主题跟随；系统托盘后台运行
- 新增隧道弹窗按 服务端/客户端 **分组记忆上次填写**，打开自动填充
- 页面切换自动刷新 + 定时轮询同步

---

## 系统要求

| 组件 | 要求 |
| --- | --- |
| 操作系统 | Windows 7 及以上（x64） |
| Qt | 5.14.2（MinGW 7.3 64-bit，构建用） |
| CMake | >= 3.12（构建用） |
| frps / frpc | **随发布包内置**于 `frp\` 目录（版本 v0.71.0），也可手动放置或从 [frp Releases](https://github.com/fatedier/frp/releases) 获取 |

> frp 程序随包内置，目录约定：`<程序目录>\frp\frpc.exe` 与 `frps.exe`。程序启动会自动检查更新（需联网，可在设置中配置代理）；若缺失会弹窗询问一键下载。

---

## 快速使用

### 服务端

1. 运行 `SimpleFrpPanel.exe`，进入 **服务端 · 用户管理**：新建数据库 → 创建用户 → 设置公网 IP 与端口
2. 进入 **服务端 · 隧道管理**：设置端口配额 → 点 **启动**（frps 路径已自动指向内置 `frp\frps.exe`）→ 启动面板服务
3. 新增隧道，例如 TCP：远端端口 `15001` → 内网 `192.168.1.10:80`
4. 客户端连接后访问 `公网IP:15001` 即可到达内网服务

### 客户端

1. 进入 **客户端 · 隧道管理**：服务器地址/端口/账号/密码将自动填充上次登录参数
2. 点击 **登录**（首次需输入）；frpc 路径已自动指向内置 `frp\frpc.exe`
3. 点击 **启动** 即开始转发，隧道可在列表内开关

### 语言与更新

- **切换语言**：齿轮 → 界面语言 → 重启生效
- **frp 更新**：启动时自动检查；可在设置中填写 HTTP 代理以加速下载

---

## 国际化（i18n）

- 源码使用 Qt 标准 `tr()`，发布包内嵌 `SimpleFrpPanel_en.qm`（英文），默认中文
- 翻译源文件：`translations/SimpleFrpPanel_en.ts`
- 修改文案后更新翻译：

```powershell
# Qt 安装目录 bin 下
lupdate -recursive src -ts translations/SimpleFrpPanel_en.ts   # 重新提取
# 编辑 translations/SimpleFrpPanel_en.ts（Qt Linguist 或手工）补全英文
lrelease translations/SimpleFrpPanel_en.ts -qm translations/SimpleFrpPanel_en.qm
```

---

## 从源码构建

### 1. 获取源码

```powershell
git clone --recursive https://github.com/CCA8798/SimpleFrpPanel.git
cd SimpleFrpPanel
```

如果已经克隆但没有拉取 ElaWidgetTools：

```powershell
git submodule update --init --recursive
```

### 2. 配置 Qt 环境

假设 Qt 安装目录为：

```text
C:\Qt\Qt5.14.2
```

设置 PATH：

```powershell
$env:PATH = "<QT_INSTALL>\Tools\mingw730_64\bin;" +
            "<QT_INSTALL>\5.14.2\mingw73_64\bin;" +
            $env:PATH
```

然后配置 CMake：

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
      -DCMAKE_BUILD_TYPE=Release `
      -DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64
```

### 3. 编译

```powershell
cmake --build build -j
```

构建完成后会自动执行 `windeployqt` 部署 Qt 运行库。

### 4. 运行

```powershell
build/SimpleFrpPanel.exe
```

### Qt 路径

Qt 套件目录：`<QT_INSTALL>\5.14.2\mingw73_64`；MinGW 工具链：`<QT_INSTALL>\Tools\mingw730_64`。

如果使用 Qt 安装器安装，请确保安装了 `MinGW 7.3.0 64-bit` 组件；也可以直接使用开始菜单中的 `Qt 5.14.2 (MinGW 7.3.0 64-bit)` 命令提示符环境（PATH 已配置好）。

---

## CLion

1. 使用 CLion 打开项目根目录
2. `Settings → Build, Execution, Deployment → Toolchains` 新建 MinGW 工具链，编译器指向 `<QT_INSTALL>\Tools\mingw730_64\bin\gcc.exe`
3. CMake Profile 可直接使用默认配置（自动回退本机 Qt），也可显式加 `-DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64`
4. 构建并运行（完成后自动部署 Qt 与 MinGW 运行库）

---

## 数据与配置

程序运行目录下：

```text
frp/
├── frpc.exe          # 内置 frp 客户端（可被自动更新替换）
├── frps.exe          # 内置 frp 服务端
└── frp_version.txt   # 内置 frp 版本号（更新检查用）

data/
├── *.db              # SQLite 数据库（WAL 模式，busy_timeout=5000）
└── *.frps.toml       # 自动生成的 frps 配置

frpc.toml             # 自动生成的 frpc 配置
config.ini            # 程序设置（见下）
```

主要数据表：`users`、`tunnels`、`settings`、`traffic_records`。`traffic_records` 不会因用户或隧道删除而丢失，可保留历史流量。

### config.ini 键值

| 分组 | 键 | 说明 |
| --- | --- | --- |
| `general` | `confirmOnClose` / `closeAction` | 关闭窗口是否询问，及记忆动作（tray/quit） |
| `general` | `language` | 界面语言：`zh_CN` / `en` |
| `general` | `updateProxy` | frp 更新 HTTP 代理（主机:端口，空=系统代理） |
| `client` | `host` / `port` / `username` / `password` | 客户端登录参数（密码 Base64 编码） |
| `server_state` | `dbName` / `userId` | 服务端上次选择的数据库与用户 |
| `tunnel_draft_server` / `tunnel_draft_client` | 各隧道字段 | 新增隧道弹窗填写记忆 |
| `frps/path`、`client/frpcPath` | — | frps / frpc 程序路径（为空时自动指向内置 `frp\`） |

---

## 常见问题

### frps 启动失败（端口被占用）

出现 `bind: Only one usage of each socket address` 说明端口已被占用。检查：`netstat -ano | findstr :<端口>`，结束占用进程，或修改 frps 绑定端口后重试（端口在页面编辑即保存）。

### 流量统计没有数据

确认：frps 正在运行、Web Dashboard 已启用、隧道正在使用且有实际流量；首次采样需等待约 20 秒建立基准。日志出现"流量采样已连接 frps 仪表盘"表示链路正常。

### 提示"未找到内置 frp" / 更新失败

- 首次运行会弹窗询问是否一键下载 frp（需要联网）
- 更新失败常见于网络受限：在 设置 → frp 更新代理 填写 HTTP 代理（如 `127.0.0.1:7897`）后重启重试
- frps/frpc 正在运行时更新会被占用，请先停止再更新

### 客户端无法登录

检查服务器地址、面板服务端口、用户名密码，以及用户是否被禁用/过期。

### 切换页面后客户端掉线

正常切页不会重启面板服务；仅当数据库或端口配置实际变化时才重启相关服务。

---

## 技术说明

### 面板 API

TCP + JSON 行协议。客户端登录后服务端签发随机 Token，后续请求携带；服务端校验 Token 有效性、用户身份、隧道归属与端口配额。

### 端口配额

每个用户可设置远端端口范围、本地端口范围与最大端口数量；客户端只能在允许范围内创建隧道。创建/修改隧道时做两层检查：数据库端口冲突 + 本机实际端口占用。

### 流量统计

定时访问 frps Dashboard API，按 用户 + 隧道 + 日期 累计保存。

---

## 许可证

本项目使用 [MIT License](LICENSE)。版权所有 © 2025 CCA8798。

ElaWidgetTools 使用 MIT License。frp 使用 Apache-2.0 License，由 [fatedier/frp](https://github.com/fatedier/frp) 提供。
