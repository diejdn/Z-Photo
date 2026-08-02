#include "ZLens.h"
#include <QDebug>

ZLens::ZLens(QObject *parent)
    : QObject(parent), m_isImage(true)
{
}

// 创建图片播放器
void ZLens::createImagePlayer()
{
	// 如果已存在，则直接返回
    if (m_imagePlayer) return;

    m_imagePlayer = new ImagePlayer();
    // etAttribute 设置窗口关闭时自动销毁，QPointer 会自动置 nullptr
    m_imagePlayer->setAttribute(Qt::WA_DeleteOnClose);

    // 连接图片播放器的信号到 ZLens 的转发信号，转发到上层进行 切换/添加tag 的处理
    connect(m_imagePlayer, &ImagePlayer::next, this, &ZLens::nextMedia);
    connect(m_imagePlayer, &ImagePlayer::addtag, this, &ZLens::addTag);
    
	// 连接加载完成信号，当m_imagePlayer准备好后，调用槽函数ZLens::onImageLoaded完成 show 显示
    connect(m_imagePlayer->m_view, &ImageGraphicsView::imageLoaded,
            this, &ZLens::onImageLoaded);
}

void ZLens::createVideoPlayer()
{
	// 如果已存在，则直接返回
    if (m_videoPlayer) return;

    m_videoPlayer = new VideoPlayer();
	// etAttribute 设置窗口关闭时自动销毁，QPointer 会自动置 nullptr
    m_videoPlayer->setAttribute(Qt::WA_DeleteOnClose);

	// 连接视频播放器的信号到 ZLens 的转发信号，转发到上层进行 切换/添加tag 的处理
    connect(m_videoPlayer, &VideoPlayer::next, this, &ZLens::nextMedia);
    connect(m_videoPlayer, &VideoPlayer::addtag, this, &ZLens::addTag);
	
	// 连接加载完成信号，当m_videoPlayer准备好后，调用槽函数ZLens::onVideoLoaded完成 show 显示
    connect(m_videoPlayer, &VideoPlayer::mediaReady,
            this, &ZLens::onVideoLoaded);
}

// showMedia，建立窗口 + setMedia -> 组件发送加载信息 -> onImageLoaded onVideoLoaded来show
void ZLens::showMedia(const media_elem& media)
{
	// 判断媒体是 图片 还是 视频
    m_isImage = (media.media_type == 0);

    // （可选功能）如果当前播放器类型与媒体类型不匹配，隐藏另一个
    if (m_isImage) {
        //if (m_videoPlayer) m_videoPlayer->hide();
        createImagePlayer();
        m_imagePlayer->setMedia(media);
		
        // 图片加载完成后，onImageLoaded 会被触发并显示窗口
    } else {
        //if (m_imagePlayer) m_imagePlayer->hide();
        createVideoPlayer();
        m_videoPlayer->setMedia(media);
		
        // 视频加载完成后，onVideoLoaded 会被触发并显示窗口
    }
}

// 关闭m_imagePlayer和m_videoPlayer窗口
void ZLens::close() {
    if (m_imagePlayer) {
        m_imagePlayer->close();
        // 由于设置了WA_DeleteOnClose，关闭后会删除对象，QPointer会自动变为null，但我们无需手动置null
    }
    if (m_videoPlayer) {
        m_videoPlayer->close();
    }
}

// 当 m_imagePlayer 准备好后，开始渲染窗口
void ZLens::onImageLoaded(bool success)
{
    if (success && m_imagePlayer) {
        m_imagePlayer->show();
        m_imagePlayer->raise();
        m_imagePlayer->activateWindow();
		
        qDebug() << "ZLens: ImagePlayer shown after load";
    }
	else if(!success){
		qWarning() << "ZLens: Image load failed, window not shown";
	}
}

// 当 m_videoPlayer 准备好后，开始渲染窗口
void ZLens::onVideoLoaded(bool success)
{
    if (success && m_videoPlayer) {
        m_videoPlayer->show();
        m_videoPlayer->raise();
        m_videoPlayer->activateWindow();
		
		// show之后再播放视频
		m_videoPlayer->play();
		
        qDebug() << "ZLens: VideoPlayer shown after load";
    } else if (!success) {
        qWarning() << "ZLens: Video load failed, window not shown";
    }
}