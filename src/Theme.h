#pragma once

#include <QString>

namespace Theme {

// 8-bit cottagecore / matcha (Compressi-aligned light theme)
inline constexpr const char* WindowBg = "#E8DFD0";
inline constexpr const char* PanelBg = "#F7F0E2";
inline constexpr const char* PanelBorder = "#A7A28B";
inline constexpr const char* Surface = "#FFFBF3";
inline constexpr const char* SurfaceBorder = "#6B744F";
inline constexpr const char* SoftGreenWash = "#E8ECD9";
inline constexpr const char* SecurityFill = "#E4E8D4";

inline constexpr const char* TextPrimary = "#2A3220";
inline constexpr const char* TextSecondary = "rgba(42, 50, 32, 0.75)";
inline constexpr const char* TextMuted = "rgba(42, 50, 32, 0.55)";
inline constexpr const char* Label = "#5A564C";
inline constexpr const char* Subtitle = "#4F5838";

inline constexpr const char* Stipple = "#C2C3A2";
inline constexpr const char* StippleBtn = "#A8AA8C";
inline constexpr const char* Accent = "#8F9A6E";
inline constexpr const char* AccentHover = "#7E895F";
inline constexpr const char* AccentPressed = "#6A744C";
inline constexpr const char* AccentLight = "#A7B18F";

inline constexpr const char* Success = "#5F6B45";
inline constexpr const char* Danger = "#9B3B3B";
inline constexpr const char* Warning = "#B07A45";

// Legacy aliases used across painted widgets
inline constexpr const char* PanelBgRaised = Surface;
inline constexpr const char* Border = PanelBorder;
inline constexpr const char* BorderMuted = SurfaceBorder;
inline constexpr const char* Selection = "rgba(143, 154, 110, 0.35)";

inline constexpr const char* UiFontFamily = "Pixelify Sans";

inline QString appStyleSheet()
{
    // Colors inlined to avoid QString::arg %1/%11 collisions.
    return QStringLiteral(R"(
        QWidget {
            background-color: #E8DFD0;
            color: #2A3220;
            font-family: "Pixelify Sans";
            font-size: 14px;
        }
        QMainWindow, QDialog {
            background-color: #E8DFD0;
        }
        QLabel {
            background: transparent;
            color: #2A3220;
        }
        QLabel#sectionTitle {
            font-size: 16px;
            font-weight: 700;
            color: #4F5838;
            border-bottom: 1px solid #4F5838;
            padding-bottom: 3px;
            margin-bottom: 2px;
        }
        QLabel#fieldLabel {
            font-size: 14px;
            font-weight: 700;
            color: #5A564C;
        }
        QLabel#helperText {
            color: rgba(42, 50, 32, 0.75);
            font-size: 12px;
        }
        QLabel#fileNameLabel {
            font-size: 16px;
            font-weight: 700;
            color: #2A3220;
        }
        QLabel#durationLabel {
            font-size: 12px;
            color: rgba(42, 50, 32, 0.75);
            font-variant-numeric: tabular-nums;
        }
        QComboBox {
            background-color: #FFFBF3;
            border: 1px solid #6B744F;
            border-radius: 4px;
            padding: 6px 10px;
            min-height: 31px;
            color: #2A3220;
            font-size: 14px;
        }
        QComboBox:hover {
            border-color: #8F9A6E;
            background-color: #E8ECD9;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #4F5838;
            width: 0;
            height: 0;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #FFFBF3;
            border: 1px solid #6B744F;
            selection-background-color: #C2C3A2;
            selection-color: #2A3220;
            outline: none;
            padding: 4px;
            color: #2A3220;
        }
        QPushButton {
            background-color: #FFFBF3;
            border: 1px solid #6B744F;
            border-width: 1px 1px 2px 1px;
            border-radius: 4px;
            padding: 6px 14px;
            min-height: 30px;
            color: #2A3220;
            font-weight: 700;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #E8ECD9;
            border-color: #8F9A6E;
        }
        QPushButton:pressed {
            border-width: 1px 1px 1px 1px;
            background-color: #C2C3A2;
        }
        QPushButton:disabled {
            color: rgba(42, 50, 32, 0.55);
            border-color: #A7A28B;
        }
        QPushButton#accentButton, QPushButton#exportButton {
            background-color: #A8AA8C;
            border: 1px solid #2A3220;
            border-width: 1px 1px 3px 1px;
            border-radius: 4px;
            color: #2A3220;
            font-weight: 700;
            font-size: 14px;
            padding: 7px 16px;
            min-height: 34px;
        }
        QPushButton#exportButton {
            padding: 10px 20px;
            min-height: 38px;
        }
        QPushButton#accentButton:hover, QPushButton#exportButton:hover {
            background-color: #7E895F;
        }
        QPushButton#accentButton:pressed, QPushButton#exportButton:pressed {
            background-color: #6A744C;
            border-width: 1px 1px 2px 1px;
        }
        QPushButton#accentButton:disabled, QPushButton#exportButton:disabled {
            background-color: #C2C3A2;
            color: rgba(42, 50, 32, 0.55);
            border-color: #A7A28B;
        }
        QPushButton#iconButton {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 4px;
            min-width: 32px;
            min-height: 32px;
            font-weight: 700;
            color: #2A3220;
        }
        QPushButton#iconButton:hover {
            background-color: #C2C3A2;
            border-color: #A7A28B;
        }
        QPushButton#iconButton:pressed {
            background-color: #A8AA8C;
            border-width: 1px;
        }
        QPushButton#titleButton {
            background: transparent;
            border: none;
            border-radius: 0;
            min-width: 46px;
            max-width: 46px;
            min-height: 36px;
            color: #2A3220;
            font-size: 12px;
            font-weight: 700;
        }
        QPushButton#titleButton:hover {
            background-color: #C2C3A2;
        }
        QPushButton#closeButton:hover {
            background-color: #9B3B3B;
            color: #FFFBF3;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #A7A28B;
            border: 1px solid #6B744F;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            height: 14px;
            margin: -5px 0;
            background: #8F9A6E;
            border: 1px solid #2A3220;
            border-radius: 3px;
        }
        QSlider::handle:horizontal:hover {
            background: #7E895F;
        }
        QSlider::handle:horizontal:pressed {
            background: #6A744C;
        }
        QSlider::sub-page:horizontal {
            background: #8F9A6E;
            border-radius: 2px;
        }
        QDoubleSpinBox, QSpinBox, QLineEdit {
            background-color: #FFFBF3;
            border: 1px solid #6B744F;
            border-radius: 4px;
            padding: 6px 8px;
            min-height: 31px;
            color: #2A3220;
            selection-background-color: #C2C3A2;
            selection-color: #2A3220;
            font-size: 14px;
        }
        QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus {
            border-color: #8F9A6E;
        }
        QProgressBar {
            background-color: #FFFBF3;
            border: 1px solid #6B744F;
            border-radius: 4px;
            text-align: center;
            color: #2A3220;
            height: 18px;
            font-size: 12px;
        }
        QProgressBar::chunk {
            background-color: #8F9A6E;
            border-radius: 3px;
        }
        QScrollBar:vertical {
            background: #E8DFD0;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #C2C3A2;
            border: 1px solid #A7A28B;
            border-radius: 2px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QFrame#sidebarPanel {
            background-color: #F7F0E2;
            border: 1px solid #A7A28B;
            border-radius: 10px;
        }
        QFrame#gpuCard {
            background-color: #E4E8D4;
            border: 1px solid #6B744F;
            border-radius: 12px;
        }
        QFrame#topBar {
            background: transparent;
            border-bottom: 1px solid #A7A28B;
        }
        QFrame#mainPanel {
            background-color: #F7F0E2;
            border: 1px solid #A7A28B;
            border-radius: 10px;
        }
        QToolTip {
            background-color: #FFFBF3;
            color: #2A3220;
            border: 1px solid #6B744F;
            padding: 4px 8px;
            font-size: 12px;
        }
        QMessageBox, QProgressDialog {
            background-color: #F7F0E2;
        }
    )");
}

} // namespace Theme
