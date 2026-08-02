#ifndef ZLENS_H
#define ZLENS_H

#include <QObject>
#include <QPointer>
#include <QKeyEvent>
#include "ImagePlayer.h"
#include "VideoPlayer.h"
#include "schema.h"

/*
	不同播放器的统一管理类，可根据媒体类型自动选择需要的播放器
*/

class ZLens : public QObject
{
    Q_OBJECT

public:
    explicit ZLens(QObject *parent = nullptr);
    ~ZLens() override = default;

    // 统一入口：显示媒体（自动判断类型），传入media_elem，内部开始 初始化播放器 以及 显示
    void showMedia(const media_elem& media);
	
	// 关闭所有窗口
	void close();

signals:
    // 转发给外层（ImageGridWidget），用于切换上一张/下一张
    void nextMedia(int step);
    // 转发给外层，用于打开添加标签窗口
    void addTag(int media_id);

private slots:
    // 图片加载完成（或失败）时的处理
    void onImageLoaded(bool success);
    // 视频加载完成（或失败）时的处理
    void onVideoLoaded(bool success);

private:
	// 创建图片播放器
    void createImagePlayer();
	// 创建视频播放器
    void createVideoPlayer();
	
    void showCurrentPlayer();

	// 使用QPointer管理两中播放器
    QPointer<ImagePlayer> m_imagePlayer;
    QPointer<VideoPlayer> m_videoPlayer;

    bool m_isImage;    // 当前媒体类型
};

#endif // ZLENS_H