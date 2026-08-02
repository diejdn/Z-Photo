#include "MediaLabel.h"
#include <QLabel>
#include <QPixmap>
#include <QPainter>     // 用于 paintEvent 中的绘制
#include <QWidget>      // 基类
#include <QRect>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

// 构造函数
MediaLabel::MediaLabel(const media_elem& media, QWidget* parent )
	: QLabel(parent), med(media), m_loaded(false){
		// setFixedSize固定大小
        // setFixedSize(150, 150);
		// 最小150x150，窗口缩小不会比这个更小；窗口放大自动拉伸，但是初始宽度不足以容纳4个150
		// 不设置最小限制，直接让网格使用列权重进行大小安排
		//setMinimumSize(150, 150);
		
		// 灰色占位底色
        setStyleSheet("background:#aaa;");
		
		// 告诉布局：不要根据内容调整大小，请完全由布局分配空间
		/*
		成功原因：1
			控件尺寸：由布局（QGridLayout）通过列权重和行拉伸全权决定，
			控件本身不再提供任何“内容大小”信息。
			
			sizePolicy 是 QWidget 的一个属性，
			它告诉布局管理器控件在水平方向和垂直方向上的伸缩意愿。
			常用策略有：
				Fixed：控件大小固定为 sizeHint()，不会拉伸或压缩。

				Minimum：控件大小至少为 sizeHint()，可以拉伸但不会小于 sizeHint。

				Preferred：控件首选大小为 sizeHint()，可以拉伸也可以压缩（但压缩不会低于 minimumSize）。

				Expanding：控件首选大小为 sizeHint()，但会尽可能多地占用额外空间（比 Preferred 更“贪婪”）。

				MinimumExpanding：与 Expanding 类似，但 sizeHint 是最小值，不会压缩。
		*/
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

		
		/* 
		setScaledContents默认false
			setScaledContents(false)：QLabel 不会自动拉伸 pixmap
			手动缩放的图片以设置的尺寸（即pix.scaled this->size()）显示，因此保持比例。
			
			setScaledContents(true)：QLabel 会忽略手动设置的 pixmap 的尺寸
			直接将图片拉伸填满整个标签区域（无论原始比例如何），导致图片变形
			如果同时使用了手动缩放，最终效果由 setScaledContents 决定（会覆盖手动缩放）。
		*/
        setScaledContents(false);
		
		// 设置最小尺寸，防止布局压缩尺寸失调
		setMinimumHeight(200);
		// 设置最大尺寸，保证放大时不失调
		setMaximumHeight(300); 
		
    }
	
// 析构函数
MediaLabel::~MediaLabel(){
	if(m_loaded){
		unloadImage(); 
	}
}
	
// （可选）重写 sizeHint 返回极小值
QSize MediaLabel::sizeHint() const{
    return QSize(10, 10);
}

// 加载缩略图，本地
void MediaLabel::loadImage_local()
{
	// 没有加载的标签才会加载
    if(m_loaded) return;
	
	m_pix.load(QString::fromStdString(med.thumbnail_path));
    if(!m_pix.isNull())
    {
        m_loaded = true;
        setStyleSheet(""); // 清除灰色背景
        update();          // 触发绘制paintEvent
    }
}


/*
加载缩略图，网络

1. 检查是否已加载或正在加载 → 是则直接返回
2. 设置 m_loading = true（防止重复请求）
3. 构建 URL（包含图片相对路径）
4. 通过 QNetworkAccessManager 发起异步 GET 请求
5. 连接 finished 信号到 lambda
6. 函数立即返回（不阻塞 UI）
7. 网络请求完成后，lambda 在主线程执行
8. 检查对象是否存活（QPointer）
9. 读取数据、加载 QPixmap、更新 UI
10. 释放 QNetworkReply

QNetworkAccessManager 单例
	static QNetworkAccessManager manager;
	它是 QObject 派生类，管理所有网络请求。
	使用 static 局部对象，确保整个程序生命周期内只有一个实例，复用连接池，减少资源开销。
	它默认会限制对同一主机的并发连接数（通常为 6），自动排队，避免瞬间发起过多请求。
	
QNetworkReply 与异步请求
	QNetworkReply* reply = manager.get(QNetworkRequest(url));
	get() 是非阻塞的，立即返回 QNetworkReply*。
	网络 I/O 在 Qt 底层线程中完成，不阻塞主线程。
	reply 会在请求完成、出错或重定向时发射 finished() 信号。
	通过信号槽捕获finished信号来加载图片
	
QPointer 防悬垂
	QPointer<MediaLabel> guard(this); 一个ImageLabel类型的指针
	QPointer 是一个弱引用指针，当所指向的 QObject 被销毁时，它会自动变为 nullptr。
	在 lambda 中捕获 guard（值捕获），回调执行时先检查 if (!guard)，如果对象已被删除，
	则安全退出，避免访问已释放内存。
	这解决了“网络请求慢，用户已关闭窗口/切换目录导致 MediaLabel 被销毁”的常见问题。
	
Lambda 捕获列表
	[this, reply, guard]
	this：用于在对象存活时调用成员函数（m_pix.loadFromData, setStyleSheet, update）。
	reply：用于读取数据和释放资源。
	guard：用于检查 this 是否还有效。
	
资源释放
	reply->deleteLater();
	QNetworkReply 是 QObject，必须手动释放，否则内存泄漏。
	deleteLater() 确保它在事件循环的下一次迭代中被安全删除，
	避免在回调中直接 delete 可能引发的递归问题。
	
错误处理与状态管理
	网络错误：reply->error() != QNetworkReply::NoError 时，
	打印错误信息并重置 m_loading = false，允许后续重试。

	数据加载失败：m_pix.loadFromData(data) 返回 false 时，
	同样重置 m_loading，但保留 m_loaded 为 false，以便再次尝试。

	成功加载：设置 m_loaded = true，清除灰色背景，调用 update() 触发重绘
	
与可视区懒加载的配合
	loadImage() 是非阻塞的，调用后立即返回，
	因此可视区检测（updateVisibleImages）可以快速遍历所有 MediaLabel，
	只需检查 isLoaded() 状态，无需等待网络。

	图片数据到达后，通过 update() 触发 paintEvent，此时 m_pix 已有效，会绘制出图片。

	如果用户在图片加载完成前滚出可视区，unloadImage() 会被调用，
	但此时网络请求可能仍在进行。由于 QPointer 的保护，回调会安全忽略，
	不会访问已卸载的 MediaLabel。
*/
void MediaLabel::loadImage_net(QString host, int port)
{
	// 如果已经加载缩略图，则直接返回
	if (m_loaded) return;

    // 构造 URL
	// 如果 file_path 包含空格、中文或特殊符号，需要对路径进行 URL 编码，否则服务器可能无法正确
	QString encodedPath = QUrl::toPercentEncoding(QString::fromStdString(med.thumbnail_path));
	QString url = QString("http://%1:%2/images/%3").arg(host).arg(port).arg(encodedPath);

    // 使用 QNetworkAccessManager 单例（或成员变量）
    static QNetworkAccessManager manager;
    QNetworkReply* reply = manager.get(QNetworkRequest(url));

    // 连接 finished 信号，使用 QPointer 防止悬垂
    QPointer<MediaLabel> guard(this);
    connect(reply, &QNetworkReply::finished, [this, reply, guard, url]() {
		// guard（MediaLabel）如果被销毁，就释放reply
        if (!guard) {
            return;
        }
		// 获取数据成功
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
			// QPixmap m_pix 使用二进制数据加载
            if (m_pix.loadFromData(data)) {
                m_loaded = true;
                setStyleSheet("");
                update(); // 触发绘制paintEvent
				
				// 加载缩略图成功，输出日志信息
                qDebug() << "MediaLabel::loadImage Load image from net succeessfully: " << url;
            } 
			else {
				 m_loaded = false;
				 
				// 加载二进制数据 失败
				qDebug() << "MediaLabel::loadImage Load image from net err: image Invalid " << url;
            }
        } 
		else {
			m_loaded = false;
			
			// 网络失败
			qDebug() << "ImageGraphicsView::loadFromUrl: net err " << reply->errorString()  << " url:"<< url;
        }
        reply->deleteLater();
    });
}

// 统一接口，自动处理 本地 / 网络模式
void MediaLabel::loadImage(AppConfig::Mode mode){
	if(mode == AppConfig::Local){
		loadImage_local();
	}
	else if(mode == AppConfig::Network){
		loadImage_net(AppConfig::instance().serverHost(), AppConfig::instance().serverPort());
	}
}

// 卸载缩略图，节省内存
void MediaLabel::unloadImage()
{
    m_pix = QPixmap();
    m_loaded = false;

	// 恢复灰色占位
    setStyleSheet("background:#aaa;");
    update();
}

// 判断图片是否加载
bool MediaLabel::isLoaded() const { return m_loaded; }
// 获得图片路径，原图路径，从 med 元信息中取
QString MediaLabel::path() const { return QString::fromStdString(med.file_path); }


/*
重写 paintEvent，自定义绘制事件发生时的行为 实现自定义绘制

	图片绘制：在 paintEvent 中根据控件的当前实际尺寸（this->rect()）实时计算缩放
	自行绘制图片，完全不受 QLabel 内部机制干扰。
	
关键：
	1.移除 setPixmap，改用成员变量存储原始图片
	2.重写 paintEvent 实现自定义绘制
	3.设置尺寸策略，忽略内容 setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	4. （可选）重写 sizeHint 返回极小值
		QSize sizeHint() const override { return QSize(10, 10); }
		虽然自定义绘制后 setPixmap 不再影响 sizeHint，
		但保留此重写可进一步防止其他意外影响，确保布局分配空间完全独立。
		
触发流程:
	update() 
		是一个请求重绘的槽函数。
		调用它会向 Qt 事件系统发送一个 QEvent::UpdateRequest 事件，并标记该控件需要重绘。
		该事件会被放入事件队列，等待当前事件循环处理。在事件循环的下一次迭代
		（即当前所有同步事件处理完毕后），QEvent::UpdateRequest 会被处理，进而调用 paintEvent。
		Qt 会合并多个连续的 update() 请求，避免多次重复绘制
		（例如在循环中连续调用 update() 100 次，最终只会触发一次 paintEvent，从而提高性能）。
		真正的 paintEvent 会在下一个事件循环周期被调用。
		
		当图片加载后，update() 被调用，Qt 会在稍后触发 paintEvent，
		根据最新的 m_pix 和控件当前尺寸重新绘制图片。
		paintEvent实现自定义绘制逻辑
		
		除了 update()，以下情况也会触发 paintEvent绘制事件：
			窗口首次显示时。
			控件大小改变（如窗口缩放、分割器拖动）。
			控件从隐藏变为显示（show()）。
			其他控件遮挡后重新暴露（系统自动发送重绘事件）。
			
			重点：
			resize也会触发paintEvent
			结果就是即便不用信号槽自定义捕获resize，label也会重新布局图片大小
			虽然没有可视区的检测
			
			&QMainWindow::resize是普通函数，不是信号
			不能用connect连接
				除非自定义窗口类继承QMainWindow编写信号
				或者：
				1.定义窗口不能用拖住边框的形式放大
				2.允许放大，但如果拖到图片加载缓冲之外，会是灰色，手动滚动滚轮即可加载
				3.扩大缓冲区范围
			
关于重加载（已加载图片修改尺寸）
	之前使用 setPixmap 时，图片缩放是在 loadImage 中一次性完成并设置给 QLabel，
	如果控件尺寸变化（如窗口放大），已加载的图片不会自动重新缩放，
	必须重新调用 loadImage 重新计算并设置 pixmap。
	
	现在使用 paintEvent 自定义绘制，每次绘制都会根据当前 rect() 实时计算缩放并绘制，
	因此只要触发 paintEvent，图片就会自动适配新尺寸。
	
	所以，对于已加载的图片，我们只需要调用 update() 请求重绘即可，无需卸载和重新加载。
*/
void MediaLabel::paintEvent(QPaintEvent* event)
{
	// 条件判断：未加载或图片无效
	/*
		m_loaded 是维护的加载状态标志；m_pix.isNull() 检查内部图片是否为空。

		如果尚未加载或图片无效，调用基类 QLabel::paintEvent(event)，
		让 QLabel 绘制其默认背景（由样式表设置的灰色背景 background:#aaa;）。
		这样就能显示占位灰色。

		return 终止函数，不继续绘制图片。
	*/
    if (!m_loaded || m_pix.isNull()) {
        // 未加载或图片无效，调用父类绘制灰色背景
        QLabel::paintEvent(event);
        return;
    }

	// 创建 QPainter 并开启平滑变换
	/*
		QPainter painter(this);：
			创建一个绘制器，
			作用于当前控件（this）。所有绘制操作都会在这个控件的画布上进行。

		setRenderHint(QPainter::SmoothPixmapTransform);：
			告诉 QPainter 在缩放图片时使用平滑插值算法（抗锯齿），
			使放大或缩小的图片更平滑、减少锯齿。这是实现高质量缩放的关键。
	*/
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

	// 获取控件尺寸和图片原始尺寸
	/*
		this->rect()：
			返回控件自身的矩形区域（相对于自身坐标系，左上角为 (0,0)，
			宽高为当前控件的实际像素尺寸）。这是我们用来容纳图片的目标矩形。

		m_pix.size()：
			原始图片的尺寸（像素）。

		pixSize.isEmpty() 
			检查图片是否无效（如宽或高为0），若是则直接返回，避免除以0。
		*/
    QRect rect = this->rect();
    QSize pixSize = m_pix.size();
    if (pixSize.isEmpty()) return;

    // 计算缩放比，使图片覆盖整个标签（保持比例）
	/*
		scaleW：
			控件宽度相对图片宽度的比例。如果图片宽 500，控件宽 200，
			则 scaleW = 0.4，即图片需要缩小到 0.4 倍才能刚好适应宽度。

		scaleH：
			同理，控件高度相对图片高度的比例。

		qMax(scaleW, scaleH)：
			取两者中的较大值。这一步决定了图片缩放后，
			至少在一个方向上等于控件的尺寸，另一个方向会大于或等于控件尺寸。
			
		示例：
		无裁剪：
			控件 200×150，图片 400×300 → scaleW=0.5，scaleH=0.5，
			二者相等，图片缩放后正好填满控件（无裁剪）。
				
		有裁剪：
			若控件 200×150，图片 600×400 → scaleW≈0.333，scaleH=0.375，
			取较大者 0.375，缩放后图片宽度 = 600×0.375 = 225
			（大于控件宽 200），高度 = 400×0.375 = 150（正好等于控件高）。
			这样图片在高度方向刚好匹配，宽度方向超出，实现了“覆盖”效果，且比例保持。
				
			如果按0.333来缩放，400*0.333 < 400*0.375 导致无法填充
				
		如果标签的尺寸大于图片
			例如控件200*200 图片100*50
			则scaleW = 2 scaleH = 4
			图片按4放大到400*200， 仍可以填充标签
				
		QSize targetSize 计算图片缩放后的尺寸
			将原始图片的宽高乘以统一的缩放比例 scale，
			得到绘制时图片的尺寸。这个尺寸在至少一个维度等于控件尺寸，另一个维度可能更大。	
		*/
    qreal scaleW = (qreal)rect.width() / pixSize.width();
    qreal scaleH = (qreal)rect.height() / pixSize.height();
    qreal scale = qMax(scaleW, scaleH);
    QSize targetSize = pixSize * scale;

    // 计算居中偏移量
	/*
		如果 targetSize 大于控件尺寸（即超出部分），x 或 y 会为负值。
		例如，targetSize.width() = 225，控件宽度 = 200，
		则 x = (200 - 225)/2 = -12.5（取整后为 -12 或 -13）。
	
		当绘制图片时，起始位置在 (-12, y)，图片的左上角在控件外部，
		右侧会多出 25 像素被裁剪，但左右各溢出 12.5 像素，实现居中裁剪效果。

		如果图片尺寸小于控件尺寸（这种情况在 scale 取最大值后不会发生，
		因为至少有一个方向等于控件尺寸，另一个方向可能略小？
		不，由于我们取的是 qMax，所以至少有一个方向等于控件尺寸，另一个方向大于或等于控件尺寸。
		因此实际上 targetSize 总是 >= 控件尺寸，所以 x 和 y 总是 ≤ 0，
		即图片总是会覆盖整个控件，只是部分区域溢出裁剪）。
	*/
    int x = (rect.width() - targetSize.width()) / 2;
    int y = (rect.height() - targetSize.height()) / 2;
		
	// 构造目标矩形并绘制
	/*
		targetRect 
			定义了图片绘制的目标区域（在控件坐标系中）。

		drawPixmap 
			将 m_pix 缩放并绘制到该矩形区域。
			由于 QPainter 默认会裁剪超出控件区域的部分，
			因此超出的边缘不会显示，从而实现了“裁剪”效果。
	*/
    QRect targetRect(x, y, targetSize.width(), targetSize.height());
    painter.drawPixmap(targetRect, m_pix);
		
	/*
		整体逻辑推演
			控件矩形 rect = (0,0,100,50)
			图片原始尺寸 pixSize = (200,100)
			
		计算缩放比例
			scaleW = 100 / 200 = 0.5
			scaleH = 50 / 100 = 0.5
			scale = qMax(0.5, 0.5) = 0.5
				
		目标绘制尺寸
			targetSize = pixSize × scale = (200×0.5, 100×0.5) = (100, 50)
			此时 targetSize 恰好等于控件尺寸，所以图片刚好填满控件，无裁剪。
				
		如果图片比例为 200×150，控件为 100×50
			scaleW = 100/200 = 0.5
			scaleH = 50/150 ≈ 0.333
			scale = qMax(0.5, 0.333) = 0.5
			targetSize = (200×0.5, 150×0.5) = (100, 75)
				
		计算居中绘制偏移：
			x = (100 - 100) / 2 = 0
			y = (50 - 75) / 2 = -12.5   → 取整为 -12 或 -13
			因此 targetRect 为 (0, -12, 100, 75)。
				
		painter.drawPixmap(targetRect, m_pix);绘制时：
			首先：
			 painter.setRenderHint(QPainter::SmoothPixmapTransform);
			因为开启了锯齿 实现高质量缩放
			图片被缩放到 100×75 的矩形区域。
				
			其次：
			根据上一步计算的居中绘制偏移，将targetSize放到相对于标签坐标系中，
			实现标签正好能框到targetSize的中间区域
			这个矩形的左上角在控件（标签）坐标系中的 y 坐标为 -12，即向上偏移了 12 像素，
			所以图片的顶部 12 像素在控件外部，底部会有 75 - 50 - 12 = 13 像素超出底部
			（因偏移 -12，实际底部 y 为 -12+75=63，超出控件底部 13 像素）。

			QPainter 默认的剪切区域是控件的可见区域（即 rect()），
			所以绘制时 超出 rect 的部分会被自动裁剪掉，只显示控件内部的区域。
	*/
}

// 重写mousePressEvent，自定义鼠标点击事件发生时的行为 对外部发送click信号
void MediaLabel::mousePressEvent(QMouseEvent* event){
	// 鼠标左键发生时，对外部发送click信号，media_elem med 作为参数
	if (event->button() == Qt::LeftButton && m_loaded) {
        emit clicked(med);
    }
}