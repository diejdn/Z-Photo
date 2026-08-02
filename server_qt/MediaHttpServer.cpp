#include "MediaHttpServer.h"

MediaHttpServer::MediaHttpServer(const QString& rootDir, QObject *parent)
    : QTcpServer(parent)
{
	m_rootDir = rootDir;
	qDebug() << "init m_rootDir: " << m_rootDir;
}

void MediaHttpServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *sock = new QTcpSocket(this);
    sock->setSocketDescriptor(socketDescriptor);
    // 挂载接收缓冲区，解决分包问题
    sock->setProperty("buf", QByteArray());
    handleClient(sock);
}

QString MediaHttpServer::urlDecode(const QString &src)
{
    return QUrl::fromPercentEncoding(src.toUtf8());
}

QString MediaHttpServer::getMimeType(const QString &path)
{
    if (path.endsWith(".mp4", Qt::CaseInsensitive))
        return "video/mp4";
    if (path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".jpeg", Qt::CaseInsensitive))
        return "image/jpeg";
    if (path.endsWith(".png", Qt::CaseInsensitive))
        return "image/png";
    if (path.endsWith(".webp", Qt::CaseInsensitive))
        return "image/webp";
    if (path.endsWith(".gif", Qt::CaseInsensitive))
        return "image/gif";
    return "application/octet-stream";
}

void MediaHttpServer::send403(QTcpSocket *sock)
{
    QByteArray resp = "HTTP/1.1 403 Forbidden\r\nContent-Length:10\r\n\r\nForbidden";
    sock->write(resp);
    sock->disconnectFromHost();
}

void MediaHttpServer::send404(QTcpSocket *sock)
{
    QByteArray resp = "HTTP/1.1 404 Not Found\r\nContent-Length:9\r\n\r\nNot Found";
    sock->write(resp);
    sock->disconnectFromHost();
}

void MediaHttpServer::send416(QTcpSocket *sock, qint64 fileSize)
{
    QByteArray resp;
    resp += "HTTP/1.1 416 Range Not Satisfiable\r\n";
    resp += "Content-Range: bytes */" + QByteArray::number(fileSize) + "\r\n";
    resp += "Content-Length:0\r\n\r\n";
    sock->write(resp);
    sock->disconnectFromHost();
}

void MediaHttpServer::handleClient(QTcpSocket *sock)
{
    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        // 先取出缓冲区副本
        QByteArray buf = sock->property("buf").toByteArray();
        buf += sock->readAll();

        // HTTP 请求结束标记 \r\n\r\n
        int endPos = buf.indexOf("\r\n\r\n");
        if (endPos == -1)
        {
            // 还没有完整请求，存回缓冲区直接返回
            sock->setProperty("buf", buf);
            return;
        }

        // 切出完整请求
        QByteArray rawReq = buf.left(endPos + 4);
        QByteArray remainBuf = buf.mid(endPos + 4);
        sock->setProperty("buf", remainBuf); // 剩余数据保存，用于长连接下一条请求

        QString req = QString::fromUtf8(rawReq);
        QStringList lines = req.split("\r\n");
        if (lines.isEmpty())
        {
            sock->disconnectFromHost();
            return;
        }

        // 解析请求首行 GET /videos/xxx HTTP/1.1
        QString firstLine = lines[0];
        QStringList firstParts = firstLine.split(" ", Qt::SkipEmptyParts);
        if (firstParts.size() < 2)
        {
            sock->disconnectFromHost();
            return;
        }
        QString method = firstParts[0];
        QString reqPath = firstParts[1];

        // 提取Range头
        QString rangeValue;
        for (const auto &line : lines)
        {
            if (line.startsWith("Range:", Qt::CaseInsensitive))
            {
                rangeValue = line.mid(6).trimmed();
                break;
            }
        }

        // 路由匹配
        QString relativePath;
        bool isVideo = false;
        bool isImage = false;

        if (reqPath.startsWith("/videos/"))
        {
            relativePath = urlDecode(reqPath.mid(8));
            isVideo = true;
        }
        else if (reqPath.startsWith("/images/"))
        {
            relativePath = urlDecode(reqPath.mid(8));
            isImage = true;
        }
        else
        {
            sock->disconnectFromHost();
            return;
        }

        QString fullPath = m_rootDir + relativePath;
        // 防目录穿越
        if (!fullPath.startsWith(m_rootDir))
        {
            send403(sock);
            return;
        }

        QFile file(fullPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            send404(sock);
            qDebug() << "404:" << fullPath;
            return;
        }
        qint64 fileSize = file.size();
        QString mime = getMimeType(fullPath);

        qint64 start = 0;
        qint64 end = fileSize - 1;
        bool useRange = false;

        if (!rangeValue.isEmpty() && rangeValue.startsWith("bytes="))
        {
            QString byteStr = rangeValue.mid(6);
            QStringList dashSplit = byteStr.split('-');
            start = dashSplit[0].toLongLong();

            if (dashSplit.size() >= 2 && !dashSplit[1].isEmpty())
            {
                end = dashSplit[1].toLongLong();
            }
            else
            {
                end = fileSize - 1;
            }

            // 边界判断
            if (start >= fileSize)
            {
                send416(sock, fileSize);
                return;
            }
            if (end >= fileSize)
                end = fileSize - 1;

            if (start <= end)
                useRange = true;
        }

        qint64 sendLength = end - start + 1;
        QByteArray header;

        if (useRange)
        {
            header += "HTTP/1.1 206 Partial Content\r\n";
            header += "Content-Range: bytes "
                    + QByteArray::number(start) + "-"
                    + QByteArray::number(end) + "/"
                    + QByteArray::number(fileSize) + "\r\n";
        }
        else
        {
            header += "HTTP/1.1 200 OK\r\n";
        }

        header += "Accept-Ranges: bytes\r\n";
        header += "Content-Type: " + mime.toUtf8() + "\r\n";
        header += "Content-Length: " + QByteArray::number(sendLength) + "\r\n\r\n";

        sock->write(header);

        // 文件流式发送
        file.seek(start);
        const qint64 BUF_SIZE = 65536;
        qint64 remain = sendLength;
        while (remain > 0 && sock->isWritable())
        {
            QByteArray bufChunk = file.read(std::min(BUF_SIZE, remain));
            if (bufChunk.isEmpty())
                break;
            sock->write(bufChunk);
            remain -= bufChunk.size();
        }

        QString clientAddr = sock->peerAddress().toString();
        qDebug().noquote() << clientAddr
                           << (useRange ? "206" : "200")
                           << (isVideo ? "VIDEO" : "IMAGE")
                           << fullPath
                           << "range:" << start << "-" << end;
    });

    connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
}