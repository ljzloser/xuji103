#ifndef MSG_PARSER_WINDOW_H
#define MSG_PARSER_WINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QList>
#include <QDragEnterEvent>
#include "FrameParser.h"

class MsgParserWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MsgParserWindow(QWidget* parent = nullptr);
    ~MsgParserWindow();

    void loadFile(const QString& filepath);

private slots:
    void openFile();
    void onFrameSelected();
    void onFieldSelected();
    void filterChanged(int index);

protected:
    void setupUI();
    void refreshFrameList();
    bool matchFilter(const FrameInfo& frame);
    
    // 拖放支持
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showFrameDetail(const FrameInfo& frame);
    void addFieldToTree(QTreeWidgetItem* parent, const FieldInfo& field);
    void highlightBytes(int start, int end);

private:
    QList<FrameInfo> m_frames;
    FrameInfo* m_currentFrame;

    // UI组件
    QTreeWidget* m_frameList;
    QTreeWidget* m_fieldTree;
    QTextEdit* m_rawView;
    QLabel* m_statusLabel;
    QComboBox* m_dirFilter;    // 方向筛选
    QComboBox* m_typeFilter;   // 帧类型筛选
    QComboBox* m_asduFilter;   // ASDU类型筛选
};

#endif // MSG_PARSER_WINDOW_H
