#include "ImagePlayer.h"
#include "ImageGraphicsView.h"      // 需要完整定义
#include <QFileInfo>
#include <QTimer>
#include "AppConfig.h"              // 因为使用了 AppConfig::instance()

// 构造函数 使用media，有图片元信息
ImagePlayer::ImagePlayer(QWidget* parent)
    : QWidget(parent), m_isFullScreen(false)
{
	setAttribute(Qt::WA_DeleteOnClose);    // 关闭时自动销毁
	setWindowTitle("zImage - whit...");
    resize(800, 600);

    // 主布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);   // 无边距，视图填满

    // 创建图片查看器ImageGraphicsView
    m_view = new ImageGraphicsView(this);
    layout->addWidget(m_view);
	
	// 设置窗口整体背景为柔和的颜色 #f5f3f0
	this->setStyleSheet("background-color: #FFFFFF;");
	// 使内部的图片视图背景透明，以显示窗口背景
	m_view->setStyleSheet("background: transparent;");
	
	// 初始化med
	med = {-1, "","","",0,""};
}

// 使用media设置图片，供切换图片使用支持 本地 + 网络 对应m_view->setImage(path); m_view->loadFromUrl(url);
void ImagePlayer::setMedia(const media_elem& media) {
	if(!isVisible()){
		show();
	}
	if(med.media_id == media.media_id){
		qDebug() << "ImagePlayer::setImage media no change";
		return;
	}
	
	med = media;
	
	// 获得file_path 转换为QString
    QString path = QString::fromStdString(med.file_path);
	qDebug() << "ImagePlayer::setImage media_file_path: " << path;
	// 设置弹窗标题
    setWindowTitle("zImage - " + QFileInfo(path).fileName());
	
	// 根据模式选择 ImageGraphicsView m_view 的加载方式
	if(AppConfig::instance().mode() == AppConfig::Local){
		m_view->setImage(path);
	}
	else if(AppConfig::instance().mode() == AppConfig::Network){
		m_view->loadFromUrl(AppConfig::instance().serverHost(), AppConfig::instance().serverPort(), path);
	}
}

// 重写KeyEvent键盘事件，实现按F11全屏
void ImagePlayer::keyPressEvent(QKeyEvent* event)
{
	// 查看案件是否为F11，是F11接受事件并全屏，不是F11将事件继续转发给父类
    if (event->key() == Qt::Key_F11) {
        toggleFullScreen();
    } 
	else if(event->key() == Qt::Key_H){
		hide();
	}
	/*
		左右方向键，则向上发信息，由上层ImageGridWidget进行处理
		1.左右方向键由子类ImageGraphicsView不处理发送到这里
		2.PreviewWindow本身就可以处理a d键
		
		a 左 上一个 ， d 右 下一个
	*/
	else if(event->key() == Qt::Key_A || event->key() == Qt::Key_Left ){
		emit next(-1);
	}
	else if(event->key() == Qt::Key_D || event->key() == Qt::Key_Right){
		emit next(1);
	}
	else if(event->key() == Qt::Key_T){
		emit addtag(med.media_id);
	}
	else {
        QWidget::keyPressEvent(event);
    }
}

// 自定义槽函数，实现全屏
void ImagePlayer::toggleFullScreen()
{
    if (m_isFullScreen) {
		// 已经是全屏，则回到正常大小
        showNormal();	
    } else {
		// 使用QWidget的方法实现全屏
        showFullScreen();
    }
    m_isFullScreen = !m_isFullScreen;
}

// 重写窗口关闭事件，输入日志
void ImagePlayer::closeEvent(QCloseEvent* event) {
    qDebug() << "ImagePlayer closeEvent triggered";
    QWidget::closeEvent(event);
}