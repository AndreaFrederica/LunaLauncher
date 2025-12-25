// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ModernDarkTheme.h"

#include <QObject>

// Modern Dark Theme Colors
namespace ModernColors {
    // Background colors
    constexpr QRgb Background = 0xFF202020;           // Window background
    constexpr QRgb BackgroundElevated = 0xFF2C2C2C;   // Elevated surface (cards, dialogs)
    constexpr QRgb BackgroundCanvas = 0xFF1A1A1A;     // Canvas area

    // Text colors
    constexpr QRgb TextPrimary = 0xFFF8F8F8;
    constexpr QRgb TextSecondary = 0xFFDEDEDE;
    constexpr QRgb TextTertiary = 0xFFC8C8C8;
    constexpr QRgb TextDisabled = 0xFF6E6E6E;

    // Accent colors (Blue theme)
    constexpr QRgb Accent = 0xFF0078D4;
    constexpr QRgb AccentHover = 0xFF106EBE;
    constexpr QRgb AccentPressed = 0xFF005A9E;
    constexpr QRgb AccentLight1 = 0xFF4DA0FF;
    constexpr QRgb AccentLight2 = 0xFF80C8FF;
    constexpr QRgb AccentLight3 = 0xFFB3E0FF;

    // Border colors
    constexpr QRgb BorderDefault = 0xFF383838;
    constexpr QRgb BorderHover = 0xFF4A4A4A;
    constexpr QRgb BorderFocused = 0xFF0078D4;
    constexpr QRgb BorderDisabled = 0xFF2A2A2A;

    // Card/Surface colors
    constexpr QRgb CardBackground = 0xFF2A2A2A;
    constexpr QRgb CardBackgroundHover = 0xFF323232;
    constexpr QRgb CardBackgroundPressed = 0xFF3E3E3E;

    // Input colors
    constexpr QRgb InputBackground = 0xFF1E1E1E;
    constexpr QRgb InputBorder = 0xFF383838;
    constexpr QRgb InputBorderHover = 0xFF4A4A4A;
    constexpr QRgb InputBorderFocused = 0xFF0078D4;

    // State colors
    constexpr QRgb Error = 0xFFD13438;
    constexpr QRgb Warning = 0xFFFFAA00;
    constexpr QRgb Success = 0xFF107C10;
    constexpr QRgb Info = 0xFF0078D4;

    // Link colors
    constexpr QRgb Link = 0xFF4DA0FF;
    constexpr QRgb LinkHover = 0xFF6BB1FF;
    constexpr QRgb LinkPressed = 0xFF004578;
}

QString ModernDarkTheme::id()
{
    return "modern-dark";
}

QString ModernDarkTheme::name()
{
    return QObject::tr("Modern Dark");
}

QPalette ModernDarkTheme::colorScheme()
{
    QPalette palette;

    // Window colors
    palette.setColor(QPalette::Window, QColor(ModernColors::Background));
    palette.setColor(QPalette::WindowText, QColor(ModernColors::TextPrimary));

    // Base colors (for text input widgets, tables, etc.)
    palette.setColor(QPalette::Base, QColor(ModernColors::InputBackground));
    palette.setColor(QPalette::AlternateBase, QColor(ModernColors::BackgroundElevated));

    // Text colors
    palette.setColor(QPalette::Text, QColor(ModernColors::TextPrimary));
    palette.setColor(QPalette::PlaceholderText, QColor(ModernColors::TextDisabled));

    // Button colors
    palette.setColor(QPalette::Button, QColor(ModernColors::CardBackground));
    palette.setColor(QPalette::ButtonText, QColor(ModernColors::TextPrimary));

    // ToolTip colors
    palette.setColor(QPalette::ToolTipBase, QColor(ModernColors::BackgroundElevated));
    palette.setColor(QPalette::ToolTipText, QColor(ModernColors::TextPrimary));

    // Highlight and selection colors
    palette.setColor(QPalette::Highlight, QColor(ModernColors::Accent));
    palette.setColor(QPalette::HighlightedText, Qt::white);

    // Link colors
    palette.setColor(QPalette::Link, QColor(ModernColors::Link));

    // Misc colors
    palette.setColor(QPalette::BrightText, QColor(ModernColors::Error));

    return fadeInactive(palette, fadeAmount(), fadeColor());
}

double ModernDarkTheme::fadeAmount()
{
    return 0.5;
}

QColor ModernDarkTheme::fadeColor()
{
    return QColor(ModernColors::Background);
}

bool ModernDarkTheme::hasStyleSheet()
{
    return true;
}

QString ModernDarkTheme::appStyleSheet()
{
    // Modern Dark QSS stylesheet
    return R"(
        /* ========== Global Settings ========== */
        * {
            font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
        }

        /* ========== MainWindow ========== */
        QMainWindow {
            background-color: #202020;
        }

        /* ========== Buttons ========== */
        QPushButton {
            background-color: #3E3E3E;
            color: #F8F8F8;
            border: 1px solid #3E3E3E;
            border-radius: 4px;
            padding: 6px 20px;
            font-weight: normal;
        }

        QPushButton:hover {
            background-color: #4A4A4A;
            border-color: #4A4A4A;
        }

        QPushButton:pressed {
            background-color: #2A2A2A;
            border-color: #2A2A2A;
        }

        QPushButton:disabled {
            background-color: #2A2A2A;
            color: #6E6E6E;
            border-color: #2A2A2A;
        }

        QPushButton:default {
            background-color: #0078D4;
            border-color: #0078D4;
        }

        QPushButton:default:hover {
            background-color: #1A86D9;
            border-color: #1A86D9;
        }

        QPushButton:default:pressed {
            background-color: #005A9E;
            border-color: #005A9E;
        }

        /* ========== Tool Button ========== */
        QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
            padding: 4px;
        }

        QToolButton:hover {
            background-color: rgba(255, 255, 255, 0.08);
        }

        QToolButton:pressed {
            background-color: rgba(255, 255, 255, 0.04);
        }

        /* ========== Line Edit ========== */
        QLineEdit {
            background-color: #1E1E1E;
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            padding: 6px 11px;
            selection-background-color: #0078D4;
            selection-color: #FFFFFF;
        }

        QLineEdit:hover {
            border-color: #4A4A4A;
            background-color: #252525;
        }

        QLineEdit:focus {
            border-color: #0078D4;
            background-color: #1E1E1E;
        }

        QLineEdit:disabled {
            background-color: #2A2A2A;
            color: #6E6E6E;
            border-color: #2A2A2A;
        }

        /* ========== Text Edit ========== */
        QTextEdit {
            background-color: #1E1E1E;
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            selection-background-color: #0078D4;
            selection-color: #FFFFFF;
        }

        QTextEdit:focus {
            border-color: #0078D4;
        }

        /* ========== Combo Box ========== */
        QComboBox {
            background-color: #2A2A2A;
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            padding: 4px 11px;
        }

        QComboBox:hover {
            border-color: #4A4A4A;
        }

        QComboBox:focus {
            border-color: #0078D4;
        }

        QComboBox QAbstractItemView {
            background-color: #2C2C2C;
            border: 1px solid #383838;
            selection-background-color: #0078D4;
            selection-color: #FFFFFF;
            outline: none;
        }

        /* ========== Spin Box ========== */
        QSpinBox, QDoubleSpinBox {
            background-color: #2A2A2A;
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            selection-background-color: #0078D4;
            selection-color: #FFFFFF;
        }

        QSpinBox:hover, QDoubleSpinBox:hover {
            border-color: #4A4A4A;
        }

        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #0078D4;
        }

        /* ========== Check Box ========== */
        QCheckBox {
            spacing: 8px;
            color: #F8F8F8;
        }

        /* ========== Radio Button ========== */
        QRadioButton {
            spacing: 8px;
            color: #F8F8F8;
        }

        /* ========== Slider ========== */
        QSlider::groove:horizontal {
            height: 4px;
            background-color: #383838;
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            width: 18px;
            height: 18px;
            background-color: #F8F8F8;
            border: none;
            border-radius: 9px;
            margin: -7px 0;
        }

        QSlider::handle:horizontal:hover {
            background-color: #FFFFFF;
            width: 20px;
            height: 20px;
            margin: -8px 0;
        }

        QSlider::sub-page:horizontal {
            background-color: #0078D4;
            border-radius: 2px;
        }

        /* ========== ScrollBar ========== */
        QScrollBar:vertical {
            background-color: transparent;
            width: 12px;
            margin: 0;
        }

        QScrollBar::handle:vertical {
            background-color: #4A4A4A;
            min-height: 24px;
            border-radius: 6px;
            margin: 2px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #5A5A5A;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }

        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background-color: none;
        }

        QScrollBar:horizontal {
            background-color: transparent;
            height: 12px;
            margin: 0;
        }

        QScrollBar::handle:horizontal {
            background-color: #4A4A4A;
            min-width: 24px;
            border-radius: 6px;
            margin: 2px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #5A5A5A;
        }

        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }

        /* ========== Group Box ========== */
        QGroupBox {
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            margin-top: 12px;
            padding-top: 8px;
            font-weight: bold;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px 0 4px;
        }

        /* ========== Tab Widget ========== */
        QTabWidget::pane {
            border: none;
            background-color: #202020;
        }

        QTabBar::tab {
            background-color: transparent;
            color: #DEDEDE;
            border: none;
            padding: 8px 16px;
            margin-right: 2px;
        }

        QTabBar::tab:hover {
            color: #F8F8F8;
        }

        QTabBar::tab:selected {
            color: #F8F8F8;
            border-bottom: 2px solid #0078D4;
        }

        QTabBar::tab:!selected {
            margin-bottom: 2px;
        }

        /* ========== Menu ========== */
        QMenu {
            background-color: #2C2C2C;
            border: 1px solid #383838;
            border-radius: 4px;
            padding: 4px;
        }

        QMenu::item {
            padding: 6px 32px 6px 12px;
            border-radius: 4px;
        }

        QMenu::item:selected {
            background-color: rgba(255, 255, 255, 0.08);
        }

        QMenu::separator {
            height: 1px;
            background-color: #383838;
            margin: 4px 8px;
        }

        /* ========== ToolTip ========== */
        QToolTip {
            background-color: #2C2C2C;
            color: #F8F8F8;
            border: 1px solid #383838;
            border-radius: 4px;
            padding: 4px 8px;
        }

        /* ========== Progress Bar ========== */
        QProgressBar {
            background-color: #2A2A2A;
            border: none;
            border-radius: 2px;
            height: 4px;
            text-align: center;
        }

        QProgressBar::chunk {
            background-color: #0078D4;
            border-radius: 2px;
        }

        /* ========== Frame ========== */
        QFrame {
            background-color: transparent;
        }

        QFrame[border="true"] {
            border: 1px solid #383838;
            border-radius: 4px;
        }

        /* ========== Label ========== */
        QLabel {
            color: #F8F8F8;
        }

        QLabel:disabled {
            color: #6E6E6E;
        }

        /* ========== Separator ========== */
        QSeparator {
            background-color: #383838;
        }

        /* ========== Dock Widget ========== */
        QDockWidget {
            background-color: #202020;
            color: #F8F8F8;
        }

        QDockWidget::title {
            background-color: #2C2C2C;
            padding: 8px;
            border: none;
        }

        QDockWidget::close-button, QDockWidget::float-button {
            background-color: transparent;
            border: none;
        }

        QDockWidget::close-button:hover, QDockWidget::float-button:hover {
            background-color: rgba(255, 255, 255, 0.08);
        }

        /* ========== Splitter ========== */
        QSplitter::handle {
            background-color: #383838;
        }

        QSplitter::handle:hover {
            background-color: #4A4A4A;
        }

        QSplitter::handle:horizontal {
            width: 1px;
        }

        QSplitter::handle:vertical {
            height: 1px;
        }

        /* ========== List Widget ========== */
        QListWidget {
            background-color: #1E1E1E;
            border: 1px solid #383838;
            border-radius: 4px;
            outline: none;
        }

        QListWidget::item {
            padding: 6px 11px;
            border-radius: 4px;
        }

        QListWidget::item:hover {
            background-color: rgba(255, 255, 255, 0.08);
        }

        QListWidget::item:selected {
            background-color: #0078D4;
            color: #FFFFFF;
        }

        QListWidget::item:selected:!active {
            background-color: #2A2A2A;
            color: #F8F8F8;
        }

        /* ========== Table Widget ========== */
        QTableWidget {
            background-color: #1E1E1E;
            border: 1px solid #383838;
            border-radius: 4px;
            gridline-color: #383838;
            outline: none;
        }

        QTableWidget::item {
            padding: 4px;
            border: none;
        }

        QTableWidget::item:hover {
            background-color: rgba(255, 255, 255, 0.04);
        }

        QTableWidget::item:selected {
            background-color: #0078D4;
            color: #FFFFFF;
        }

        QHeaderView::section {
            background-color: #2A2A2A;
            color: #F8F8F8;
            padding: 6px 11px;
            border: none;
            border-right: 1px solid #383838;
            border-bottom: 1px solid #383838;
        }

        QTableWidget QTableCornerButton::section {
            background-color: #2A2A2A;
            border: none;
        }

        /* ========== Tree Widget ========== */
        QTreeWidget {
            background-color: #1E1E1E;
            border: 1px solid #383838;
            border-radius: 4px;
            outline: none;
        }

        QTreeWidget::item {
            padding: 4px;
        }

        QTreeWidget::item:hover {
            background-color: rgba(255, 255, 255, 0.04);
        }

        QTreeWidget::item:selected {
            background-color: #0078D4;
            color: #FFFFFF;
        }

        QTreeWidget::branch:has-children:!has-siblings:closed,
        QTreeWidget::branch:closed:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branches/closed);
        }

        QTreeWidget::branch:open:has-children:!has-siblings,
        QTreeWidget::branch:open:has-children:has-siblings {
            border-image: none;
            image: url(:/icons/branches/open);
        }
    )";
}

QString ModernDarkTheme::tooltip()
{
    return QObject::tr("Modern dark theme with clean styling");
}
