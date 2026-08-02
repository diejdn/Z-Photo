#ifndef ADDTAGWINDOW_H
#define ADDTAGWINDOW_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QKeyEvent>

// 前向声明，避免在头文件中包含 sqlite_api.h
class SqliteApi;

/*
	// 1. 添加标签（需要图片 ID）
	AddTagWindow* addWin = new AddTagWindow(&sqlite, media_id, this);
	addWin->show();

	// 2. 选择标签（用于查询，media_id = -1）
	AddTagWindow* queryWin = new AddTagWindow(&sqlite, -1, this);
	connect(queryWin, &AddTagWindow::tagSelected, this, [this](int tag_id) {
		// 执行按标签检索
		qDebug() << "Selected tag ID:" << tag_id;
		// 例如：调用 sqlite.query_images_by_tag(tag_id, result);
	});
	queryWin->show();
*/

class AddTagWindow : public QWidget
{
    Q_OBJECT
public:
    // 构造函数：media_id == -1 时为查询模式，否则为添加模式
    explicit AddTagWindow(SqliteApi* sqlite, int media_id = -1, QWidget* parent = nullptr);
	void set_media_id(int media_id = -1);
	
signals:
    // 查询模式下，点击“选择”按钮时发射此信号
	/*
		重载信号，在connect中这样写
		QOverload<std::vector<int>>::of(&AddTagWindow::tagSelected)
	*/
    void tagSelected(int tag_id);
	void tagSelected(std::vector<int> tag_ids);
	
protected:
	// 重写KeyEvent键盘事件，实现按h隐藏AddTagWindow
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onTagItemClicked(QListWidgetItem* item);
    void onActionButtonClicked();
	void onDeleteTagClicked();

private:
    void loadTags();
	// -1按标签筛选，-2标签管理，>0 为图片插入标签
    int Mode();

    SqliteApi* m_sqlite;
	// 要添加tag的图片
	int m_media_id;
	int mode;
    QListWidget* m_tagList;
    QLineEdit* m_lineEdit;
    QPushButton* m_actionBtn;
	QPushButton* m_deleteTag_Btn;
	
public:
	// 选择的多个tag_id
    std::vector<int> m_selectedTagIds;
};

#endif // ADDTAGWINDOW_H