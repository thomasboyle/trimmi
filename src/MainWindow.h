#pragma once

#include <QMainWindow>
#include <QPoint>

class DropZone;
class EncoderCapabilities;
class Exporter;
class GpuStatusCard;
class QComboBox;
class QLabel;
class QLineEdit;
class QProgressDialog;
class QPushButton;
class QSlider;
class ThumbnailGenerator;
class TimelineWidget;
class TitleBar;
class VideoPlayer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    void loadVideo(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void openFileDialog();
    void onPlayPause();
    void onVolumeChanged(int value);
    void onTrimChanged(qint64 startMs, qint64 endMs);
    void onStartEdited();
    void onEndEdited();
    void onExport();
    void onFullscreen();
    void updateTimeLabels(qint64 positionMs);
    void refreshEncoderUi();
    void stepStart(int direction);
    void stepEnd(int direction);

private:
    void buildUi();
    void applyWindowChrome();
    void setControlsEnabled(bool enabled);
    void syncTrimFields();
    void updateDurationLabel();
    QString selectedEncoderId() const;
    QString selectedFormatId() const;

    TitleBar* m_titleBar = nullptr;
    DropZone* m_dropZone = nullptr;
    GpuStatusCard* m_gpuCard = nullptr;
    QComboBox* m_encoderCombo = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLabel* m_encoderHelper = nullptr;
    QLabel* m_formatHelper = nullptr;
    QLabel* m_gpuBadge = nullptr;

    QLabel* m_fileNameLabel = nullptr;
    QLabel* m_totalDurationLabel = nullptr;
    QLabel* m_timeReadout = nullptr;
    QLabel* m_trimDurationLabel = nullptr;

    QWidget* m_previewHost = nullptr;
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QSlider* m_volumeSlider = nullptr;

    QLineEdit* m_startEdit = nullptr;
    QLineEdit* m_endEdit = nullptr;
    QPushButton* m_startUp = nullptr;
    QPushButton* m_startDown = nullptr;
    QPushButton* m_endUp = nullptr;
    QPushButton* m_endDown = nullptr;

    TimelineWidget* m_timeline = nullptr;
    VideoPlayer* m_player = nullptr;
    EncoderCapabilities* m_caps = nullptr;
    ThumbnailGenerator* m_thumbs = nullptr;
    Exporter* m_exporter = nullptr;
    QProgressDialog* m_progressDialog = nullptr;

    bool m_updatingFields = false;
    bool m_fullscreen = false;
    QWidget* m_normalCentral = nullptr;
};
