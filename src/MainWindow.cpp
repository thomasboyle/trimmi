#include "MainWindow.h"

#include "DropZone.h"
#include "EncoderCapabilities.h"
#include "Exporter.h"
#include "GpuStatusCard.h"
#include "Theme.h"
#include "ThumbnailGenerator.h"
#include "TimelineWidget.h"
#include "TimeFormat.h"
#include "TitleBar.h"
#include "VideoPlayer.h"

#include <QApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {

class GrainOverlay : public QWidget {
public:
    explicit GrainOverlay(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setObjectName(QStringLiteral("grainOverlay"));
        m_grain = QPixmap(QStringLiteral(":/ui/texture-grain.png"));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_grain.isNull())
            return;
        QPainter p(this);
        p.setOpacity(0.30);
        p.drawTiledPixmap(rect(), m_grain);
    }

private:
    QPixmap m_grain;
};

QPushButton* makeIconButton(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setObjectName(QStringLiteral("iconButton"));
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(36, 36);
    return btn;
}

QWidget* makeStepperField(QLineEdit*& edit, QPushButton*& up, QPushButton*& down, QWidget* parent)
{
    edit = new QLineEdit(parent);
    edit->setText(QStringLiteral("00:00:00.000"));
    edit->setAlignment(Qt::AlignCenter);
    edit->setFixedWidth(120);

    up = new QPushButton(QStringLiteral("▴"), parent);
    down = new QPushButton(QStringLiteral("▾"), parent);
    for (auto* b : {up, down}) {
        b->setObjectName(QStringLiteral("iconButton"));
        b->setFixedSize(22, 18);
        b->setFocusPolicy(Qt::NoFocus);
        b->setCursor(Qt::PointingHandCursor);
    }

    auto* steppers = new QVBoxLayout;
    steppers->setSpacing(0);
    steppers->setContentsMargins(0, 0, 0, 0);
    steppers->addWidget(up);
    steppers->addWidget(down);

    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(edit);
    layout->addLayout(steppers);
    return row;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Trimmi"));
    resize(1280, 820);
    setMinimumSize(1024, 680);

    m_player = new VideoPlayer(this);
    m_caps = new EncoderCapabilities(this);
    m_thumbs = new ThumbnailGenerator(this);
    m_exporter = new Exporter(this);

    applyWindowChrome();
    buildUi();
    setControlsEnabled(false);

    connect(m_caps, &EncoderCapabilities::detectionFinished, this, &MainWindow::refreshEncoderUi);
    m_caps->detect();

    connect(m_player, &VideoPlayer::loaded, this, [this](const VideoMetadata& meta) {
        m_fileNameLabel->setText(meta.fileName);
        m_totalDurationLabel->setText(TimeFormat::formatMs(meta.durationMs));
        m_timeline->setDuration(meta.durationMs);
        m_timeline->setThumbnails({});
        setControlsEnabled(true);
        syncTrimFields();
        updateTimeLabels(0);
        updateDurationLabel();

        const int count = qBound(8, m_timeline->width() / 90, 24);
        m_thumbs->generate(meta.path, meta.durationMs, count);
    });

    connect(m_player, &VideoPlayer::loadFailed, this, [this](const QString& msg) {
        setControlsEnabled(false);
        QMessageBox::warning(this, QStringLiteral("Trimmi"), msg);
    });

    connect(m_player, &VideoPlayer::positionChanged, this, [this](qint64 pos) {
        m_timeline->setPosition(pos);
        updateTimeLabels(pos);
    });

    connect(m_player, &VideoPlayer::playingChanged, this, [this](bool playing) {
        m_playBtn->setText(playing ? QStringLiteral("⏸") : QStringLiteral("▶"));
    });

    connect(m_thumbs, &ThumbnailGenerator::thumbnailReady, this,
            [this](int index, const QImage& image) {
                m_timeline->setThumbnail(index, image);
            });

    connect(m_exporter, &Exporter::progress, this, [this](double pct, const QString& status) {
        if (!m_progressDialog)
            return;
        m_progressDialog->setValue(static_cast<int>(pct));
        m_progressDialog->setLabelText(status);
    });

    connect(m_exporter, &Exporter::finished, this, [this](bool ok, const QString& message) {
        if (m_progressDialog) {
            m_progressDialog->hide();
            m_progressDialog->deleteLater();
            m_progressDialog = nullptr;
        }
        m_exportBtn->setEnabled(true);
        if (ok)
            QMessageBox::information(this, QStringLiteral("Trimmi"), message);
        else
            QMessageBox::critical(this, QStringLiteral("Trimmi"), message);
    });
}

MainWindow::~MainWindow()
{
    if (m_thumbs)
        m_thumbs->cancel();
    if (m_exporter)
        m_exporter->cancel();
}

void MainWindow::applyWindowChrome()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
}

void MainWindow::buildUi()
{
    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_titleBar = new TitleBar(root);
    connect(m_titleBar, &TitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeClicked, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(m_titleBar, &TitleBar::closeClicked, this, &QWidget::close);

    auto* body = new QWidget(root);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // ---- Sidebar ----
    auto* sidebar = new QFrame(body);
    sidebar->setObjectName(QStringLiteral("sidebarPanel"));
    sidebar->setFixedWidth(340);
    auto* sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 14, 14, 14);
    sideLayout->setSpacing(12);

    auto* selectTitle = new QLabel(QStringLiteral("1. Select Video"), sidebar);
    selectTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_dropZone = new DropZone(sidebar);
    auto* supports = new QLabel(QStringLiteral("Supports: MP4, MOV, MKV, WebM, AVI, etc."), sidebar);
    supports->setObjectName(QStringLiteral("helperText"));
    supports->setWordWrap(true);

    connect(m_dropZone, &DropZone::selectFileClicked, this, &MainWindow::openFileDialog);
    connect(m_dropZone, &DropZone::fileDropped, this, &MainWindow::loadVideo);

    auto* outputTitle = new QLabel(QStringLiteral("2. Output Settings"), sidebar);
    outputTitle->setObjectName(QStringLiteral("sectionTitle"));

    auto* encLabel = new QLabel(QStringLiteral("Encoder"), sidebar);
    encLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_encoderCombo = new QComboBox(sidebar);
    m_encoderHelper =
        new QLabel(QStringLiteral("Will fallback to CPU (AV1) if GPU is not available."), sidebar);
    m_encoderHelper->setObjectName(QStringLiteral("helperText"));
    m_encoderHelper->setWordWrap(true);

    // Soft sage GPU badge when a GPU encoder is selected
    auto* encRow = new QWidget(sidebar);
    auto* encRowLayout = new QHBoxLayout(encRow);
    encRowLayout->setContentsMargins(0, 0, 0, 0);
    encRowLayout->setSpacing(8);
    m_gpuBadge = new QLabel(QStringLiteral("GPU"), encRow);
    m_gpuBadge->setObjectName(QStringLiteral("gpuBadge"));
    m_gpuBadge->setStyleSheet(QStringLiteral(
        "QLabel#gpuBadge { background-color: #E4E8D4; color: #5F6B45; border: 1px solid #6B744F;"
        " border-radius: 4px; padding: 2px 7px; font-size: 11px; font-weight: 700; }"));
    m_gpuBadge->setVisible(false);
    encRowLayout->addWidget(m_encoderCombo, 1);
    encRowLayout->addWidget(m_gpuBadge, 0, Qt::AlignVCenter);

    connect(m_encoderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto enc = m_caps->encoderById(selectedEncoderId());
        m_gpuBadge->setVisible(enc.isGpu);
    });

    auto* fmtLabel = new QLabel(QStringLiteral("Format"), sidebar);
    fmtLabel->setObjectName(QStringLiteral("fieldLabel"));
    m_formatCombo = new QComboBox(sidebar);
    m_formatHelper = new QLabel(sidebar);
    m_formatHelper->setObjectName(QStringLiteral("helperText"));
    m_formatHelper->setWordWrap(true);

    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        const auto fmt = m_caps->formatById(selectedFormatId());
        m_formatHelper->setText(fmt.helperText);
    });

    m_gpuCard = new GpuStatusCard(sidebar);

    auto* encBlock = new QVBoxLayout;
    encBlock->setContentsMargins(0, 0, 0, 0);
    encBlock->setSpacing(4);
    encBlock->addWidget(encLabel);
    encBlock->addWidget(encRow);

    auto* fmtBlock = new QVBoxLayout;
    fmtBlock->setContentsMargins(0, 0, 0, 0);
    fmtBlock->setSpacing(4);
    fmtBlock->addWidget(fmtLabel);
    fmtBlock->addWidget(m_formatCombo);

    sideLayout->addWidget(selectTitle);
    sideLayout->addWidget(m_dropZone);
    sideLayout->addWidget(supports);
    sideLayout->addSpacing(8);
    sideLayout->addWidget(outputTitle);
    sideLayout->addLayout(encBlock);
    sideLayout->addWidget(m_encoderHelper);
    sideLayout->addSpacing(4);
    sideLayout->addLayout(fmtBlock);
    sideLayout->addWidget(m_formatHelper);
    sideLayout->addStretch(1);
    sideLayout->addWidget(m_gpuCard);

    // ---- Main panel ----
    auto* mainPanel = new QFrame(body);
    mainPanel->setObjectName(QStringLiteral("mainPanel"));
    auto* mainLayout = new QVBoxLayout(mainPanel);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    auto* topBar = new QFrame(mainPanel);
    topBar->setObjectName(QStringLiteral("topBar"));
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 10);
    m_fileNameLabel = new QLabel(QStringLiteral("No video loaded"), topBar);
    m_fileNameLabel->setObjectName(QStringLiteral("fileNameLabel"));
    m_totalDurationLabel = new QLabel(QStringLiteral("00:00:00.000"), topBar);
    m_totalDurationLabel->setObjectName(QStringLiteral("durationLabel"));
    topLayout->addWidget(m_fileNameLabel);
    topLayout->addStretch(1);
    topLayout->addWidget(m_totalDurationLabel);

    m_previewHost = new QWidget(mainPanel);
    m_previewHost->setMinimumHeight(320);
    m_previewHost->setStyleSheet(
        QStringLiteral("background-color: #2A2924; border: 1px solid #6B744F; border-radius: 12px;"));
    auto* previewLayout = new QVBoxLayout(m_previewHost);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(m_player->videoWidget());

    // Transport controls
    auto* controls = new QWidget(mainPanel);
    auto* controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(6);

    m_playBtn = makeIconButton(QStringLiteral("▶"), controls);
    auto* prevBtn = makeIconButton(QStringLiteral("⏮"), controls);
    auto* nextBtn = makeIconButton(QStringLiteral("⏭"), controls);
    auto* endBtn = makeIconButton(QStringLiteral("⏭|"), controls);
    endBtn->setText(QStringLiteral("⏭"));
    endBtn->setToolTip(QStringLiteral("Jump to end"));

    auto* volIcon = new QLabel(QStringLiteral("🔊"), controls);
    volIcon->setFixedWidth(22);
    m_volumeSlider = new QSlider(Qt::Horizontal, controls);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(110);

    m_timeReadout = new QLabel(QStringLiteral("00:00:00.000 / 00:00:00.000"), controls);
    m_timeReadout->setObjectName(QStringLiteral("durationLabel"));
    m_timeReadout->setMinimumWidth(210);

    auto* fullBtn = makeIconButton(QStringLiteral("⛶"), controls);
    fullBtn->setToolTip(QStringLiteral("Fullscreen"));

    controlsLayout->addWidget(m_playBtn);
    controlsLayout->addWidget(prevBtn);
    controlsLayout->addWidget(nextBtn);
    controlsLayout->addWidget(endBtn);
    controlsLayout->addSpacing(10);
    controlsLayout->addWidget(volIcon);
    controlsLayout->addWidget(m_volumeSlider);
    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(m_timeReadout);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(fullBtn);

    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(prevBtn, &QPushButton::clicked, this, [this]() { m_player->stepFrame(-1); });
    connect(nextBtn, &QPushButton::clicked, this, [this]() { m_player->stepFrame(1); });
    connect(endBtn, &QPushButton::clicked, this, [this]() { m_player->jumpToEnd(); });
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    connect(fullBtn, &QPushButton::clicked, this, &MainWindow::onFullscreen);

    m_timeline = new TimelineWidget(mainPanel);
    connect(m_timeline, &TimelineWidget::trimChanged, this, &MainWindow::onTrimChanged);
    connect(m_timeline, &TimelineWidget::seekRequested, this, [this](qint64 ms) {
        m_player->seek(ms);
    });

    // Trim fields row
    auto* trimRow = new QWidget(mainPanel);
    auto* trimLayout = new QHBoxLayout(trimRow);
    trimLayout->setContentsMargins(0, 4, 0, 0);

    auto* startLabel = new QLabel(QStringLiteral("Start"), trimRow);
    startLabel->setObjectName(QStringLiteral("fieldLabel"));
    auto* startField = makeStepperField(m_startEdit, m_startUp, m_startDown, trimRow);

    m_trimDurationLabel = new QLabel(QStringLiteral("Duration  00:00:00.000"), trimRow);
    m_trimDurationLabel->setAlignment(Qt::AlignCenter);
    m_trimDurationLabel->setObjectName(QStringLiteral("durationLabel"));

    auto* endLabel = new QLabel(QStringLiteral("End"), trimRow);
    endLabel->setObjectName(QStringLiteral("fieldLabel"));
    auto* endField = makeStepperField(m_endEdit, m_endUp, m_endDown, trimRow);

    trimLayout->addWidget(startLabel);
    trimLayout->addWidget(startField);
    trimLayout->addStretch(1);
    trimLayout->addWidget(m_trimDurationLabel);
    trimLayout->addStretch(1);
    trimLayout->addWidget(endLabel);
    trimLayout->addWidget(endField);

    connect(m_startEdit, &QLineEdit::editingFinished, this, &MainWindow::onStartEdited);
    connect(m_endEdit, &QLineEdit::editingFinished, this, &MainWindow::onEndEdited);
    connect(m_startUp, &QPushButton::clicked, this, [this]() { stepStart(1); });
    connect(m_startDown, &QPushButton::clicked, this, [this]() { stepStart(-1); });
    connect(m_endUp, &QPushButton::clicked, this, [this]() { stepEnd(1); });
    connect(m_endDown, &QPushButton::clicked, this, [this]() { stepEnd(-1); });

    auto* bottomRow = new QHBoxLayout;
    bottomRow->addStretch(1);
    m_exportBtn = new QPushButton(QStringLiteral("✂  Trim & Export"), mainPanel);
    m_exportBtn->setObjectName(QStringLiteral("exportButton"));
    m_exportBtn->setCursor(Qt::PointingHandCursor);
    m_exportBtn->setMinimumWidth(180);
    connect(m_exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);
    bottomRow->addWidget(m_exportBtn);

    mainLayout->addWidget(topBar);
    mainLayout->addWidget(m_previewHost, 1);
    mainLayout->addWidget(controls);
    mainLayout->addWidget(m_timeline);
    mainLayout->addWidget(trimRow);
    mainLayout->addLayout(bottomRow);

    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->setSpacing(10);
    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(mainPanel, 1);

    rootLayout->addWidget(m_titleBar);
    rootLayout->addWidget(body, 1);
    setCentralWidget(root);
    m_normalCentral = root;

    auto* grain = new GrainOverlay(root);
    grain->setGeometry(root->rect());
    grain->raise();
    root->installEventFilter(this);

    qApp->installEventFilter(this);
}

void MainWindow::refreshEncoderUi()
{
    m_gpuCard->setGpuInfo(m_caps->gpuInfo());

    m_encoderCombo->clear();
    for (const auto& enc : m_caps->encoderOptions()) {
        QString label = enc.label;
        if (!enc.badge.isEmpty())
            label += QStringLiteral("  [%1]").arg(enc.badge);
        m_encoderCombo->addItem(label, enc.id);
    }

    m_formatCombo->clear();
    for (const auto& fmt : m_caps->formatOptions())
        m_formatCombo->addItem(fmt.label, fmt.id);

    if (m_formatCombo->count() > 0) {
        const auto fmt = m_caps->formatById(selectedFormatId());
        m_formatHelper->setText(fmt.helperText);
    }
    if (m_gpuBadge) {
        const auto enc = m_caps->encoderById(selectedEncoderId());
        m_gpuBadge->setVisible(enc.isGpu);
    }
}

void MainWindow::setControlsEnabled(bool enabled)
{
    m_playBtn->setEnabled(enabled);
    m_exportBtn->setEnabled(enabled);
    m_volumeSlider->setEnabled(enabled);
    m_timeline->setEnabled(enabled);
    m_startEdit->setEnabled(enabled);
    m_endEdit->setEnabled(enabled);
    m_startUp->setEnabled(enabled);
    m_startDown->setEnabled(enabled);
    m_endUp->setEnabled(enabled);
    m_endDown->setEnabled(enabled);
}

void MainWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Video"), QString(),
        QStringLiteral("Video Files (*.mp4 *.mov *.mkv *.webm *.avi *.m4v *.wmv *.flv *.ts *.mpg *.mpeg);;All Files (*.*)"));
    if (!path.isEmpty())
        loadVideo(path);
}

void MainWindow::loadVideo(const QString& path)
{
    m_thumbs->cancel();
    m_player->pause();
    m_player->load(path);
}

void MainWindow::onPlayPause()
{
    m_player->togglePlayPause();
}

void MainWindow::onVolumeChanged(int value)
{
    m_player->setVolume(value / 100.0f);
}

void MainWindow::onTrimChanged(qint64 startMs, qint64 endMs)
{
    Q_UNUSED(startMs);
    Q_UNUSED(endMs);
    syncTrimFields();
    updateDurationLabel();
}

void MainWindow::syncTrimFields()
{
    m_updatingFields = true;
    m_startEdit->setText(TimeFormat::formatMs(m_timeline->startMs()));
    m_endEdit->setText(TimeFormat::formatMs(m_timeline->endMs()));
    m_updatingFields = false;
}

void MainWindow::updateDurationLabel()
{
    const qint64 dur = m_timeline->endMs() - m_timeline->startMs();
    m_trimDurationLabel->setText(QStringLiteral("Duration  %1").arg(TimeFormat::formatMs(dur)));
}

void MainWindow::onStartEdited()
{
    if (m_updatingFields)
        return;
    bool ok = false;
    qint64 ms = TimeFormat::parseToMs(m_startEdit->text(), &ok);
    if (!ok) {
        syncTrimFields();
        return;
    }
    ms = qBound(ms, 0LL, m_timeline->endMs());
    m_timeline->setTrimRange(ms, m_timeline->endMs());
    syncTrimFields();
    updateDurationLabel();
}

void MainWindow::onEndEdited()
{
    if (m_updatingFields)
        return;
    bool ok = false;
    qint64 ms = TimeFormat::parseToMs(m_endEdit->text(), &ok);
    if (!ok) {
        syncTrimFields();
        return;
    }
    ms = qBound(ms, m_timeline->startMs(), m_player->duration());
    m_timeline->setTrimRange(m_timeline->startMs(), ms);
    syncTrimFields();
    updateDurationLabel();
}

void MainWindow::stepStart(int direction)
{
    const qint64 ms = qBound(m_timeline->startMs() + direction * 100, 0LL, m_timeline->endMs());
    m_timeline->setTrimRange(ms, m_timeline->endMs());
    syncTrimFields();
    updateDurationLabel();
}

void MainWindow::stepEnd(int direction)
{
    const qint64 ms =
        qBound(m_timeline->endMs() + direction * 100, m_timeline->startMs(), m_player->duration());
    m_timeline->setTrimRange(m_timeline->startMs(), ms);
    syncTrimFields();
    updateDurationLabel();
}

void MainWindow::updateTimeLabels(qint64 positionMs)
{
    const qint64 dur = m_player->duration();
    m_timeReadout->setText(QStringLiteral("%1 / %2")
                               .arg(TimeFormat::formatMs(positionMs), TimeFormat::formatMs(dur)));
}

QString MainWindow::selectedEncoderId() const
{
    return m_encoderCombo->currentData().toString();
}

QString MainWindow::selectedFormatId() const
{
    return m_formatCombo->currentData().toString();
}

void MainWindow::onExport()
{
    if (m_player->metadata().path.isEmpty())
        return;
    if (m_timeline->endMs() <= m_timeline->startMs()) {
        QMessageBox::warning(this, QStringLiteral("Trimmi"),
                             QStringLiteral("End time must be greater than Start time."));
        return;
    }

    const auto format = m_caps->formatById(selectedFormatId());
    const QString filter =
        QStringLiteral("%1 (*.%2);;All Files (*.*)").arg(format.label, format.defaultExtension);
    const QString suggested =
        QFileInfo(m_player->metadata().path).completeBaseName() + QStringLiteral("_trim.")
        + format.defaultExtension;

    const QString out = QFileDialog::getSaveFileName(this, QStringLiteral("Export Trimmed Video"),
                                                     suggested, filter);
    if (out.isEmpty())
        return;

    ExportRequest req;
    req.inputPath = m_player->metadata().path;
    req.outputPath = out;
    req.startMs = m_timeline->startMs();
    req.endMs = m_timeline->endMs();
    req.encoder = m_caps->encoderById(selectedEncoderId());
    req.format = format;

    m_exportBtn->setEnabled(false);
    m_progressDialog = new QProgressDialog(QStringLiteral("Exporting…"), QStringLiteral("Cancel"),
                                           0, 100, this);
    m_progressDialog->setWindowTitle(QStringLiteral("Trim & Export"));
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setMinimumDuration(0);
    m_progressDialog->setValue(0);
    connect(m_progressDialog, &QProgressDialog::canceled, this, [this]() {
        m_exporter->cancel();
        m_exportBtn->setEnabled(true);
    });

    m_exporter->exportVideo(req);
}

void MainWindow::onFullscreen()
{
    if (!m_fullscreen) {
        m_player->videoWidget()->setParent(nullptr);
        m_player->videoWidget()->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        m_player->videoWidget()->showFullScreen();
        m_player->videoWidget()->installEventFilter(this);
        m_fullscreen = true;
    } else {
        m_player->videoWidget()->removeEventFilter(this);
        m_player->videoWidget()->setWindowFlags(Qt::Widget);
        auto* layout = qobject_cast<QVBoxLayout*>(m_previewHost->layout());
        if (layout)
            layout->addWidget(m_player->videoWidget());
        m_player->videoWidget()->showNormal();
        m_fullscreen = false;
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange && m_titleBar)
        m_titleBar->setMaximized(isMaximized());
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_normalCentral && event->type() == QEvent::Resize) {
        if (auto* grain = m_normalCentral->findChild<QWidget*>(QStringLiteral("grainOverlay"))) {
            grain->setGeometry(m_normalCentral->rect());
            grain->raise();
        }
    }
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && m_fullscreen) {
            onFullscreen();
            return true;
        }
        if (key->key() == Qt::Key_Space && watched != m_startEdit && watched != m_endEdit) {
            if (m_player->metadata().valid) {
                onPlayPause();
                return true;
            }
        }
    }
    if (m_fullscreen && watched == m_player->videoWidget()
        && event->type() == QEvent::MouseButtonDblClick) {
        onFullscreen();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}
