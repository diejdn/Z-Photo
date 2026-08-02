#include "VideoPlayer.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QPixmap>
#include <QTimer>

#include "AppConfig.h"   

VideoPlayer::VideoPlayer(QWidget *parent)
	: QWidget(parent)
{
		setAttribute(Qt::WA_DeleteOnClose);
		setStyleSheet("background: #181818;");
		setWindowTitle("zVideo - whit...");
        resize(800, 600);
		
		// 关闭标题栏
		// setWindowFlags(Qt::FramelessWindowHint);

// 主布局：视频 + 底部控制条 m_videoWidget + controlBar
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

// 主布局1: 视频输出区域（占满剩余空间）
		// QVideoWidget 是 Qt 提供的视频渲染专用控件。
        m_videoWidget = new QVideoWidget(this);
		// addWidget(..., 1)：拉伸系数为 1，表示视频区域占据所有剩余空间（控制条固定高度后，剩余空间全部分配给视频）。
        // Leftlayout->setStretchFactor(dir_btn, 1);设置权重，具体大小和布局中其他组件按比例分分配
		mainLayout->addWidget(m_videoWidget, 1);

// 主布局2: 底部控制条区域（高度仅30像素）暂停 + 进度条 + 时长
        QWidget *controlBar = new QWidget(this);
		// 固定高度 30 像素，不会随窗口拉伸。
        controlBar->setFixedHeight(30);  
		// 透明背景 让视频画面透过控制条显示，实现覆盖效果。
        controlBar->setStyleSheet("background: transparent;"); 
		// 水平布局，添加 暂停按钮 和 进度条
        QHBoxLayout *controlLayout = new QHBoxLayout(controlBar);
		// 左右边距 5 像素，让按钮和进度条不贴边。
        controlLayout->setContentsMargins(5, 0, 5, 0);
        controlLayout->setSpacing(5);

// 底部控制条1： 进度条（极细）水平方向滑动条。 范围 0~1000，用千分比表示进度，避免直接用时间戳做整数除法精度问题。
        m_progressSlider = new QSlider(Qt::Horizontal, this);
        m_progressSlider->setRange(0, 1000);
        // 样式：细进度条，隐藏滑块手柄
		/*
			groove	滑槽（轨道背景），灰色半透明细条
			sub-page	已填充的部分（进度），蓝色（#1e90ff）
			handle	滑块手柄，宽度设为 0 像素，完全隐藏，实现“无圆点”进度条
		*/
		m_progressSlider->setStyleSheet(
			// 滑槽（轨道）
			"QSlider::groove:horizontal {"
			"    height: 4px;"
			"    background: rgba(255,255,255,0.25);"
			"    border-radius: 2px;"
			"}"
			// 已填充部分（进度）
			"QSlider::sub-page:horizontal {"
			"    background:  #FB7299;"
			"    border-radius: 2px;"
			"}"
			// 滑块手柄（圆形）
			"QSlider::handle:horizontal {"
			"    width: 12px;"
			"    height: 12px;"
			"    background: #FB7299;"
			"    border-radius: 6px;" // 半径，应为w h（） 的一半才能显示出圆的
			"    margin: -4px 0px;"   /* 垂直居中于滑槽（滑槽高度4px，手柄14px，上移5px使中心对齐） */
			"}"
			// 滑块手柄（圆形）鼠标悬停时
			"QSlider::handle:horizontal:hover {"
			"	width: 14px;"
			"	height: 14px;"
			"   border-radius: 7px;"
			"	margin: -5px 0;"
			"}"
		);
		/*
			用户拖动进度条时触发 seek 槽，跳转到对应位置。
			注意：sliderMoved 在拖动过程中连续触发，适合实时定位。
			
			sliderMoved 参数int 滑动条位置变化直接触发信号，
			滑动时每次都合调用seek渲染画面，速度过快，会导致硬件跟不上
			所以在sliderMoved时只设置标记位
			
			sliderReleased 无参数 滑倒最终位置释放，才使用seek渲染到对应画面
		*/
        connect(m_progressSlider, &QSlider::sliderMoved, this, &VideoPlayer::Dragging);
		connect(m_progressSlider, &QSlider::sliderReleased, this, &VideoPlayer::seek);
		
// 底部控制条2： 暂停/播放按钮
        m_playBtn = new QPushButton(this);
		// style()->standardIcon(QStyle::SP_MediaPlay)：获取 Qt 内置的标准播放图标（不依赖外部资源文件）。
        m_playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
		// 固定大小 24×24 像素。
        m_playBtn->setFixedSize(24, 24);
		/*
			默认透明，无边框。
			鼠标悬停时显示半透明白色圆形背景，提升交互反馈。
		*/
        m_playBtn->setStyleSheet(
            "QPushButton {"
			"    background: #FB7299;"
			"    border-radius: 12px;"
            "}"
            "QPushButton:hover {"
            "    background: #ff88aa;"
            "    border-radius: 12px;"
            "}"
        );
		// 点击时切换播放/暂停状态。
        connect(m_playBtn, &QPushButton::clicked, this, &VideoPlayer::togglePlay);

// 底部控制条3： 视频时长显示
		m_timeLabel = new QLabel("00:00 / 00:00", this);
		m_timeLabel->setStyleSheet("color: white; font-size: 12px;");
		// 固定宽度以避免布局抖动（根据你的需求调整）
		m_timeLabel->setFixedWidth(90);
		
// 底部控制条4： 是否静音
		// 静音状态图标
		m_muteLabel = new QLabel(this);
		m_muteLabel->setFixedSize(20, 20);  // 固定大小，与图片尺寸匹配
		m_muteLabel->setScaledContents(true);  // 让图片自动适应标签大小
		
// 底部控制条5： 显示音量
		m_volumeLabel = new QLabel(this);
		m_volumeLabel->setFixedWidth(40);
		m_volumeLabel->setStyleSheet("color: white; font-size: 12px;");
		m_volumeLabel->setText("100%");
		
// 底部控制条6： 是否循环播放
		m_repeatLabel = new QLabel(this);
		m_repeatLabel->setFixedSize(20, 20);  // 固定大小，与图片尺寸匹配
		m_repeatLabel->setScaledContents(true);  // 让图片自动适应标签大小
		
		/*
			按钮在左，进度条在右（拉伸系数 1，占据大部分空间）。

			控制条作为整体添加到主布局底部。	
		*/
		controlLayout->addWidget(m_playBtn);
        controlLayout->addWidget(m_progressSlider, 1);   // 进度条占据大部分空间
		controlLayout->addWidget(m_timeLabel);
		controlLayout->addWidget(m_muteLabel);
		controlLayout->addWidget(m_volumeLabel);
		controlLayout->addWidget(m_repeatLabel);

        mainLayout->addWidget(controlBar);

// 媒体播放器 设置视频输出到 QVideoWidget *m_videoWidget; 音频输出到 QAudioOutput m_audioOutput
		/*
			QMediaPlayer：核心播放引擎，支持本地文件和网络流。
			QAudioOutput：音频输出控制（音量、静音）。
			将音频和视频输出分别绑定到 QAudioOutput 和 QVideoWidget。
		*/
		m_player = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
		// m_audioOutput 创建后，更新m_muteLabel
		//updateMuteAndDisplay();
        m_player->setAudioOutput(m_audioOutput);
        m_player->setVideoOutput(m_videoWidget);
		
		// 绑定在 m_player 的信号槽
		/*
		信号							触发时机				用途
		positionChanged(qint64)		播放位置变化			更新进度条
		durationChanged(qint64)		媒体时长确定			初始化进度条范围
		playbackStateChanged(State)	播放/暂停/停止		切换按钮图标
		
		QMediaPlayer::durationChanged 确实表示媒体文件已经加载并解析出总时长。
		它是在媒体源（如视频文件）加载完成后、播放器获取到媒体信息时触发的，
		这时可以安全地获取 duration() 并更新 UI（如显示总时长）。
		*/
        connect(m_player, &QMediaPlayer::positionChanged, this, &VideoPlayer::onPositionChanged);
        connect(m_player, &QMediaPlayer::durationChanged, this, &VideoPlayer::onDurationChanged);
        connect(m_player, &QMediaPlayer::playbackStateChanged, this, &VideoPlayer::onPlaybackStateChanged);
		
		// 错误处理
		/*
			当加载失败（文件不存在、格式不支持、解码器缺失等）时，QMediaPlayer 会发出 errorOccurred 信号，
			并附带错误码和描述字符串。
		*/
		connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error err, const QString &msg) {
			qWarning() << " 播放器错误：" << msg;
		});
		
		// 需要焦点，防止这两捕获左右方向键
		m_progressSlider->setFocusPolicy(Qt::NoFocus);
		m_playBtn->setFocusPolicy(Qt::NoFocus);
		
		// 初始化med
		med = {-1, "","","",0,""};
}

// 使用media设置图片，供切换图片使用支持 本地 + 网络
void VideoPlayer::setMedia(const media_elem& media) {
    if (med.media_id == media.media_id) {
        qDebug() << "VideoPlayer::setMedia: same media, skip";
        return;
    }
    med = media;

    QString path = QString::fromStdString(media.file_path);
    qDebug() << "VideoPlayer::setMedia file_path:" << path;
	
	// 设置弹窗标题
    setWindowTitle("zVideo - " + QFileInfo(path).fileName());

    // ---- 重要：先连接信号，再设置 source ----
    // 断开旧的连接 mediaStatusChanged（防止重复连接导致多次发射）
    disconnect(m_player, &QMediaPlayer::mediaStatusChanged, this, nullptr);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::LoadedMedia ||
                    status == QMediaPlayer::BufferedMedia) {
                    // 加载成功，通知上层可以显示窗口了
                    qDebug() << "VideoPlayer media loaded successfully";
                    emit mediaReady(true);
                } 
				else if (status == QMediaPlayer::InvalidMedia) {
                    // 加载失败（文件损坏、网络404等）
                    qWarning() << "VideoPlayer media invalid!";
                    emit mediaReady(false);
                }
				else if (status == QMediaPlayer::EndOfMedia && is_repeat) {
					m_player->setPosition(0);
					m_player->play();
				}
            });

    // 构造 URL（本地或网络）
    QUrl url;
    if (AppConfig::instance().mode() == AppConfig::Local) {
        // 本地模式，已经拼接rootPath
        url = QUrl::fromLocalFile(path);
    } else {
        QString baseUrl = QString("http://%1:%2/videos/")
                          .arg(AppConfig::instance().serverHost())
                          .arg(AppConfig::instance().serverPort());
        // 路径编码（处理空格/中文）
        url = QUrl(baseUrl + QUrl::toPercentEncoding(path));
    }

    // 设置视频源
    m_player->setSource(url);

    // 检查是否已经加载完成（极快的情况，比如本地小文件）
    QMediaPlayer::MediaStatus currentStatus = m_player->mediaStatus();
    if (currentStatus == QMediaPlayer::LoadedMedia ||
        currentStatus == QMediaPlayer::BufferedMedia) {
        // 如果已经加载完成，但信号已经连接，可能会重复发射，但没关系，槽函数内可做去重
        qDebug() << "VideoPlayer::setMedia media already loaded";
        emit mediaReady(true);
    } else if (currentStatus == QMediaPlayer::InvalidMedia) {
        emit mediaReady(false);
    }

    // 可选：自动播放
    // 如果你希望加载完成后自动播放，这里调用 play()
    // 但也可以让用户在窗口显示后点击播放按钮
    // 为了与 ImagePlayer 行为一致（图片显示即呈现），建议调用 play()
     //m_player->play();
}

void VideoPlayer::play(){
	if(m_player){
		m_player->play();
	}
}
	
// m_playBtn暂停按钮 切换播放状态 检查当前状态，取反操作。
void VideoPlayer::togglePlay()
{	
	// PlayingState正在播放
    if (m_player->playbackState() == QMediaPlayer::PlayingState){
		m_player->pause();
	}  
    else{
		m_player->play();
	}     
}

// m_progressSlider QSlider::sliderMoved （手动拖动） 设置 m_isDragging = true; 表示正在拖动进度条
void VideoPlayer::Dragging(int value){
	Q_UNUSED(value);
	// 直接赋值就行，不需要判断，因为 mov 比 分支 更高效
	m_isDragging = true;
}

/*
		m_progressSlider QSlider::sliderReleased 不在拖动进度条
		（进度条位置确定，用户拖到了想要的位置）
		
		重定位视频位置（使用进度条值 m_progressSlider->value() 计算出来）
		
		QMediaPlayer m_player->duration() 返回总时长（毫秒）。
		千分比 × 总时长 / 1000 → 目标位置（毫秒）。
		先判断 duration > 0 避免除零。
*/
void VideoPlayer::seek()
{
	int value = m_progressSlider->value();
	if (m_player->duration() > 0) {
		qint64 pos = (qint64)value * m_player->duration() / 1000;				
		m_player->setPosition(pos);
	}
		
	// 延迟 50ms 重置标志，给 positionChanged 留出时间
	QTimer::singleShot(50, this, [this]() {
		m_isDragging = false;
	});
}

/*
		位置变化更新进度条 m_player, &QMediaPlayer::positionChanged 
		qint64 pos当前视频播放位置
		
		m_player->duration() 视频总时长 毫秒
*/
void VideoPlayer::onPositionChanged(qint64 pos)
{
	// 当用户没有托进度条时才能够更新m_progressSlider的值
	if(m_isDragging == false){
		if (m_player->duration() > 0) {
			int value = (int)(pos * 1000 / m_player->duration());
			// 设置进度条的值（即进度条前进）
			m_progressSlider->setValue(value);
		}
			
		// 更新m_timeLabel
		updateTimeLabel(pos, m_player->duration());
	}
}

/*
		媒体时长确定(才加载视频的时候) 初始化进度条范围
		m_player, &QMediaPlayer::durationChanged 
		设置进度条范围，与构造函数中一致。 Q_UNUSED 防止编译器警告未使用参数。
		
		duration视频总时长
		
		m_player->position() 当前视频播放位置 毫秒
*/
void VideoPlayer::onDurationChanged(qint64 duration)
{
    // 视频刚加载时m_player->position()返回0， 这里可以直接写0
	updateTimeLabel(m_player->position(), duration);
	// 初始化静音/无静音图标
	updateMuteAndDisplay(false); 
	// 初始化静音/无静音图标
	updateRepeatAndDisplay(false);
	m_progressSlider->setRange(0, 1000);
}

/*
		播放状态变化更新图标
		
		m_player, &QMediaPlayer::playbackStateChanged 即视频暂停或播放
		时，切换m_playBtn的图标
		
		m_playBtn 点击，暂停视频 -> m_player playbackStateChanged -> m_playBtn变化图标
*/
void VideoPlayer::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    QStyle::StandardPixmap icon = (state == QMediaPlayer::PlayingState) ?
                                      QStyle::SP_MediaPause : QStyle::SP_MediaPlay;
    m_playBtn->setIcon(style()->standardIcon(icon));
}

// 自定义槽函数，实现全屏 调用 showFullScreen() 时，Qt 会自动移除窗口边框和标题栏，
void VideoPlayer::toggleFullScreen()
{
    if (m_isFullScreen) {
		// 已经是全屏，则回到正常大小
        showNormal();
		
		// 重置按钮的文字
        // m_fullScreenBtn->setText("⛶ 全屏");
    } else {
		// 使用QWidget的方法实现全屏
        showFullScreen();
		
		// 重置按钮的文字
        // m_fullScreenBtn->setText("⛶ 退出全屏");
    }
    m_isFullScreen = !m_isFullScreen;
}

// 更新 m_timeLabel 时间标签 参数：视频当前位置 视频总时长
void VideoPlayer::updateTimeLabel(qint64 current, qint64 total) {
	QString currentStr = formatTime(current);
	QString totalStr = formatTime(total);
	m_timeLabel->setText(currentStr + " / " + totalStr);
}

// 辅助函数，将qint64 的毫秒 转换为 分钟 秒 的形式
QString VideoPlayer::formatTime(qint64 ms) {
	qint64 totalSec = ms / 1000;
	qint64 minutes = totalSec / 60;
	qint64 seconds = totalSec % 60;
	return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

// 更新m_muteLabel 是否静音图标
void VideoPlayer::updateMuteAndDisplay(bool is_change) {
	if (m_audioOutput) {
		bool muted = m_audioOutput->isMuted();
		if(is_change){
			muted = !muted;  // 切换状态
			m_audioOutput->setMuted(muted); 
		}
			
		// 加载对应图标 isMuted() false为非静音 true为静音
		QString iconPath = muted ? ":/icon/volume-off.png" : ":/icon/volume.png";
		QPixmap pix(iconPath);
		if (!pix.isNull()) {
			m_muteLabel->setPixmap(pix.scaled(m_muteLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		} else {
			// 如果加载失败，使用文本备用
			m_muteLabel->setText(muted ? "🔇" : "🔊");
		}
	}
}

// 更新updateVolumeAndDisplay 显示音量
void VideoPlayer::updateVolumeAndDisplay(qreal step){
	if (m_audioOutput) {
		qreal volume = m_audioOutput->volume();
		volume = qMax(qMin(volume + step, 1.0), 0.0);
			
		m_audioOutput->setVolume(volume);
			
		int percent = (int)(volume * 100);
		m_volumeLabel->setText(QString("%1%").arg(percent));
	}
}

// 更新updateRepeatDisplay 切换 循环播放/播完暂停 图标
void VideoPlayer::updateRepeatAndDisplay(bool is_change){	
	// 不切换，即初始化，is_change == false 
	if(is_change){
		is_repeat = !is_repeat;
	}
	
	// 加载对应图标 m_repeatLabel true 循环播放 false 播完暂停
	QString iconPath = is_repeat ? ":/icon/repeat.png" : ":/icon/repeat-off.png";
	QPixmap pix(iconPath);
	if (!pix.isNull()) {
		m_repeatLabel->setPixmap(pix.scaled(m_repeatLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	} else {
		// 如果加载失败，使用文本备用
		m_repeatLabel->setText(is_repeat ? "⟳" : "◻");
	}
}

// 重载 按下键 和 松开键
void VideoPlayer::keyPressEvent(QKeyEvent *event) {
	// 按下r 右 时判断是否为重复事件（即长按） 此处实现长按倍速
	if (event->key() == Qt::Key_R || event->key() == Qt::Key_Right) {
		/*
			首次按下时event->isAutoRepeat()位false
			长按，接下来的event->isAutoRepeat()就是true
		*/
		if (event->isAutoRepeat()) {
			// 长按：仅加速，不跳转，这里设置为1.5倍速
			if (m_player->playbackState() == QMediaPlayer::PlayingState) {
				m_player->setPlaybackRate(1.5);
				m_isRightPressed = true;
			}
		} 
		event->accept();
	}
	// 按空格键可暂停或播放视频，和m_playBtn的槽togglePlay一样的逻辑 PlayingState 表示视频正在播放
	else if(event->key() == Qt::Key_Space){
		if (m_player->playbackState() == QMediaPlayer::PlayingState){
			m_player->pause();
		}
		else{
			m_player->play();
		}
		event->accept();
	}
	// 按m静音，内部调用updateMuteAndDisplay(); 静音切换 + 静音图标切换
	else if(event->key() == Qt::Key_M){
		if (m_audioOutput) {
			updateMuteAndDisplay();
		}
		event->accept();
	}
	// 按上下键缩放音量 VOLUEM_STEP音量步长 调用updateVolumeAndDisplay 音量调整 + 音量图标调整
	else if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
		if (event->key() == Qt::Key_Up) {
			updateVolumeAndDisplay(VOLUEM_STEP);
		} else {
			updateVolumeAndDisplay(-VOLUEM_STEP);
		}
		event->accept();
	}
	// 按 f11 全屏 （全屏 != 最大化，全屏是字面意思）
	else if(event->key() == Qt::Key_F11){
		toggleFullScreen();
	}
	// 按e 切换循环播放 / 播完暂停
	else if(event->key() == Qt::Key_E){
		updateRepeatAndDisplay();
	}
	// 按t添加标签
	else if(event->key() == Qt::Key_T){
		emit addtag(med.media_id);
	}
	// a d 切换上一个下一个
	else if(event->key() == Qt::Key_A){
		emit next(-1);
	}
	else if(event->key() == Qt::Key_D){
		emit next(1);
	}
	else {
		QWidget::keyPressEvent(event);
	}
}
// 重写keyReleaseEvent 键释放逻辑，在此处实现 快进结束 或 跳跃
void VideoPlayer::keyReleaseEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_R || event->key() == Qt::Key_Right) {
		/*
			点按的event->isAutoRepeat()为false 直接进入下面，
			长按的释放后event->isAutoRepeat()为false，才进入下面
		*/
		if(event->isAutoRepeat()){
			// 忽略自动重复的释放事件
			event->accept();
			return;
		} 
		// 点按 event->isAutoRepeat() !m_isRightPressed = false true，可以直接进入
		if(!m_isRightPressed){
			// 点按右键 向前跳跃五秒
			qint64 pos = m_player->position() + SEEK_STEP;
			if (pos > m_player->duration()){
				pos = m_player->duration();
			} 
			m_player->setPosition(pos);
				
			// 确保状态为 false（虽然本来也是 false，但显式重置更安全）
			m_isRightPressed = false;
		}
		// 长按 event->isAutoRepeat() m_isRightPressed = false true，在结束isAutoRepeat()后进入
		else if(m_isRightPressed){
			// 恢复速度
			m_player->setPlaybackRate(1.0);
				
			// 重置控制参数m_isRightPressed
			m_isRightPressed = false;
		}
			
		event->accept();
	}
	else if(event->key() == Qt::Key_L || event->key() == Qt::Key_Left){
		// 左键：跳转后退5秒（不受长按影响）
		qint64 pos = m_player->position() - SEEK_STEP;
		if (pos < 0) pos = 0;
		m_player->setPosition(pos);
		event->accept();
	}
	else {
		QWidget::keyReleaseEvent(event);
	}
}