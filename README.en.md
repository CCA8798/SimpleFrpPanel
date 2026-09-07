
<div align="center">

[**English**](README.en.md) | [简体中文](README.md)

# SimpleFrpPanel

![License: MIT](https://img.shields.io/badge/license-MIT-green)

A visual management panel for FRP, built with **Qt 5.14.2 + ElaWidgetTools**.

Manage both `frps` and `frpc` in one window — users, tunnels, port quotas, traffic statistics — with Windows system-tray background running and a bilingual (Chinese / English) interface.

</div>

## Features

### Home · Overview

- Four self-drawn overview cards, refreshed every second:
  - **Running status**: frps / panel service / database / dashboard
  - **Statistics**: user count, tunnel count (with enabled count)
  - **Traffic**: today & cumulative inbound/outbound
  - **Client session**: login state, frpc state, my tunnels
- Server and client pages publish their state into the global info store, and the Home page renders it live

### Server

- User management
  - SQLite database management
  - Add / edit / delete users, enable / disable, expiry date
  - Salted SHA-256 password digests
  - Public IP / port configuration
- Tunnel management
  - TCP / UDP / HTTP / HTTPS, including domain tunnels
  - Per-user port quotas (remote/local ranges + max port count)
  - Database port-conflict detection + local TCP/UDP port-in-use detection
- frps management
  - Uses the bundled `frps.exe` (under `app\frp\`) or a custom path
  - Auto-generates `frps.toml`, one-click start/stop, auto-restart on config change
  - Running state and live logs; bind port / token / dashboard port **saved on edit**
- Panel service
  - TCP + JSON API with login auth and tunnel management endpoints
  - Remembers the last selected database and user, restores on restart

### Client

- Login with server address/port/account/password (credentials **including password are remembered**, prefilled next time)
- Shows the port quota and usage; full tunnel CRUD, enable/disable, live status
- Uses the bundled `frpc.exe` (under `app\frp\`) or a custom path
- Auto-generates `frpc.toml`, one-click start/stop, auto-restart on config change, live logs

### Traffic statistics

Samples traffic through the frps Web API periodically and stores it by **user + tunnel + date**; query by user, tunnel and date range. History survives user/tunnel deletion.

### Settings · Other

- **Settings dialog (gear button in the title bar)**:
  - Ask on window close: run in background vs. exit (choice can be remembered; tray "Quit" always exits)
  - Interface language: 简体中文 / English (takes effect after restart)
  - frp update proxy (HTTP proxy like `127.0.0.1:7897`; leave empty to use the system proxy)
- **Bundled frp with auto-update**: `frpc.exe` / `frps.exe` ship under `app\frp\`; on startup the app checks GitHub for a newer release — when missing or when a newer version exists, you can **download it with one click** (with a progress dialog)
- ElaWidgetTools dark/light theme following; system-tray background running
- The Add-Tunnel dialog **remembers your last input separately for server/client** and prefills it
- Auto refresh on page switch + periodic polling

---

## System requirements

| Component | Requirement |
| --- | --- |
| OS | Windows 7 or later (x64) |
| Qt | 5.14.2 (MinGW 7.3 64-bit, for building) |
| CMake | >= 3.12 (for building) |
| frps / frpc | **Bundled** under `frp\` in the release package (v0.71.0); you may also drop in your own files or fetch from [frp Releases](https://github.com/fatedier/frp/releases) |

> frp ships with the package under `<app dir>\frp\frpc.exe` and `frps.exe`. The app checks for updates on startup (needs network; a proxy can be configured in Settings); if missing, it asks to download frp in one click.

---

## Quick start

### Server

1. Run `SimpleFrpPanel.exe`, open **Server · User Management**: create a database → add a user → set the public IP and port
2. Open **Server · Tunnel Management**: set the port quota → click **Start** (frps path already points to the bundled `frp\frps.exe`) → start the panel service
3. Add a tunnel, e.g. TCP: remote port `15001` → LAN `192.168.1.10:80`
4. Clients reach your LAN service at `PublicIP:15001`

### Client

1. Open **Client · Tunnel Management**: server address/port/account/password are prefilled from the last login
2. Click **Login** (first time requires input); the frpc path already points to the bundled `frp\frpc.exe`
3. Click **Start** to begin forwarding; tunnels can be toggled in the list

### Language & updates

- **Switch language**: gear → Interface language → restart the app
- **frp updates**: checked automatically on startup; fill in an HTTP proxy in Settings to speed up downloads

---

## Internationalization (i18n)

- Source strings use Qt standard `tr()`; the release embeds `SimpleFrpPanel_en.qm` (English), Chinese is the default
- Translation source: `translations/SimpleFrpPanel_en.ts`
- After changing UI text, refresh translations:

```powershell
# From the Qt bin directory
lupdate -recursive src -ts translations/SimpleFrpPanel_en.ts        # re-extract
# Edit translations/SimpleFrpPanel_en.ts (Qt Linguist or manually) to complete English
lrelease translations/SimpleFrpPanel_en.ts -qm translations/SimpleFrpPanel_en.qm
```

---

## Building from source

### 1. Get the source

```powershell
git clone --recursive https://github.com/CCA8798/SimpleFrpPanel.git
cd SimpleFrpPanel
```

If you already cloned without submodules:

```powershell
git submodule update --init --recursive
```

### 2. Set up the Qt environment

Assuming Qt is installed at:

```text
C:\Qt\Qt5.14.2
```

Add to PATH:

```powershell
$env:PATH = "<QT_INSTALL>\Tools\mingw730_64\bin;" +
            "<QT_INSTALL>\5.14.2\mingw73_64\bin;" +
            $env:PATH
```

Configure CMake:

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
      -DCMAKE_BUILD_TYPE=Release `
      -DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64
```

### 3. Build

```powershell
cmake --build build -j
```

`windeployqt` runs automatically after the build to deploy the Qt runtime.

### 4. Run

```powershell
build/SimpleFrpPanel.exe
```

### Qt paths

Qt kit directory: `<QT_INSTALL>\5.14.2\mingw73_64`; MinGW toolchain: `<QT_INSTALL>\Tools\mingw730_64`.

If you use the Qt installer, make sure the `MinGW 7.3.0 64-bit` component is installed. Alternatively open the `Qt 5.14.2 (MinGW 7.3.0 64-bit)` command prompt from the Start menu (PATH already configured).

---

## CLion

1. Open the project root in CLion
2. `Settings → Build, Execution, Deployment → Toolchains`: create a MinGW toolchain pointing to `<QT_INSTALL>\Tools\mingw730_64\bin\gcc.exe`
3. The CMake profile can use the defaults (falls back to the local Qt), or add `-DQT_SDK_DIR=<QT_INSTALL>/5.14.2/mingw73_64`
4. Build and run (Qt and MinGW runtimes are deployed automatically)

---

## Data & configuration

Under the app directory:

```text
frp/
├── frpc.exe          # bundled frp client (can be replaced by auto-update)
├── frps.exe          # bundled frp server
└── frp_version.txt   # bundled frp version (used by the update check)

data/
├── *.db              # SQLite database (WAL mode, busy_timeout=5000)
└── *.frps.toml       # auto-generated frps config

frpc.toml             # auto-generated frpc config
config.ini            # app settings (see below)
```

Main tables: `users`, `tunnels`, `settings`, `traffic_records`. `traffic_records` survives user/tunnel deletion so history is preserved.

### config.ini keys

| Section | Key | Description |
| --- | --- | --- |
| `general` | `confirmOnClose` / `closeAction` | ask when closing the window, and the remembered action (tray/quit) |
| `general` | `language` | UI language: `zh_CN` / `en` |
| `general` | `updateProxy` | HTTP proxy for frp updates (host:port; empty = system proxy) |
| `client` | `host` / `port` / `username` / `password` | client login parameters (password Base64-encoded) |
| `server_state` | `dbName` / `userId` | last selected database and user on the server page |
| `tunnel_draft_server` / `tunnel_draft_client` | tunnel fields | Add-Tunnel dialog input memory |
| `frps/path`, `client/frpcPath` | — | frps / frpc paths (auto-points to the bundled `frp\` when empty) |

---

## FAQ

### frps fails to start (port in use)

`bind: Only one usage of each socket address` means the port is taken. Check with `netstat -ano | findstr :<port>`, end the occupying process, or change the frps bind port (ports are saved on edit).

### Traffic statistics show nothing

Make sure frps is running, the Web Dashboard is enabled, a tunnel is active with real traffic, and wait ~20 s for the baseline sample. Seeing "流量采样已连接 frps 仪表盘" in the log means the link is healthy.

### "Bundled frp not found" / update fails

- The first run asks whether to download frp in one click (requires network)
- Update failures are usually network-related: fill an HTTP proxy (e.g. `127.0.0.1:7897`) in Settings → frp update proxy, restart and retry
- Updating while frps/frpc are running fails because the files are in use — stop them first

### Client cannot log in

Check the server address, panel-service port, username/password, and whether the user is disabled or expired.

### Client drops after switching pages

Normal page switches never restart the panel service; it restarts only when the database or port config actually changes.

---

## Technical notes

### Panel API

TCP + JSON line protocol. After login the server issues a random token carried by later requests; the server validates the token, user identity, tunnel ownership and port quota.

### Port quotas

Each user has a remote port range, a local port range and a max port count; clients may only create tunnels within their allowed ranges. Two checks run on tunnel create/edit: database port-conflict + local port-in-use.

### Traffic statistics

Periodically samples the frps Dashboard API and accumulates by user + tunnel + date.

---

## License

MIT License. Copyright © 2025 CCA8798.

ElaWidgetTools is MIT licensed. frp is Apache-2.0 licensed by [fatedier/frp](https://github.com/fatedier/frp).
