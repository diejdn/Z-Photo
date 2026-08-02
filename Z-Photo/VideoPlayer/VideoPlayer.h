#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QKeyEvent>
#include <QTimer>
#include <QUrl>
#include "schema.h"

/*
	Z-Photo 实现的视频播放器
	
	用法：
	全屏/缩小： 			点按 F11
	
	快进： 				点按 r / 点按 右方向键 
	倒退： 				点按 l / 点按 左方向键 
	倍速(1.5)： 			长按 r / 长按 右方向键 
	更改播放进度：			手动拖动进度条 (拖动进度条顶部的圆圈，不支持点击进度条进行跳转)
	
	暂停： 				点按 空格键 / 点击播放器左下角暂停按钮
	
	静音： 			 	点按 m
	放大/缩小音量：		点按 上/下 方向键
	
	播完循环/暂停切换：	点按e (默认播完循环)
	
	为视频添加标签：		点按t (开启tag弹窗)
	
	切换下一个视频：		点按 d
	切换上一个视频：		点按 a
*/

class VideoPlayer : public QWidget
{
    Q_OBJECT
signals:
	// 切换图片信号
    void next(int step);
	
	// 添加标签信号 -> show 添加标签页面
	void addtag(int media_id);
	
    // 视频加载完成（或失败），通知上层窗口可以显示了
    void mediaReady(bool success);
	
public:
    explicit VideoPlayer(QWidget *parent = nullptr);
	
	// 设置图片，供切换图片使用
	// 提供 image 来切换图片，本地 + 网络
	void setMedia(const media_elem& image);
	void play();
	
private slots:
	// m_playBtn暂停按钮 切换播放状态 检查当前状态，取反操作。
    void togglePlay();
	
	// m_progressSlider QSlider::sliderMoved （手动拖动） 设置 m_isDragging = true; 表示正在拖动进度条
	void Dragging(int value);
		
	// 重定位视频位置（使用进度条值 m_progressSlider->value() 计算出来）
    void seek();

	// 位置变化更新进度条
	void onPositionChanged(qint64 pos);
	
	// 媒体时长确定(才加载视频的时候) 初始化进度条范围
	void onDurationChanged(qint64 duration);

	// 播放状态变化更新图标
	void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
	
	// 自定义槽函数，实现全屏
	void toggleFullScreen();
	
private:
	// 更新 m_timeLabel 时间标签 参数：视频当前位置 视频总时长
	void updateTimeLabel(qint64 current, qint64 total);
	
	// 辅助函数，将qint64 的毫秒 转换为 分钟 秒 的形式
	QString formatTime(qint64 ms);
	
	// 更新m_muteLabel 是否静音图标
	void updateMuteAndDisplay(bool is_change = true);
	
	// 更新updateVolumeAndDisplay 显示音量
	void updateVolumeAndDisplay(qreal step);
	
	// 更新updateRepeatDisplay 切换 循环播放/播完暂停 图标
	void updateRepeatAndDisplay(bool is_change = true);

	
	// 重载 按下键 和 松开键
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

private:
    QVideoWidget *m_videoWidget;
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QSlider *m_progressSlider;
    QPushButton *m_playBtn;
	QLabel *m_timeLabel; // 显示视频的 已播放时长 和 总时长
	QLabel *m_muteLabel;  // 显示是否静音
	QLabel *m_volumeLabel; // 显示当前音量
	QLabel *m_repeatLabel; // 切换循环播放/播完暂停
	bool is_repeat = true;	// 控制因子，控制是否循环
	
	bool m_isFullScreen = false; // 判断是否全屏
	
	/*
		设置标志位，用于判断用户是否在滑动进度条
		
		因为在滑动进度条时，视频正常播放
		
		信号QMediaPlayer::positionChanged + 槽onPositionChanged 根据视频进度更新进度条值
		会和用户手动拖动进度条 sliderMoved 相冲突
		
		最终就是，用户拖到想要的位置sliderReleased，和onPositionChanged同时设置进度条的值
		
		有可能设置为旧进度，即m_progressSlider->value();有可能时onPositionChanged设置的值
		
		视频更改到的位置 是使用 (qint64)value * m_player->duration() / 1000;计算的
		所有如果是旧进度（最终的赋值是onPositionChanged生效）则视频不会变化
	*/
	bool m_isDragging = false;
	
	// 用于判断是否长按右键，记录用户长按以及真正释放，false表示没有长按，true表示长按
    bool m_isRightPressed = false;
    const qint64 SEEK_STEP = 5000; // 跳跃步长 5秒
	
	const qreal VOLUEM_STEP = 0.05; // 音量步长 5%
	
public:
	// media_elem，与media表中字段保持一致
	media_elem med;
};

#endif