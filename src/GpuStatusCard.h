#pragma once

#include <QFrame>

#include "EncoderCapabilities.h"

class QLabel;

class GpuStatusCard : public QFrame {
    Q_OBJECT
public:
    explicit GpuStatusCard(QWidget* parent = nullptr);

public slots:
    void setGpuInfo(const GpuInfo& info);

private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
};
