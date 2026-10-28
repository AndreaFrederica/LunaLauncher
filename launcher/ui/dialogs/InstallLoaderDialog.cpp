// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2023 TheKodeToad <TheKodeToad@proton.me>
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

#include "InstallLoaderDialog.h"

#include <QDialogButtonBox>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <utility>
#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "meta/Index.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "api/LauncherApiSpecial.h"
#include "ui/widgets/PageContainer.h"
#include "ui/widgets/SpecialLoaderVersionList.h"
#include "ui/widgets/VersionSelectWidget.h"

class InstallLoaderPage : public VersionSelectWidget, public BasePage {
    Q_OBJECT
   public:
    InstallLoaderPage(const QString& id,
                      const QString& iconName,
                      const QString& name,
                      const Version& oldestVersion,
                      PackProfile* profile,
                      QString specialLoader = {})
        : VersionSelectWidget(nullptr), uid(id), iconName(iconName), name(name), specialLoader(std::move(specialLoader))
    {
        const QString minecraftVersion = profile->getComponentVersion("net.minecraft");
        setEmptyString(tr("No versions are currently available for Minecraft %1").arg(minecraftVersion));
        setExactIfPresentFilter(BaseVersionList::ParentVersionRole, minecraftVersion);

        if (oldestVersion != Version() && Version(minecraftVersion) < oldestVersion)
            setExactFilter(BaseVersionList::ParentVersionRole, "AAA");

        const auto currentUid = !this->specialLoader.isEmpty() && this->specialLoader.startsWith("labymod") ? QStringLiteral("net.labymod") : id;
        if (const QString currentVersion = profile->getComponentVersion(currentUid); !currentVersion.isNull())
            setCurrentVersion(currentVersion);
        if (!this->specialLoader.isEmpty())
            specialList = new SpecialLoaderVersionList(this->specialLoader, minecraftVersion, this);
    }

    QString id() const override { return uid; }
    QString displayName() const override { return name; }
    QIcon icon() const override { return QIcon::fromTheme(iconName); }

    void openedImpl() override
    {
        if (loaded)
            return;

        if (specialList)
            initialize(specialList, true);
        else {
            const auto versions = APPLICATION->metadataIndex()->get(uid);
            if (!versions)
                return;
            initialize(versions.get());
        }
        loaded = true;
    }

    bool isSpecial() const { return !specialLoader.isEmpty(); }
    QString specialLoaderId() const { return specialLoader; }
    SpecialLoaderVersion* selectedSpecialVersion() const
    {
        if (!specialList || !selectedVersion())
            return nullptr;
        return dynamic_cast<SpecialLoaderVersion*>(selectedVersion().get());
    }

    void setParentContainer(BasePageContainer* container) override
    {
        auto dialog = dynamic_cast<QDialog*>(dynamic_cast<PageContainer*>(container)->parent());
        connect(view(), &QAbstractItemView::doubleClicked, dialog, &QDialog::accept);
    }

   private:
    const QString uid;
    const QString iconName;
    const QString name;
    const QString specialLoader;
    SpecialLoaderVersionList* specialList = nullptr;
    bool loaded = false;
};

static InstallLoaderPage* pageCast(BasePage* page)
{
    auto result = dynamic_cast<InstallLoaderPage*>(page);
    Q_ASSERT(result != nullptr);
    return result;
}

InstallLoaderDialog::InstallLoaderDialog(PackProfile* profile, const QString& uid, QWidget* parent)
    : QDialog(parent), profile(profile), container(new PageContainer(this, QString(), this)), buttons(new QDialogButtonBox(this))
{
    auto layout = new QVBoxLayout(this);
    // small margins look ugly on macOS on modal windows
    #ifndef Q_OS_MACOS
    layout->setContentsMargins(0, 0, 0, 0);
    #endif
    container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    layout->addWidget(container);

    auto buttonLayout = new QHBoxLayout(this);
    // small margins look ugly on macOS on modal windows
    #ifndef Q_OS_MACOS
    buttonLayout->setContentsMargins(0, 0, 6, 6);
    #endif
    auto refreshButton = new QPushButton(tr("&Refresh"), this);
    connect(refreshButton, &QPushButton::clicked, this, [this] { pageCast(container->selectedPage())->loadList(); });
    buttonLayout->addWidget(refreshButton);

    buttons->setOrientation(Qt::Horizontal);
    buttons->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Ok"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonLayout->addWidget(buttons);

    container->addButtons(buttonLayout);

    setWindowTitle(dialogTitle());
    setWindowModality(Qt::WindowModal);
    resize(520, 347);

    for (BasePage* page : container->getPages()) {
        if (page->id() == uid)
            container->selectPage(page->id());

        connect(pageCast(page), &VersionSelectWidget::selectedVersionChanged, this, [this, page] {
            if (page->id() == container->selectedPage()->id())
                validate(container->selectedPage());
        });
    }
    connect(container, &PageContainer::selectedPageChanged, this, [this](BasePage* previous, BasePage* current) { validate(current); });
    pageCast(container->selectedPage())->selectSearch();
    validate(container->selectedPage());
}

QList<BasePage*> InstallLoaderDialog::getPages()
{
    return { // NeoForge
             new InstallLoaderPage("net.neoforged", "neoforged", tr("NeoForge"), {}, profile),
             // Forge
             new InstallLoaderPage("net.minecraftforge", "forge", tr("Forge"), {}, profile),
             // Cleanroom (1.12.2 Forge fork)
             new InstallLoaderPage("com.cleanroommc.cleanroom", "cleanroom", tr("Cleanroom"), {}, profile),
             // Fabric
             new InstallLoaderPage("net.fabricmc.fabric-loader", "fabricmc", tr("Fabric"), Version("1.14"), profile),
             // Quilt
             new InstallLoaderPage("org.quiltmc.quilt-loader", "quiltmc", tr("Quilt"), Version("1.14"), profile),
             // LiteLoader
             new InstallLoaderPage("com.mumfrey.liteloader", "liteloader", tr("LiteLoader"), {}, profile),
             // Legacy Fabric and LabyMod use their official profile services
             // instead of the Prism metadata index.
             new InstallLoaderPage("net.legacyfabric", "fabricmc", tr("Legacy Fabric"), {}, profile, "legacyfabric"),
             new InstallLoaderPage("net.labymod", "labymod", tr("LabyMod (Production)"), {}, profile, "labymod-production"),
             new InstallLoaderPage("net.labymod-snapshot", "labymod", tr("LabyMod (Snapshot)"), {}, profile, "labymod-snapshot")
    };
}

QString InstallLoaderDialog::dialogTitle()
{
    return tr("Install Loader");
}

void InstallLoaderDialog::validate(BasePage* page)
{
    buttons->button(QDialogButtonBox::Ok)->setEnabled(pageCast(page)->selectedVersion() != nullptr);
}

void InstallLoaderDialog::done(int result)
{
    if (result == Accepted) {
        auto* page = pageCast(container->selectedPage());
        if (page->selectedVersion()) {
            if (page->isSpecial()) {
                const auto selected = page->selectedSpecialVersion();
                QString error;
                const auto minecraft = profile->getComponentVersion("net.minecraft");
                if (!selected || !SpecialLoader::install(profile->instance(),
                                                         page->specialLoaderId(),
                                                         minecraft,
                                                         selected->loaderVersion(),
                                                         selected->commitRef(),
                                                         &error)) {
                    QMessageBox::critical(this, tr("Install Loader"), error.isEmpty() ? tr("Could not install the loader.") : error);
                    QDialog::done(QDialog::Rejected);
                    return;
                }
            } else {
                profile->setComponentVersion(page->id(), page->selectedVersion()->descriptor());
                profile->resolve(Net::Mode::Online);
            }
        }
    }

    QDialog::done(result);
}
#include "InstallLoaderDialog.moc"
