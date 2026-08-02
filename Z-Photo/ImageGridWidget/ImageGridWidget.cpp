#include "ImageGridWidget.h"
#include "MediaLabel.h"
#include "ZLens.h"
#include "AddTagWindow.h"
#include "sqlite_api.h"
#include <QGridLayout>

// 构造函数
ImageGridWidget::ImageGridWidget(int columns, QWidget* parent)
    : QWidget(parent), m_columns(columns)
{
	// 为ImageGridWidget添加网格布局
    m_layout = new QGridLayout(this);
	
	// 图片之间间距
    m_layout->setSpacing(5);
	// 内边距
	// m_layout->setContentsMargins(8, 8, 8, 8);
    
	// 设置列拉伸 让所有列等比例拉伸，铺满滚动区域宽度
    for (int i = 0; i < m_columns; ++i) {
        m_layout->setColumnStretch(i, 1);
    }
}

// 设置图片路径列表，并重新构建网格rebuildGrid
/*
	声明为 const std::vector<image_elem>&，表示它们是对主函数中 imgVector 对象的只读引用。
	
	medias 是成员变量是 vector对象，与传入的参数完全分离。
    medias.clear(); 清空的是成员变量自身，不会影响主函数中的 paths 对象。
	
	this->medias = medias; 是拷贝赋值
	
	即this->medias并不是mediaVector的引用，两者在不同内存
	
	注意medias就是结果元信息列表，更换目录时需要手动修改images（主窗口中管理该对象）
*/
void ImageGridWidget::setMediaPaths(const std::vector<media_elem>& medias){
	this->medias.clear();
	
	this->medias = medias;
	
	// 清空现有的预览窗口
	/*
	if (g_zlens) {
		g_zlens->close();
		g_zlens->deleteLater();
		g_zlens = nullptr;
	}*/
	
	// 切换目录时，只标记zlens_not_close为真，用于之后 快捷键 切换media时 加载新目录的第一个媒体
	if (g_zlens) {
		zlens_not_close = true;
	}
	
	rebuildGrid();
}

// 转入数据库连接句柄
void ImageGridWidget::setSqliteApi(SqliteApi* sqlite) {
    m_sqlite = sqlite;
}

// 清空现有的布局项以及QList<MediaLabel*>m_labels
void ImageGridWidget::clear()
{
    // 删除所有标签（布局会自动处理子控件删除，释放内存）
    // 从布局中移除所有项，并安全删除控件
	/*
		takeAt(0) —— 从布局中取走第一个项
		作用：
			将布局中的所有子项“剥离”出来，布局本身变空，
			后续再添加新控件时不会与旧控件冲突。
			
			m_layout 是一个 QGridLayout（继承自 QLayout）。
			takeAt(int index) 会移除并返回布局中指定索引处的 
			QLayoutItem*，且不再管理该项（布局不再拥有它）。
			
			这里用 while 循环反复取第 0 
			个，直到全部取完。每取一个，布局中的项就少一个，所以索引始终为 0 
			总是取到剩余的第一个。
			
		item->widget() —— 获取控件指针
		作用：
			拿到正在处理的子控件对象。
			
			QLayoutItem 可以代表一个控件（QWidget*）、子布局或空白空间。
			调用 item->widget() 如果该项是一个控件，则返回该控件指针；
			否则返回 nullptr。
			由于只往网格中添加了 MediaLabel（继承自 QWidget），
			所以这里 item->widget() 一定会返回有效的 QWidget*。
			
		widget->setParent(nullptr) —— 解除父子关系（关键！）
			当一个控件被添加到布局时，布局所属的 QWidget
			（即 ImageGridWidget）会自动成为该控件的父对象
			（因为 addWidget 内部调用了 widget->setParent(this)）。

			父对象在销毁时会自动 delete 所有子控件，这是 Qt 对象树机制。

			如果我们直接 delete widget，而父对象仍认为它是自己的子对象，
			那么当 ImageGridWidget 析构时，会再次尝试 delete 
			这个已经销毁的指针，导致双重删除（Double Delete），程序崩溃。
			
			通过 setParent(nullptr)，明确告诉Qt：
				这个控件不再属于父对象。
				父对象的子对象列表中会移除该控件，这样父对象在构时就不会再碰它。
				
		delete widget —— 手动释放控件内存
		作用：
			释放控件内存，避免内存泄漏。
			
		    现在控件已与父对象脱离关系，可以安全地 delete 它。
			它会调用析构函数，释放其占用的资源（包括其子控件，如果有的话）。
			
		delete item —— 释放布局项对象
		作用：
			清理布局项对象，避免内存泄漏。
			
			QLayoutItem* 本身也是一个对象（例如 QWidgetItem），它持有对控件的引用。
			已经取走了控件并删除了，但布局项对象还在，占用内存。
			因此需要 delete item 释放布局项自身。

		m_labels.clear() —— 清空内部容器
		作用：
			防止悬垂指针误用。
			
			m_labels 是 QList<MediaLabel*>，存储了所有 MediaLabel 指针。

			删除了所有控件对象，列表中的指针现在都是悬垂指针（指向已释放的内存）
			如果不清理，后续访问或析构时再次遍历就会崩溃。

			clear() 清空列表，丢弃所有元素（MediaLabel* 指针值）
	*/
	
    while (QLayoutItem* item = m_layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);   // 关键：解除父子关系
            delete widget;                // 此时安全删除
        }
        delete item;                      // 删除布局项本身
    }
	
	// 清空QList
	/*
		关于连接在lab上的connect
		
		QList<MediaLabel*> m_labels 保存所有标签的指针
		m_labels.clear() 清空其元素（指针类型）
		
		由于 clear() 已删除所有 MediaLabel，
		Qt 会自动断开它们的信号槽连接，因此不需要手动管理connect, 当某组件销毁时，其连接也会被qt销毁
	*/
    m_labels.clear();
}

// 重建网格（清空并重新添加）自动构建QList<MediaLabel*> + 网格，
void ImageGridWidget::rebuildGrid()
{
	// 清空现有的 布局项 以及 QList<MediaLabel*>m_labels
    clear(); 

    int idx = 0;
	for(int i = 0; i < medias.size(); i++)
    {
		// 得到单项图片的元信息
		media_elem med = medias[i];
		
        MediaLabel* lab = new MediaLabel(med);
        int r = idx / m_columns;
        int c = idx % m_columns;
		/*
			布局添加控件时：layout->addWidget(widget) 内部会调用 
			widget->setParent(this)，
			
			m_layout->addWidget(lab, r, c);
			因此 ImageLabel 的父对象变为 ImageGridWidget。
		*/
        m_layout->addWidget(lab, r, c);
        m_labels.append(lab);
        idx++;
		
		// MediaLabel* lab 被点击时会发送带有 media_elem& media 参数的 clicked信号，这里捕获信号并处理
		// 当被lab被点击时，再由 ImageGridWidget 向上发送信号，实现展示媒体信息
		// 并且初始化ZLens g_zlens（内置的 媒体预览器），实现播放功能
		QMetaObject::Connection conn = QObject::connect(lab, &MediaLabel::clicked, this, [this, i](const media_elem& media) {
			// 标签被点击，则记录索引，即在 medias 中的下标
			m_currentIndex = i;
			
			// 点击图片时 发送信号，请求在主界面显示被点击的图片的详细信息
			emit show_clicked_media_info(media);
			
			zlens_not_close = false;
			
			if (!g_zlens) {
				// g_previewWindow为空值则重建
				g_zlens = new ZLens();
				
				// 处理g_zlens发的带有参数int step(正负1) 的next信号 捕获this
				bool changeImg = QObject::connect(g_zlens, &ZLens::nextMedia, this, [this](int step){
					
					// 边界保险检查，目录为空的情况基本不可能发生。防止不销毁zlens直接切换目录后，新目录为空的情况。
					if(medias.size() == 0){
						return;
					}
					
					// 接受信号，只有上一张 L Left 下一张 R Right两种情况
					QString L_or_R = (step == -1) ? "Left" : "Right";
					qDebug() << L_or_R;
					
					int newIndex;
					// 当存在close_zlens标记时，表示切换目录，且上一个目录的 g_lens没有关闭
					if(zlens_not_close){
						newIndex = 0;
						
						zlens_not_close = false;
					}
					else{
						newIndex = m_currentIndex + step;
					}
					
					
					// 判断
					if (newIndex < 0) {
						newIndex = 0;   // 停在首张
						// 或循环： newIndex = m_data_paths.size() - 1;
					} else if (newIndex >= medias.size()) {
						newIndex = medias.size() - 1;   // 停在末张
						// 或循环： newIndex = 0;
					}
					
					m_currentIndex = newIndex;
					qDebug() << "m_currentIndex: " << m_currentIndex;
					if (g_zlens) {
						g_zlens->showMedia(medias[m_currentIndex]);
						// 切换图片时 发送信号，请求在主界面显示被点击的图片的详细信息
						emit show_clicked_media_info(medias[m_currentIndex]);
						
						// 如果g_AddTagWindow，则设置其media_id
						if(g_AddTagWindow){
							g_AddTagWindow->set_media_id(medias[m_currentIndex].media_id);
							qDebug() << "change g_zlens so g_AddTagWindow set_media_id: " << medias[m_currentIndex].media_id;
						}
					}
					
					// 接受事件，不再向上传递 该事件已在 ZLens 中被发射，不需要在槽中 accept，可以去掉。
					// event->accept();
				});
				
				// 连接g_zlens带有参数int media_id的 addTag 信号，按T后，图片/视频播放器 发送自身media_id，为AddTagWindow提供media_id
				QObject::connect(g_zlens, &ZLens::addTag, this, [this](int media_id) {
					qDebug() << "ZLens signal addTag media_id" << media_id;
					if (!m_sqlite) return;
					if(!g_AddTagWindow){
						g_AddTagWindow = new AddTagWindow(m_sqlite, media_id);
						qDebug() << "g_AddTagWindow created";
					}
					else{
						g_AddTagWindow->set_media_id(media_id);
						qDebug() << "click T g_AddTagWindow set_media_id_id: " << media_id;
					}
	
					g_AddTagWindow->show();
					g_AddTagWindow->raise();
					g_AddTagWindow->activateWindow();
				});
				
				g_zlens->showMedia(media);
				qDebug() << "ImageGridWidget::rebuildGrid g_zlens created and show";
			}
			else {
				g_zlens->showMedia(media);
			}
			qDebug() << "clicked: " << QString::fromStdString(media.file_path);
		});
    }
	
	/*
		m_layout->addWidget后
		 
		activate() 强制 QGridLayout 立即计算所有子控件的几何位置，使 geometry() 返回正确值。
		
		用于updateVisibleImages可视区缓冲区计算标签位置 使 geometry() 返回正确值。
	*/
	m_layout->activate();
}

void ImageGridWidget::setColumns(int columns)
{
    if (m_columns == columns) return;
    m_columns = columns;
    // 重建列拉伸
    for (int i = 0; i < m_columns; ++i) {
        m_layout->setColumnStretch(i, 1);
    }
    // 如果已有数据，重新布局
    if (medias.size() != 0) {
        rebuildGrid();
    }
}