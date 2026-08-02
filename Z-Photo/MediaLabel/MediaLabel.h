#ifndef IMAGELABEL_H
#define IMAGELABEL_H

#include <QLabel>
#include <QPixmap>
#include <QPainter>     // 用于 paintEvent 中的绘制
#include <QWidget>      // 基类
#include <QRect>
#include <QString>
#include <QMouseEvent> // 鼠标事件需要引入

#include<string>

#include "schema.h"
#include "AppConfig.h"

/*
	MediaLabel，网格布局中的元素
	支持：
	1.随窗口尺寸变化重绘制图片
	2.被点击时发送clicked(const QString& filePath)信号
*/

class MediaLabel : public QLabel
{
    Q_OBJECT
signals:
    void clicked(const media_elem& media);
	
public:
	// 构造函数
	explicit MediaLabel(const media_elem& media, QWidget* parent = nullptr);
	
	// 析构函数
    ~MediaLabel() override;
	
    // （可选）重写 sizeHint 返回极小值
    QSize sizeHint() const override;

	// 加载media的缩略图
	// 统一接口，根据mode选择运行 loadImage_local 或 loadImage_net
	void loadImage(AppConfig::Mode mode);
    void loadImage_local();
	void loadImage_net(QString host, int port);

	// 卸载图片，节省内存
    void unloadImage();

	// 判断图片是否加载
    bool isLoaded() const;
	// 获得图片路径
    QString path() const;

protected:
	// 重写 paintEvent，自定义绘制事件发生时的行为 实现自定义绘制
    void paintEvent(QPaintEvent* event) override;
	
	// 重写mousePressEvent，自定义鼠标点击事件发生时的行为 对外部发送click信号
	void mousePressEvent(QMouseEvent* event) override;
    
private:
	// 判断是否加载缩略图
    bool m_loaded;
    QPixmap m_pix;  // 存储原始图片

public:
	// media_elem，与media表中字段保持一致，作为被点击时，发送的媒体元数据
	media_elem med;
};


#endif