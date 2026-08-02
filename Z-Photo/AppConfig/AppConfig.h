#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

/*
	判断模式模式
	AppConfig::instance().mode() == AppConfig::Network
	
	获得ip
	QString AppConfig::instance().serverHost()
	
	获得port
	int AppConfig::instance().serverPort()
	
	只要包含 #include "AppConfig.h"，就可以在任何地方使用 
	AppConfig::instance().serverHost() 等方式访问配置。
*/

class AppConfig
{
public:
    enum Mode { Local, Network };

    static AppConfig& instance();

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode = mode; }

    QString serverHost() const { return m_serverHost; }
    void setServerHost(const QString& host);

    int serverPort() const { return m_serverPort; }
    void setServerPort(int port) { m_serverPort = port; }

    QString localRoot() const { return m_localRoot; }
    void setLocalRoot(const QString& root) { m_localRoot = root; }

private:
    AppConfig();
    Mode m_mode;
    QString m_serverHost;
    int m_serverPort;
    QString m_localRoot;
};

#endif // APPCONFIG_H