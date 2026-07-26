#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QMouseEvent;

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

public slots:
    void setMaximized(bool maximized);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_maximized = false;
};
