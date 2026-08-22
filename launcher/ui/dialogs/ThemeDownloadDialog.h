// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 Prism Launcher Contributors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 */
#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTreeWidget;
class QTreeWidgetItem;

class ThemeDownloadDialog : public QDialog {
    Q_OBJECT

   public:
    explicit ThemeDownloadDialog(QWidget* parent = nullptr);

   signals:
    void themesInstalled();

   protected:
    void showEvent(QShowEvent* event) override;

   private slots:
    void loadRelease();
    void installChecked();
    void filterAssets(const QString& text);
    void selectAllVisible();
    void updateInstallButton();
    void manageSources();

   private:
    enum class AssetKind { Theme, Icons, CatPack };

    static bool assetKind(const QString& fileName, AssetKind& kind);
    static QString assetName(const QString& fileName, AssetKind kind);
    static QString assetTypeName(AssetKind kind);
    static QString archivePrefix(AssetKind kind);
    static QString installPath(AssetKind kind);

    bool validateArchive(const QString& path, AssetKind kind, QString& error) const;
    bool validateDownload(QTreeWidgetItem* item, const QString& path, QString& error) const;
    void reloadSources(const QString& preferredSource = {});

    QComboBox* m_sourceCombo = nullptr;
    QLabel* m_releaseLabel = nullptr;
    QLineEdit* m_searchBox = nullptr;
    QTreeWidget* m_assetList = nullptr;
    QPushButton* m_installButton = nullptr;
    bool m_loadStarted = false;
};
