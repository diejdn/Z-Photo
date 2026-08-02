#ifndef MEDIAHTTPSERVER_H
#define MEDIAHTTPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QUrl>
#include <QDebug>
#include <QStringList>
#include <QString>

class MediaHttpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit MediaHttpServer(const QString& rootDir, QObject *parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    // 数据根目录，用于寻找和url拼接寻找文件
    QString m_rootDir;

    // URL解码
    QString urlDecode(const QString &src);
    // 获取MIME类型
    QString getMimeType(const QString &path);
    // 处理单个客户端连接
    void handleClient(QTcpSocket *sock);
    // 发送403
    void send403(QTcpSocket *sock);
    // 发送404
    void send404(QTcpSocket *sock);
    // 发送416
    void send416(QTcpSocket *sock, qint64 fileSize);
};

#endif // MEDIAHTTPSERVER_H