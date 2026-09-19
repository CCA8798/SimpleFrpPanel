# AGENTS.md

> 面向在本仓库工作的 AI 代理（以及新加入的开发者）的速查与约束说明。
> 人类用户文档见 [`README.md`](README.md) / [`README.en.md`](README.en.md)；本文件聚焦"怎么改、别踩什么坑"。

---

## 1. 项目一句话

Windows 桌面端 **FRP 可视化管理面板**：一个进程同时充当 **服务端**（管理 frps、用户、隧道、配额、流量）与 **客户端**（登录面板、管理自己的隧道、运行 frpc），
基于 **Qt 5.14.2 (MinGW 7.3 64-bit) + [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools)（FluentUI 风格组件库，git submodule）**。

- 当前版本：**v1.1.0**（`src/Information.h` 的 `appVersion`、`CMakeLists.txt` 的 `project(... VERSION 1.1.0)`、git tag 三处保持一致）
- 仓库：`https://github.com/CCA8798/SimpleFrpPanel`（MIT）
- 语言：界面简体中文 / English（Qt i18n，重启生效）

---

## 2. 目录与文件职责

```
CMakeLists.txt                 顶层构建脚本（AUTOMOC/AUTOUIC/AUTORCC + windeployqt 自动部署）
.gitmodules                    include/ElaWidgetTools 子模块（勿手改其内容；需要升级用 submodule 命令）
translations/
  SimpleFrpPanel_en.ts         英文翻译源（lupdate 产物 + 人工/工具补全）
  SimpleFrpPanel_en.qm         编译后的翻译（被 src/resources.qrc 内嵌进 exe）
src/
  main.cpp                     入口：QApplication + ElaApplication 初始化；按 config.ini 语言加载 QTranslator
  MainWindow.h/.cpp            ElaWindow 主窗口：导航树、标题栏设置按钮、托盘、关闭询问、frp 更新检查与下载进度
  HomePage.h/.cpp/.ui          首页：标题 + LCD 时钟 + 运行总览 4 卡（每秒读 g_GlobalInformation 刷新）
  OverviewCard.h/.cpp          自绘总览卡片（QPainter：图标+标题+分隔线+键值行+状态圆点），随主题换色
  SettingsDialog.h/.cpp        设置对话框（自由函数 showSettingsDialog）：关闭询问开关 / 界面语言 / frp 更新代理
  ServerUserPage.h/.cpp/.ui    服务端·用户管理：数据库文件增删选、用户 CRUD、公网 IP/端口
  ServerTunnelPage.h/.cpp/.ui  服务端·隧道管理：frps 启停与参数、面板服务、按用户隧道表、端口配额、运行日志、流量采样入口
  ClientTunnelPage.h/.cpp/.ui  客户端·隧道管理：登录、配额、我的隧道、frpc 启停、日志
  TrafficPage.h/.cpp/.ui       服务端·流量统计：按用户/隧道/日期区间查询
  DatabaseManager.h/.cpp       SQLite 数据层（users/tunnels/settings/traffic_records）+ 密码摘要 + 登录校验 + 聚合统计
  FrpsManager.h/.cpp           frp 进程管理（frps/frpc 共用）：生成 toml、启停、日志转发；程序路径存 config.ini
  FrpUpdater.h/.cpp            内置 frp 的目录约定 + GitHub 更新检查 + 下载/解压/替换（含代理）
  PanelApiServer.h/.cpp        面板 API 服务端（TCP + JSON 行协议，token 认证，配额与归属校验）
  PanelClient.h/.cpp           面板 API 客户端
  TrafficMonitor.h/.cpp        frps 仪表盘采样（10s）→ traffic_records
  TunnelEditDialog.h/.cpp      新增/修改隧道对话框（新增模式带分组"填写记忆"）
  UserEditDialog.h/.cpp        用户编辑对话框
  PortChecker.h/.cpp           TCP/UDP 端口占用探测
  StatusLight.h/.cpp           状态灯控件
  StatusDotDelegate.h/.cpp     表格状态灯委托（按文本关键字上色，注意与 tr 的一致性）
  Information.h/.cpp           全局信息总线 g_GlobalInformation：跨页状态 + 应用设置持久化
  resources.qrc                内嵌 translations/SimpleFrpPanel_en.qm
  *.ui                         Qt Designer 文件；Ela 控件通过"提升法"使用（无 Designer 插件）
```

---

## 3. 架构与数据流（改代码前必读）

### 3.1 页面与"每页自己的管理器"

`MainWindow` 只创建页面对象；**每个页面自己 new 所需管理器**（父子关系挂在页面下）：

- `ServerTunnelPage`：`DatabaseManager`、`FrpsManager("frps/path")`、`PanelApiServer`、`TrafficMonitor`、隧道表 model
- `ClientTunnelPage`：`PanelClient`、`FrpsManager("client/frpcPath")`、隧道表 model
- `ServerUserPage` / `TrafficPage`：各自持有 `DatabaseManager`（同一个 SQLite 连接名，见 `DatabaseManager.cpp` 的 `kConnectionName`）

> ⚠️ 由于多个页面各持 `DatabaseManager`，**数据库只允许单一"当前库"**；切换数据库是全局影响操作。

### 3.2 全局信息总线 `g_GlobalInformation`（`Information.h/.cpp`）

跨页面共享状态**必须**经过它，不要跨页直接访问其它页面的成员：

- 服务端侧（`ServerTunnelPage::updateGlobalOverview()` 每 5s + 启停事件写入）：`serverDbOpen/serverDbName/frpsRunning/frpsBindPort/frpsWebPort/panelRunning/panelPort/usersCount/tunnelsCount/tunnelsEnabledCount/todayBytesIn/Out/totalBytesIn/Out`
- 客户端侧（`ClientTunnelPage::updateGlobalSession()` 在登录/登出/隧道同步/frpc 启停/3s 轮询写入）：`clientConnected/clientLoggedIn/clientUserName/clientServerHost/clientServerPort/frpcRunning/frpcServerHost/frpcServerPort/clientTunnelsCount/clientTunnelsEnabledCount`
- 应用设置（持久化到 `config.ini [general]`）：`confirmOnClose`、`closeAction`、`language`、`updateProxy`
- `HomePage` 每秒读取并渲染

**新增跨页状态时**：在 `GlobalInformation` 加字段 → 明确"谁写、何时写" → 首页/使用方只读。

### 3.3 面板 API（服务端 ⇄ 客户端）

`PanelApiServer`（TCP + JSON 行协议）：命令 `login / logout / list / add_tunnel / update_tunnel / delete_tunnel / set_tunnel_enabled`。
登录成功签发随机 token 并下发 `serverInfo{publicIp, publicPort, frpsBindPort, frpsToken}`；服务端校验 token、用户身份、隧道归属与端口配额。

**协议字符串是"数据"，不参与翻译**（见 §5.4）。客户端 `PanelClient` 用请求序号关联响应，5s 连接超时。

### 3.4 frp 进程与配置

`FrpsManager` 生成两种 TOML（UTF-8 + 纯 ASCII 注释，避免 GBK/解析问题）：

- `data/<库名>.frps.toml`：`bindPort`、`auth.token`、`allowPorts`（各用户远端端口范围白名单）、可选 `webServer.*`（仪表盘 = 流量数据源）
- `frpc.toml`：`serverAddr`（= 客户端填写/连接的服务器地址）、`serverPort`（= 服务端下发的 `frpsBindPort`）、`auth.token`、`[[proxies]]`（仅 enabled 的隧道；proxy name = 隧道名）

### 3.5 流量统计

`TrafficMonitor` 每 **10s** 轮询 `http://127.0.0.1:<frps_web_port>/api/proxy/<protocol>/<隧道名>`（Basic 认证，**显式 NoProxy** 直连本机），
用 `todayTrafficIn/Out` 差值累加进 `traffic_records`（按 用户+隧道+日期 UPSERT；负增量钳制 0）。
配置项来自 `settings` 表：`frps_web_port / frps_web_user / frps_web_password`。

### 3.6 内置 frp 与自动更新

- 约定目录：`<程序目录>/frp/`，含 `frpc.exe`、`frps.exe`、`frp_version.txt`
- `ServerTunnelPage`/`ClientTunnelPage` 在"用户未自定义路径"时自动把路径指向内置 exe 并写入 `config.ini`
- `FrpUpdater`：启动后 1.5s 检查 GitHub `fatedier/frp` 最新 release（`windows_amd64.zip`）
  - 有新版 → 弹"发现新版本"确认 → 下载（带进度对话框）→ `tar -xf` 解压 → 覆盖 exe → 写版本号
  - frp 缺失 → 弹"是否下载最新版"（拿不到地址时点按钮会重新联网获取）
  - 已最新 / 网络失败 → 静默
  - 代理：`config.ini [general] updateProxy`（`host:port`，空 = 系统代理）

---

## 4. 构建、运行、调试

### 4.1 环境

| 项 | 值 |
| --- | --- |
| Qt | 5.14.2，套件 `mingw73_64`（构建脚本会校验 5.12 ~ 6.7.0 及 MinGW/MSVC ABI 匹配） |
| 编译器 | MinGW GCC 7.3 x64 |
| CMake | >= 3.12（AUTOMOC/AUTOUIC/AUTORCC 开启） |
| 子模块 | `include/ElaWidgetTools`（`git submodule update --init --recursive`） |

### 4.2 常用命令（Windows PowerShell）

```powershell
# 配置（QT_SDK_DIR 指向 Qt 套件根目录；未传则回退本机默认路径）
cmake -S . -B build-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
      -DQT_SDK_DIR=F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64

# 增量构建（构建后自动复制 ElaWidgetTools.dll 并运行 windeployqt）
cmake --build build-release -j 8

# 冒烟运行（GUI 程序：启动数秒后检查进程是否存活，然后杀掉）
$p = Start-Process -FilePath "build-release\SimpleFrpPanel.exe" -PassThru
Start-Sleep -Seconds 5
if ($p.HasExited) { "EXITED $($p.ExitCode)" } else { "RUNNING OK"; Stop-Process -Id $p.Id -Force }
```

> 本项目**没有单元测试**；验证手段 = 编译通过 + GUI 冒烟 + 关键场景手工验证（见 §8）。

### 4.3 运行期文件（都在 exe 同目录，便于"绿色"分发）

```
frp/                     内置 frp 程序与版本文件
data/*.db                账号数据库（SQLite，WAL，busy_timeout=5000）
data/<库名>.frps.toml    自动生成的 frps 配置
frpc.toml                自动生成的 frpc 配置
config.ini               应用设置（general / client / server_state / frp / tunnel_draft_*）
```

### 4.4 快速切换语言（免点 UI，便于验证 i18n）

`config.ini`：

```ini
[general]
language=en      ; 或 zh_CN
```

写入后重启进程即生效（`main.cpp` 在创建窗口前安装 `QTranslator`）。

---

## 5. 关键约定与"别踩的坑"

### 5.1 国际化：lupdate 只看**直接字面量调用**

- 可被提取：`tr("中文")`、`QCoreApplication::translate("SimpleFrpPanel", "中文")`
- **不可提取**：宏包装（曾经的 `SETTINGS_TR(...)`）、自定义 helper 转发（曾经的 `overviewTr("运行状态")`）
  → 结果就是"界面切英文了但这里还是中文"。
  **规则：任何要翻译的字符串，必须直接写在 `tr(...)` / `translate(...)` 里。**
- 非 QObject 的自由函数用 `QCoreApplication::translate("SimpleFrpPanel", "...")`；类内成员用 `tr(...)`（context = 类名）
- `.ui` 中的文本由 uic 生成 `retranslateUi`，`lupdate` 会自动纳入（context = ui 类名）
- 翻译刷新完整流程（见 §7）

### 5.2 UI 文案 vs 数据/协议文案（**改动前务必区分**）

| 类别 | 处理方式 | 例子 |
| --- | --- | --- |
| 界面文案 | 必须 `tr()` | 按钮、表头、提示条、对话框、导航标题、总览卡文本 |
| 面板 API JSON 载荷 | **固定中文，不翻译** | `PanelApiServer` 的 `message`、`tunnelToJson` 的 `status`（"运行中/未运行/已禁用"）——跨语言客户端契约 |
| 写入数据库的快照名 | **固定中文，不翻译** | `DatabaseManager` 的 `(已删除用户)/(已删除隧道)` |
| 协议/配置键、单位、时间格式、TOML/JSON/SQL 片段 | 不翻译 | `tcp/http`、`frps_bind_port`、`HH:mm:ss`、`yyyy-MM-dd` |

### 5.3 代码中**用字符串比较**做逻辑的地方（翻译必须保持一致）

- `StatusDotDelegate` 用 `text == tr("运行中") / tr("已禁用")` 决定状态灯颜色
- `ServerTunnelPage::updateTunnelStatusColumn()` 用 `text == tr("已禁用")` 跳过更新
- `TrafficPage` 对 `（已删除）` 做 `endsWith/chop`（该字符串**保持不译**，避免长度变化破坏逻辑）

→ 英文翻译里这些词必须**全局一致**：`运行中=Running`、`未运行=Stopped`、`已禁用=Disabled`。

### 5.4 ElaWidgetTools 实用陷阱（本项目已踩过）

- **`ElaToggleSwitch`**：构造即 `setFixedSize(44,22)`，旋钮端点按该宽度算；**永远不要再改它的宽度**（`setMinimumWidth` 等会让旋钮停在"中间"）。表格内创建开关需要正确初始化位置（参考 `refreshTunnelTable` 内的写法）。
- **`ElaText`**：`setTextStyle(ElaTextType::Title)` 会把字号**强制设成 28px**（覆盖你先前设的 pixelSize）——需要精确字号时用 `setTextPixelSize()` + 手动 `QFont::DemiBold`。
- **`ElaText` 的 `setIsWrapAnywhere(true)`** 会走库内自绘换行分支，多行时**底部文字可能被裁剪**。对话框里的说明文字推荐：拆成**单行 `ElaText`** + `setWordWrap(false)` + 垂直 `QSizePolicy::Fixed`（必要时 `setFixedHeight`）。
- **`ElaContentDialog`**：默认宽 400，长文本会挤；显示位置由库内蒙层几何计算，个别情况偏移 → `SettingsDialog` 里用 `centerOnParent()` 显式居中（可复用该模式）。
- **`ElaDialog`/`ElaContentDialog` 的按钮**：文本为空时应隐藏（项目里已有统一写法）。
- Ela 主窗口（`ElaWindow`）标题栏自带 日/夜主题按钮；要加自定义按钮用 `setCustomWidget(ElaAppBarType::RightArea, w)`，且**必须用"垂直布局(控件+stretch)"包一层**，否则会被拉伸、与内置按钮不齐。
- 组件库无 `ElaCard` 类；卡片族是 `ElaReminderCard` / `ElaPopularCard` / `ElaPromotionCard` 等（本项目总览卡是自绘 `OverviewCard`）。
- 子模块 `include/ElaWidgetTools` 请勿随手改动（保持与上游一致）。

### 5.5 数据层注意点

- 表：`users` / `tunnels` / `settings`(KV) / `traffic_records`(UNIQUE(user_id, tunnel_id, record_date)，名称以快照列冗余保存)
- `settings` 是**每库独立**的，应用级设置放 `config.ini`；不要混用
- 密码：`hashPassword()` 生成 `盐:摘要`（盐 16B + SHA-256），登录用 `verifyUserLogin()`（含禁用/过期判断）
- 端口配额校验在 `DatabaseManager` 内（范围 + 数量上限 + 库内唯一 + 本机占用由 `PortChecker` 辅助）

### 5.6 Windows / PowerShell 5.1 环境注意（本项目实际踩坑）

- **`Compress-Archive -Path "folder\*"` 会漏掉子目录**（打包出的 zip 缺 `platforms/` 等）→ 打包请用：
  - `[System.IO.Compression.ZipFile]::CreateFromDirectory($dir,$zip,'Optimal',$false)`，或
  - 手写条目遍历并 `entryName.Replace('\','/')`（zip 规范要求正斜杠；反斜杠条目在部分工具解压成扁平文件名 → Qt 报 "no Qt platform plugin"）
- **文件编码**：PowerShell 5.1 的 `Get-Content/Set-Content` 默认 ANSI，会破坏 UTF-8 源码/`.ui`/`.ts`
  → 一律用 `[System.IO.File]::ReadAllText/WriteAllText($path, $s, [System.Text.UTF8Encoding]::new($false))`
- 不要用 `GetRelativePath`（.NET Framework 4.x 无此 API）
- `git push` 需要 token URL：`git push "https://x-access-token:$(gh auth token)@github.com/CCA8798/SimpleFrpPanel.git" <ref>`；
  推送前若远端有他人提交（如 CI/网页提交）会 `non-fast-forward` → 先 `git pull --rebase origin main`
- 外网受限时可用本机代理（如 `http://127.0.0.1:7897`）：`curl.exe -x <proxy>` / `$env:HTTPS_PROXY` / `gh` 同环境变量
- Windows 上删除仍被进程占用的 exe/dll 会失败（更新 frp、拷贝发布目录前先停相关进程）

---

## 6. 修改代码时的检查清单

- [ ] 新增/修改**界面文案** → 用 `tr()`/`translate()` 直接字面量；改完执行 §7 的翻译刷新（否则英文界面漏翻）
- [ ] 新增**设置项** → `GlobalInformation` 加字段 + `loadSettings/saveSettings` 读写 + 设置对话框加控件 + README 键值表更新
- [ ] 新增**跨页状态** → 只走 `g_GlobalInformation`，并明确写入时机（事件 or 轮询）
- [ ] 改动**协议载荷/数据库快照** → 不要 `tr()`（见 §5.2）
- [ ] 改动**参与字符串比较**的状态词 → 保持与 `StatusDotDelegate` / `TrafficPage` 一致
- [ ] 新增**源文件** → 加入 `CMakeLists.txt` 的 `add_executable(...)` 列表（本项目不使用 GLOB）
- [ ] 新增**对话框/控件** → 套用 §5.4 的 Ela 用法约定，避免布局/换行/对齐返工
- [ ] 构建（`cmake --build`）通过 + GUI 冒烟通过

---

## 7. 翻译刷新流程（改文案后必做）

```powershell
$qt = "F:/Software/Qt5/Qt5.14.2/5.14.2/mingw73_64/bin"

# 1) 重新提取（-no-obsolete 清理废弃条目）
& "$qt/lupdate.exe" -recursive -no-obsolete -source-language zh_CN -target-language en src -ts translations/SimpleFrpPanel_en.ts

# 2) 补全/修正 ts 中 <translation type="unfinished"> 的条目（手工或脚本；完成后不应再有 unfinished）
#    注意：ts 是 XML，多行文本用真实换行；改完可用 [xml] 解析做一次结构校验

# 3) 编译 qm（内嵌资源依赖它，先产出再构建）
& "$qt/lrelease.exe" translations/SimpleFrpPanel_en.ts -qm translations/SimpleFrpPanel_en.qm

# 4) 重新构建（qrc 会重新打包 qm）
cmake --build build-release -j 8
```

校验 unfinished 数量（应为 0）：

```powershell
[xml]$doc = Get-Content translations\SimpleFrpPanel_en.ts -Raw -Encoding UTF8
($doc.TS.context.message | Where-Object { $_.translation.type -eq 'unfinished' }).Count
```

---

## 8. 关键场景的手工验证清单（无自动化测试）

1. **语言**：`config.ini` 写 `language=en` → 重启 → 导航/页面/设置/关闭询问/首页总览/托盘菜单应为英文；改回 `zh_CN` 恢复中文
2. **内置 frp**：删掉/改名 `frp` 目录 → 启动 → 应弹"尝试下载"对话框（有网时点按钮可下载并出现在 `frp/`）
3. **版本检查**：把 `frp/frp_version.txt` 改成旧版本号 → 启动 → 应弹"发现新版本"
4. **关闭行为**：点右上角 X → 弹「后台运行 / 直接退出」+ 勾选框；取消勾选后再关一次 → 不再弹窗，直接按上次动作；设置页可恢复
5. **参数记忆**：客户端登录一次（含密码）→ 重启 → 登录框应已填充；服务端选库/用户后重启 → 应回到该库/用户；新增隧道填一次 → 再开弹窗应预填（服务端/客户端各自独立）
6. **流量统计**：frps 运行 + 仪表盘启用 + 隧道启用 → 等 >20s（首轮为基准）→ 流量页与首页应出现数据；`0.0.0.0` 端口等异常时看服务端日志
7. **配置编辑即存**：改 frps 绑定端口/Token/仪表盘端口/面板端口 → 失焦 → 重启 → 数值应保留

---

## 9. 发布流程（当前人工执行）

1. 同步版本号三处：`src/Information.h::appVersion`、`CMakeLists.txt::project VERSION`、git tag
2. 构建 Release：`cmake --build build-release -j 8`
3. 组装发布目录 `release/SimpleFrpPanel-v<版本>-win64/`：
   - 从 `build-release` 复制 `SimpleFrpPanel.exe`、`*.dll`、插件目录（`platforms/ iconengines/ imageformats/ sqldrivers/ styles/ bearer/`）
   - 复制 `LICENSE`、`README.md`、`README.en.md`
   - **内置 frp**：`frp/frpc.exe`、`frp/frps.exe`、`frp/frp_version.txt`（当前 v0.71.0，来自 fatedier/frp 的 `windows_amd64` 包）
4. 打包 zip：**必须用正斜杠条目**（见 §5.6），打包后校验 `platforms/qwindows.dll`、`frp/frps.exe` 是否在条目里
5. 上传：`gh release create/upload`（网络受限时先设 `$env:HTTPS_PROXY`），上传后下载回来比对 SHA-256
6. 提交：`git add` 源码/翻译/文档（`.idea/` 属 IDE 状态文件，不提交）

> 注意：`release/`、`build*/` 已被 `.gitignore` 忽略，**不要**把二进制塞进 git 历史。

---

## 10. 已知问题 / 待改进

- `.github/workflows/build-windows.yml`（用户侧 CI）：
  - 打包步骤使用 `Compress-Archive -Path "package/*"` → **会漏子目录**（与 §5.6 同坑），产物可能缺 Qt 插件
  - **不包含 frp**（无下载/内置步骤），与正式发布包不一致
  - workflow 注释存在乱码（编码问题），可顺手修
- 无自动化测试；无 CI 上的翻译/打包校验
- `release/SimpleFrpPanel-v1.0.0-win64/` 为历史遗留目录名，v1.1.0 使用新命名目录
- `install-qt-action` 固定 Qt 5.14.2 / win64_mingw73，与本地开发环境一致，升级需同步 CMake 校验范围

---

## 11. 与用户协作的偏好（本项目实际约定）

- 交流语言：**中文**；提交信息也用中文（简洁分节描述改动）
- 改动后通常期望：**构建通过 + 冒烟 + 相关文案翻译补齐 + 提交推送**（必要时重新打包发布）
- 版本发布点：用户明确说"发布 release"时才打 tag/上传；`gh` 与 `git` 需网络（本机可用代理 `127.0.0.1:7897`）
