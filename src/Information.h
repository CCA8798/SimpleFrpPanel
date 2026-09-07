//
// Created by Administrator on 2026/9/7.
//

#ifndef INFORMATION_H
#define INFORMATION_H
#include <QtGlobal>
#include <QString>

// 进程级全局信息：跨页面共享状态统一从这里读写。
// - 服务端总览字段由「服务端 · 隧道管理页」(ServerTunnelPage) 在其轮询中写入；
// - 客户端会话字段由「客户端 · 隧道管理页」(ClientTunnelPage) 在其状态变化/轮询时写入；
// - 「首页」(HomePage) 每秒读取并刷新总览卡片；
// - 应用设置字段持久化在 <程序目录>/config.ini（loadSettings/saveSettings）。
class GlobalInformation{
public:
	const QString appVersion = "1.1.0";

	// ============ 服务端总览（服务端·隧道管理页写入） ============
	bool serverDbOpen = false;            // 是否已打开账号数据库
	QString serverDbName;                 // 当前数据库文件名（未打开时为空）
	bool frpsRunning = false;             // frps 进程是否运行
	int frpsBindPort = 0;                 // frps 绑定端口（0 = 未设置）
	int frpsWebPort = 0;                  // frps 仪表盘端口（0 = 未启用）
	bool panelRunning = false;            // 面板 API 服务是否运行
	int panelPort = 0;                    // 面板服务监听端口（客户端登录端口）
	int usersCount = 0;                   // 用户总数
	int tunnelsCount = 0;                 // 隧道总数
	int tunnelsEnabledCount = 0;          // 已启用隧道数
	qint64 todayBytesIn = 0;              // 今日接收流量
	qint64 todayBytesOut = 0;             // 今日发送流量
	qint64 totalBytesIn = 0;              // 历史累计接收
	qint64 totalBytesOut = 0;             // 历史累计发送

	// ============ 客户端会话（客户端·隧道管理页写入） ============
	bool clientConnected = false;         // 与面板服务器 TCP 连接
	bool clientLoggedIn = false;          // 是否已登录
	QString clientUserName;               // 登录用户名
	QString clientServerHost;             // 面板服务器地址
	int clientServerPort = 0;             // 面板服务器端口
	bool frpcRunning = false;             // frpc 进程是否运行
	QString frpcServerHost;               // frpc 连接的 frps 地址
	int frpcServerPort = 0;               // frpc 连接的 frps 端口
	int clientTunnelsCount = 0;           // 我的隧道总数
	int clientTunnelsEnabledCount = 0;    // 我的已启用隧道数

	// ============ 应用设置（config.ini 持久化） ============
	bool confirmOnClose = true;           // 点关闭按钮时是否弹出"退出 / 后台运行"询问
	QString closeAction = "tray";         // 不询问时的关闭动作："tray" = 后台运行；"quit" = 直接退出
	QString language = "zh_CN";           // 界面语言："zh_CN" / "en"（重启后生效）
	QString updateProxy;                  // frp 更新所用的 HTTP 代理 "主机:端口"，空 = 系统代理

	// 从 <程序目录>/config.ini 读取设置；保存设置到 config.ini
	void loadSettings();
	void saveSettings() const;
};

extern  GlobalInformation g_GlobalInformation;

#endif //INFORMATION_H
