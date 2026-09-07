

# SimpleFrpPanel

![License: MIT](https://img.shields.io/badge/license-MIT-green)

基于 **Qt 5.14.2 + ElaWidgetTools** 的 FRP 可视化管理面板。

支持在一个界面中管理 `frps` 和 `frpc`，包括用户、隧道、端口配额、流量统计等功能，并支持 Windows 系统托盘运行。


## 功能

### 服务端

- 用户管理
  - SQLite 数据库管理
  - 用户增删改查
  - 盐值 + SHA-256 密码摘要
  - 用户启用/禁用
  - 到期时间
  - 公网 IP 和端口配置
- 隧道管理
  - TCP / UDP / HTTP / HTTPS
  - 域名隧道
  - 按用户设置端口配额
  - 远端端口范围和本地端口范围限制
  - 数据库端口冲突检测
  - 本机 TCP / UDP 端口占用检测
- frps 管理
  - 选择 `frps.exe`
  - 自动生成 `frps.toml`
  - 一键启动/停止
  - 配置修改后自动重启
  - 运行状态和实时日志
- 面板服务
  - TCP + JSON API
  - 用户登录认证
  - 隧道管理接口

### 客户端

- 服务器地址、端口、账号密码登录
- 自动拒绝禁用或过期账号
- 显示用户端口配额
- 隧道增删改查
- 隧道启停
- 运行状态显示
- 配额使用情况
- 选择 `frpc.exe`
- 自动生成 `frpc.toml`
- frpc 一键启动/停止
- 配置修改后自动重启
- 实时日志

### 流量统计

通过 frps Web API 定时采样流量，并按以下维度保存：

- 用户
- 隧道
- 日期

支持按用户、隧道和日期范围查询接收/发送流量。

已删除的用户和隧道，其历史流量记录仍会保留。

### 其他

- ElaWidgetTools 深色/浅色主题
- 系统托盘后台运行
- 关闭窗口后隐藏到托盘
- 通过托盘菜单退出程序
- 页面切换自动刷新
- 定时轮询同步数据
- 隧道编辑支持填写快照保存/还原

---

## 系统要求

| 组件 | 要求 |
| --- | --- |
| 操作系统 | Windows 7 及以上（x64） |
| Qt | 5.14.2（MinGW 7.3 64-bit） |
| CMake | >= 3.12 |
| frps | 服务端需要 |
| frpc | 客户端需要 |

`frps.exe` 和 `frpc.exe` 随面板提供，亦可从 [frp Releases](https://github.com/fatedier/frp/releases) 下载。

---

## 使用

### 服务端

1. 运行 `SimpleFrpPanel.exe`
2. 进入 **服务端 · 用户管理**
3. 新建数据库
4. 创建用户
5. 设置公网 IP 和端口
6. 进入 **服务端 · 隧道管理**
7. 设置用户端口配额
8. 选择 `frps.exe` 并启动
9. 启动面板服务
10. 创建隧道

例如：

```text
远端端口：15001
内网地址：192.168.1.10
内网端口：80
协议：TCP
```

客户端连接后访问：

```text
公网IP:15001
```

即可访问内网服务。

### 客户端

1. 进入 **客户端 · 隧道管理**
2. 填写服务器地址和端口
3. 输入账号密码并登录
4. 选择 `frpc.exe`
5. 启动 frpc

登录成功后可以查看自己的端口配额并管理隧道。

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

Qt 套件目录：

```text
<QT_INSTALL>\5.14.2\mingw73_64
```

MinGW 工具链：

```text
<QT_INSTALL>\Tools\mingw730_64
```

如果使用 Qt 安装器安装，确保安装了：

```text
MinGW 7.3.0 64-bit
```

也可以直接使用 Qt 提供的：

```text
Qt 5.14.2 (MinGW 7.3.0 64-bit)
```

命令行环境，此时不需要手动配置 PATH。

---

## CLion

1. 使用 CLion 打开项目根目录
2. 配置 MinGW Toolchain
3. 编译器选择：

```text
<QT_INSTALL>\Tools\mingw730_64\bin\gcc.exe
```

4. CMake Profile 可以直接使用默认配置，也可以添加：

```text
-DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64
```

5. 构建并运行

构建完成后会自动部署 Qt 和 MinGW 运行库。

---

## 项目结构

```text
SimpleFrpPanel/
├── CMakeLists.txt
├── include/
│   └── ElaWidgetTools/
└── src/
    ├── main.cpp
    ├── MainWindow.h/.cpp
    ├── DatabaseManager.h/.cpp
    ├── UserEditDialog.h/.cpp
    ├── TunnelEditDialog.h/.cpp
    ├── PortChecker.h/.cpp
    ├── FrpsManager.h/.cpp
    ├── PanelApiServer.h/.cpp
    ├── PanelClient.h/.cpp
    ├── TrafficMonitor.h/.cpp
    ├── StatusLight.h/.cpp
    ├── StatusDotDelegate.h/.cpp
    └── *Page.h/.cpp/.ui
```

### 主要模块

| 文件                  | 功能                  |
| ------------------- | ------------------- |
| `MainWindow`        | 主窗口、导航、系统托盘         |
| `DatabaseManager`   | SQLite 数据库          |
| `FrpsManager`       | frps/frpc 进程管理和配置生成 |
| `PanelApiServer`    | 面板 API 服务           |
| `PanelClient`       | 客户端 API             |
| `TrafficMonitor`    | 流量采样和统计             |
| `PortChecker`       | TCP/UDP 端口检测        |
| `StatusLight`       | 状态灯                 |
| `StatusDotDelegate` | 表格状态灯               |

页面使用独立 `.ui` 文件，可以直接通过 Qt Designer 编辑。

ElaWidgetTools 控件通过 Qt Designer 的提升功能使用，例如：

```text
QPushButton → ElaPushButton
```

---

## 数据和配置

程序运行目录下：

```text
data/
├── *.db
└── *.frps.toml

frpc.toml
config.ini
```

### 数据库

SQLite 数据库位于：

```text
data/*.db
```

数据库使用 WAL 模式，并设置：

```text
busy_timeout = 5000
```

主要数据表：

```text
users
tunnels
settings
traffic_records
```

其中 `traffic_records` 不会因为用户或隧道删除而删除，因此可以保留历史流量。

### 配置文件

服务端：

```text
data/<数据库名>.frps.toml
```

客户端：

```text
frpc.toml
```

程序设置：

```text
config.ini
```

---

## 常见问题

### frps 启动失败

如果出现：

```text
bind: Only one usage of each socket address
```

说明端口已经被其他程序占用。

可以检查端口占用情况：

```powershell
netstat -ano | findstr :<端口>
```

结束占用进程，或者修改 frps 的绑定端口。

### 流量统计没有数据

确认：

1. frps 正在运行
2. Web Dashboard 已启用
3. 隧道正在使用
4. 已经有实际流量经过隧道

首次采样需要等待一段时间，之后按照采样结果累计记录。

如果日志显示：

```text
流量采样已连接 frps 仪表盘
```

说明连接正常。

### 客户端无法登录

检查：

* 服务器地址
* 面板服务端口
* 用户名
* 密码
* 用户是否被禁用
* 用户是否已经过期

### 切换页面后客户端掉线

正常情况下切换页面不会重启面板服务。

只有数据库或端口配置实际发生变化时，相关服务才会重新启动。

---

## 技术说明

### 面板 API

使用：

```text
TCP + JSON Line
```

客户端登录后由服务端签发随机 Token，后续请求携带 Token。

服务端会检查：

* Token 是否有效
* 用户身份
* 隧道归属
* 端口配额

### 端口配额

每个用户可以设置：

```text
远端端口范围
本地端口范围
最大端口数量
```

客户端只能在允许范围内创建隧道。

创建或修改隧道时会进行两层检查：

```text
数据库端口冲突
        +
本机实际端口占用
```

### 流量统计

定时访问 frps Dashboard API 获取流量数据，并按照：

```text
用户 + 隧道 + 日期
```

累计保存。

---

## 许可证

本项目使用 [MIT License](LICENSE)。

版权所有 © 2025 CCA8798

ElaWidgetTools 使用 MIT License。

frp 使用 Apache-2.0 License，由 [fatedier/frp](https://github.com/fatedier/frp) 提供。

```

我这里主要做了三件事：**删掉重复说明、把过于“解释性”的句子改成项目文档口吻、把功能和构建流程重新分层**。这样放 GitHub 上会更像一个正常的开源项目 README，而不是产品需求文档。
```
