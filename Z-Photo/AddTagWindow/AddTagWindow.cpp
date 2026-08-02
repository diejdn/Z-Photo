#include "AddTagWindow.h"
#include "sqlite_api.h"      // 因为使用了 SqliteApi 的成员函数吗
#include "schema.h"          // 因为使用了 tag_elem
#include <QTimer>
#include <QMessageBox>
#include <QVBoxLayout>       // 构造函数中创建布局
#include <QFile>

AddTagWindow::AddTagWindow(SqliteApi* sqlite, int media_id, QWidget* parent)
    : QWidget(parent), m_sqlite(sqlite)
{
	setObjectName("AddTagWindow");
	
    // 首先设置m_image_id，再判断模式
    set_media_id(media_id);
    Mode();

    bool tag_manage = (mode == -2);
    QString title, actionBtntext;
    if (mode == -1) {
        title = "根据标签筛选文件";
        actionBtntext = "筛选";
    } else if (mode == -2) {
        title = "标签管理";
        actionBtntext = "添加标签";
    } else {
        title = QString("为文件添加标签 (id: %1)").arg(m_media_id);
        actionBtntext = "添加";
    }

    setWindowTitle(title);
	// 固定页面大小
    // setFixedSize(300, 400);
	// 可拖动大小
	resize(300, 400);
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // ----- 输入框 -----
    m_lineEdit = new QLineEdit(this);
    if (!tag_manage) {
        m_lineEdit->setReadOnly(true);
    }
    m_lineEdit->setPlaceholderText("请点击选择标签");
    m_lineEdit->setFixedHeight(32);
    layout->addWidget(m_lineEdit);

    // ----- 标签列表 -----
    m_tagList = new QListWidget(this);
    if (!tag_manage) {
        m_tagList->setSelectionMode(QAbstractItemView::MultiSelection);
    }
    m_tagList->setFlow(QListView::LeftToRight);
    m_tagList->setWrapping(true);
    m_tagList->setGridSize(QSize(80, 40));
    m_tagList->setSpacing(15);
    m_tagList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    layout->addWidget(m_tagList);

    // ----- 按钮 -----
    m_actionBtn = new QPushButton(actionBtntext, this);
    m_actionBtn->setObjectName("actionBtn");   // 设置对象名，匹配样式表
    m_actionBtn->setFixedHeight(34);

    m_deleteTag_Btn = new QPushButton("删除标签", this);
    m_deleteTag_Btn->setObjectName("deleteBtn");
    m_deleteTag_Btn->setVisible(tag_manage);
    m_deleteTag_Btn->setFixedHeight(34);

    layout->addWidget(m_actionBtn);
    layout->addWidget(m_deleteTag_Btn);

    // ----- 信号连接 -----
    if (tag_manage) {
        connect(m_tagList, &QListWidget::itemClicked, this, &AddTagWindow::onTagItemClicked);
    }
    connect(m_actionBtn, &QPushButton::clicked, this, &AddTagWindow::onActionButtonClicked);
    connect(m_deleteTag_Btn, &QPushButton::clicked, this, &AddTagWindow::onDeleteTagClicked);

    // ----- 加载样式表（从外部文件）-----
    // qrc中定义样式位置
    QFile styleFile(":/style/AddTagWindowstyle.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString style = styleFile.readAll();
        this->setStyleSheet(style);
    } else {
        // 如果文件不存在，使用备用基础样式（可选）
        qDebug() << "Warning: Could not load style.qss, using default.";
    }

    // 加载标签数据
    loadTags();
}

int AddTagWindow::Mode(){ 
	mode = m_media_id; 
	return mode;
}

// 设置m_image_id 用于切换图片
void AddTagWindow::set_media_id(int media_id){
	m_media_id = media_id;
	
	QString title = QString("为文件添加标签 (id: %1)").arg(m_media_id);

    setWindowTitle(title);
}


void AddTagWindow::loadTags()
{
    if (!m_sqlite) {
        QMessageBox::warning(this, "错误", "数据库未连接");
        return;
    }
	
	// 首先清空m_tagList
	m_tagList->clear();

    std::vector<tag_elem> tags;   // 假设 schema.h 中定义了 tag_elem
    if (!m_sqlite->get_all_tag(tags, true, false)) { // 按名称升序
        QMessageBox::warning(this, "错误", 
            "加载标签失败: " + QString::fromStdString(m_sqlite->getError()));
        return;
    }

    for (const auto& tag : tags) {
        QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(tag.tag_name));
        item->setData(Qt::UserRole, tag.tag_id);
        m_tagList->addItem(item);
    }
}

// 标签管理模式有作用 点击标签时在m_lineEdit输出标签信息
void AddTagWindow::onTagItemClicked(QListWidgetItem* item)
{	   
	int tag_id = item->data(Qt::UserRole).toInt();
	QString tag_name = item->text().trimmed();
	// 根据标签id查询对应的图片数量
	int bindImage = m_sqlite->count_media_by_tag(tag_id);
	
	QString tag_info = QString(R"(标签id：%1，标签名称：%2，标签绑定图片数量：%3)").arg(tag_id).arg(tag_name).arg(bindImage);
    
	// m_lineEdit中显示信息
	m_lineEdit->setText(tag_info);
}

void AddTagWindow::onActionButtonClicked()
{
	if(mode != -2){
		int select_num = m_tagList->selectedItems().size();
		
		if (select_num == 0) {
			m_lineEdit->setText("请先选择一个标签");
			return;
		}
		
		// 再次清空m_selectedTagIds
		m_selectedTagIds.clear();
		// 将被选择的存入标签QListWidgetItem的tag_id加入m_selectedTagIds中
		for (QListWidgetItem* item : m_tagList->selectedItems()){
			int tag_id = item->data(Qt::UserRole).toInt();
			// QString tagName = item->text();获得文本内容
			m_selectedTagIds.push_back(tag_id);
		}

		// 查询模式
		if (mode == -1) {
			// 查询模式：发射信号以及参数，关闭窗口
			qDebug() << "AddTagWindow::onActionButtonClicked querry mode, tag_ids num: " << m_selectedTagIds.size();
			emit tagSelected(m_selectedTagIds);
			close();
		} 
		// 为图片添加标签模式
		else {
			// 添加模式：执行插入操作
			if (!m_sqlite) {
				m_lineEdit->setText("数据库未连接");
				return;
			}

			// bool add_tag_to_media(int media_id, std::vector<int>& tag_ids)
			if (m_sqlite->add_tag_to_media(m_media_id, m_selectedTagIds)) {
				m_lineEdit->setText("添加成功!");
				m_tagList->clearSelection();
				
				// 添加玩后就清空vector m_selectedTagIds
				m_selectedTagIds.clear();
				// 延迟重置提示文字
				/*
				QTimer::singleShot(800, this, [this]() {
					m_lineEdit->clear();
					m_lineEdit->setPlaceholderText("请点击选择标签");
				});
				*/
			} else {
				m_lineEdit->setText("添加失败: " + QString::fromStdString(m_sqlite->getError()));
				// 添加失败也要清空
				m_tagList->clearSelection();
				m_selectedTagIds.clear();
			}
		}
	}
	// 标签管理模式，添加标签按钮
	else if(mode == -2){
		// bool add_tag(const std::string& tag_name);
		QString tagQStr = m_lineEdit->text().trimmed();
		std::string std_tag_name = tagQStr.toStdString();
		
		// 判断是否为空
		if(std_tag_name.empty()){
			m_lineEdit->setText("错误：标签名称不能为空！");
			return;
		}
		
		bool res = m_sqlite->add_tag(std_tag_name);
		if(res){
			m_lineEdit->setText(QString("标签「%1」添加成功，重复标签自动忽略").arg(tagQStr));
			//m_lineEdit->clear(); // 添加成功清空输入框
			m_tagList->clearSelection();
			// 重新加载tagList
			loadTags();
		}
		else
		{
			// 取出数据库错误
			QString err = QString::fromStdString(m_sqlite->getError());
			m_lineEdit->setText(QString("添加标签失败：%1").arg(err));
		}
	}
}

void AddTagWindow::onDeleteTagClicked(){
	// bool delete_tag(int tag_id);
    // 获取选中的项列表（管理模式下为单选）
    QList<QListWidgetItem*> selected = m_tagList->selectedItems();
    if (selected.isEmpty()) {
        m_lineEdit->setText("请先选择一个标签");
        return;
    }
	
	// 取列表中第一个元素
	QListWidgetItem* item = selected.first();
	
	int delete_tag_id = item->data(Qt::UserRole).toInt();
	QString delete_tag_name = item->text().trimmed();
	
	bool res = m_sqlite->delete_tag(delete_tag_id);
	if(res){
		m_lineEdit->setText(QString("标签「%1」「%2」删除成功").arg(delete_tag_id).arg(delete_tag_name));
		m_tagList->clearSelection();
		// 重新加载tagList
		loadTags();
	}
	else
	{
		// 取出数据库错误
		QString err = QString::fromStdString(m_sqlite->getError());
		m_lineEdit->setText(QString("删除标签失败：%1").arg(err));
	}
}

// 重写KeyEvent键盘事件，实现按m隐藏AddTagWindow
void AddTagWindow::keyPressEvent(QKeyEvent* event){
	
	if(event->key() == Qt::Key_M){
		hide();
	}
	else{
		QWidget::keyPressEvent(event);
	}
}