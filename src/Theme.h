#pragma once

#include <QString>

namespace Theme {

inline constexpr const char* WindowBg = "#1a1a1a";
inline constexpr const char* PanelBg = "#232323";
inline constexpr const char* PanelBgRaised = "#2a2a2a";
inline constexpr const char* Border = "#333333";
inline constexpr const char* BorderMuted = "#3a3a3a";
inline constexpr const char* TextPrimary = "#e8e8e8";
inline constexpr const char* TextSecondary = "#9a9a9a";
inline constexpr const char* TextMuted = "#6a6a6a";
inline constexpr const char* Accent = "#2f7ff0";
inline constexpr const char* AccentHover = "#3d8cff";
inline constexpr const char* AccentPressed = "#2568c8";
inline constexpr const char* Danger = "#e05555";
inline constexpr const char* Success = "#3cb371";
inline constexpr const char* Selection = "rgba(47, 127, 240, 0.35)";

inline QString appStyleSheet()
{
    return QStringLiteral(R"(
        QWidget {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI", "SF Pro Text", sans-serif;
            font-size: 13px;
        }
        QMainWindow, QDialog {
            background-color: %1;
        }
        QLabel {
            background: transparent;
            color: %2;
        }
        QLabel#sectionTitle {
            font-size: 13px;
            font-weight: 600;
            color: %2;
            letter-spacing: 0.2px;
        }
        QLabel#helperText {
            color: %3;
            font-size: 11px;
        }
        QLabel#fileNameLabel {
            font-size: 14px;
            font-weight: 600;
            color: %2;
        }
        QLabel#durationLabel {
            font-size: 13px;
            color: %3;
            font-variant-numeric: tabular-nums;
        }
        QComboBox {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 7px 10px;
            min-height: 20px;
            color: %2;
        }
        QComboBox:hover {
            border-color: %6;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid %3;
            width: 0;
            height: 0;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %4;
            border: 1px solid %5;
            selection-background-color: %6;
            outline: none;
            padding: 4px;
        }
        QPushButton {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 8px 14px;
            color: %2;
        }
        QPushButton:hover {
            background-color: %7;
            border-color: %6;
        }
        QPushButton:pressed {
            background-color: %1;
        }
        QPushButton:disabled {
            color: %8;
            border-color: %5;
        }
        QPushButton#accentButton {
            background-color: %6;
            border: none;
            color: white;
            font-weight: 600;
            padding: 9px 18px;
        }
        QPushButton#accentButton:hover {
            background-color: %9;
        }
        QPushButton#accentButton:pressed {
            background-color: %10;
        }
        QPushButton#accentButton:disabled {
            background-color: #3a4a60;
            color: #8a9ab0;
        }
        QPushButton#exportButton {
            background-color: %6;
            border: none;
            color: white;
            font-weight: 600;
            font-size: 14px;
            padding: 12px 22px;
            border-radius: 8px;
        }
        QPushButton#exportButton:hover {
            background-color: %9;
        }
        QPushButton#exportButton:pressed {
            background-color: %10;
        }
        QPushButton#exportButton:disabled {
            background-color: #3a4a60;
            color: #8a9ab0;
        }
        QPushButton#iconButton {
            background: transparent;
            border: none;
            border-radius: 6px;
            padding: 6px;
            min-width: 32px;
            min-height: 32px;
        }
        QPushButton#iconButton:hover {
            background-color: %7;
        }
        QPushButton#titleButton {
            background: transparent;
            border: none;
            border-radius: 0;
            min-width: 46px;
            max-width: 46px;
            min-height: 36px;
            color: %2;
            font-size: 11px;
        }
        QPushButton#titleButton:hover {
            background-color: #333333;
        }
        QPushButton#closeButton:hover {
            background-color: #e81123;
            color: white;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: %5;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 12px;
            height: 12px;
            margin: -4px 0;
            background: %2;
            border-radius: 6px;
        }
        QSlider::sub-page:horizontal {
            background: %6;
            border-radius: 2px;
        }
        QDoubleSpinBox, QSpinBox, QLineEdit {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            padding: 6px 8px;
            color: %2;
            selection-background-color: %6;
        }
        QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus {
            border-color: %6;
        }
        QProgressBar {
            background-color: %4;
            border: 1px solid %5;
            border-radius: 6px;
            text-align: center;
            color: %2;
            height: 18px;
        }
        QProgressBar::chunk {
            background-color: %6;
            border-radius: 5px;
        }
        QScrollBar:vertical {
            background: %1;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %5;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QFrame#sidebarPanel {
            background-color: %4;
            border-right: 1px solid %5;
        }
        QFrame#gpuCard {
            background-color: %7;
            border: 1px solid %5;
            border-radius: 8px;
        }
        QFrame#topBar {
            background: transparent;
            border-bottom: 1px solid %5;
        }
        QToolTip {
            background-color: %4;
            color: %2;
            border: 1px solid %5;
            padding: 4px 8px;
        }
    )")
        .arg(QLatin1String(WindowBg),
             QLatin1String(TextPrimary),
             QLatin1String(TextSecondary),
             QLatin1String(PanelBg),
             QLatin1String(Border),
             QLatin1String(Accent),
             QLatin1String(PanelBgRaised),
             QLatin1String(TextMuted),
             QLatin1String(AccentHover),
             QLatin1String(AccentPressed));
}

} // namespace Theme
