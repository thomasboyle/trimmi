#pragma once

#include <QFrame>

class QLabel;
class QPushButton;
class QDragEnterEvent;
class QDropEvent;

class DropZone : public QFrame {
    Q_OBJECT
public:
    explicit DropZone(QWidget* parent = nullptr);

signals:
    void fileDropped(const QString& path);
    void selectFileClicked();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    bool isVideoFile(const QString& path) const;

    bool m_dragActive = false;
};
