#include "AppConfig.h"

AppConfig& AppConfig::instance()
{
	/*
		局部静态对象，程序启动时首次调用才创建
		无需 new，对象在首次调用时自动创建，在程序结束时自动销毁。
		所有通过 AppConfig::instance() 访问的都是同一个对象，因此配置是全局共享的。
	*/
    static AppConfig config;
    return config;
}

AppConfig::AppConfig()
    : m_mode(Local),
      m_serverHost("127.0.0.1"),
      m_serverPort(8080),
      m_localRoot("D:/Z_project/")
{
}

// 支持ipv4和ipv6，如果是ipv6，url需要构造成[ip]:port格式
void AppConfig::setServerHost(const QString& host) {
    QString clean = host.trimmed();
    // 如果是 IPv6 地址（包含 ':'）且没有被方括号包围，则添加上括号
	// ipv4则不用管，这里直接构造出host = [ipv6] 能够直接用于url拼接
    if (clean.contains(':') && !clean.startsWith('[')) {
        clean = QString("[%1]").arg(clean);
    }
    m_serverHost = clean;
}