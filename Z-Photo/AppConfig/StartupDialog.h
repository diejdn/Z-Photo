#ifndef STARTUPDIALOG_H
#define STARTUPDIALOG_H

#include <QDialog>
#include <QLineEdit>

/*
	配置弹窗，用于用户输入配置信息
	
	网络模式支持ipv4 ipv6
*/
class StartupDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StartupDialog(QWidget* parent = nullptr);

	// 判断是否为网络模式
    bool isNetworkMode() const;
	
	// 获取用户输入的远端ip 以及 端口
    QString serverHost() const;
    int serverPort() const;
	
	// 获取用户输入的数据库路径
    QString dbPath() const;
	// 获取用户输入的根路径（用于本地模式和数据库中相对路径进行拼接）
    QString dataRoot() const;

private:
    QLineEdit* m_ipEdit;
    QLineEdit* m_portEdit;
    QLineEdit* m_dbPathEdit;
    QLineEdit* m_dataRootEdit;
    bool m_isNetwork;
};

#endif // STARTUPDIALOG_H