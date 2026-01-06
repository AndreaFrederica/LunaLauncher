/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#include "ServerModLoaderPage.h"
#include "server/ServerInstance.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>

ServerModLoaderPage::ServerModLoaderPage(ServerInstance *instance, QWidget *parent)
    : QWidget(parent), m_instance(instance)
{
    auto layout = new QVBoxLayout(this);

    // Info label
    auto infoLabel = new QLabel(
        tr("Configure the mod loader types and Minecraft version for this server instance.\n"
           "This information will be used when downloading mods from online platforms."), this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Form for configuration
    auto form = new QFormLayout();

    // Minecraft Version
    m_minecraftVersionEdit = new QLineEdit(this);
    m_minecraftVersionEdit->setPlaceholderText(tr("e.g., 1.20.1"));
    form->addRow(tr("Minecraft Version:"), m_minecraftVersionEdit);

    // ModLoader checkboxes
    auto loadersGroup = new QGroupBox(tr("Mod Loaders"), this);
    auto loadersLayout = new QVBoxLayout(loadersGroup);

    m_neoforgeCheck = new QCheckBox(tr("NeoForge"), this);
    m_forgeCheck = new QCheckBox(tr("Forge"), this);
    m_fabricCheck = new QCheckBox(tr("Fabric"), this);
    m_quiltCheck = new QCheckBox(tr("Quilt"), this);

    loadersLayout->addWidget(m_neoforgeCheck);
    loadersLayout->addWidget(m_forgeCheck);
    loadersLayout->addWidget(m_fabricCheck);
    loadersLayout->addWidget(m_quiltCheck);

    layout->addLayout(form);
    layout->addWidget(loadersGroup);

    // Plugin loaders (for future plugin download system)
    auto pluginLoadersGroup = new QGroupBox(tr("Plugin Loaders (For Future Plugin Downloads)"), this);
    auto pluginLoadersLayout = new QVBoxLayout(pluginLoadersGroup);

    m_paperCheck = new QCheckBox(tr("Paper"), this);
    m_spigotCheck = new QCheckBox(tr("Spigot"), this);
    m_bukkitCheck = new QCheckBox(tr("Bukkit"), this);
    m_purpurCheck = new QCheckBox(tr("Purpur"), this);
    m_spongeCheck = new QCheckBox(tr("Sponge"), this);
    m_velocityCheck = new QCheckBox(tr("Velocity (Proxy)"), this);
    m_waterfallCheck = new QCheckBox(tr("Waterfall (Proxy)"), this);
    m_bungeecordCheck = new QCheckBox(tr("BungeeCord (Proxy)"), this);

    pluginLoadersLayout->addWidget(m_paperCheck);
    pluginLoadersLayout->addWidget(m_spigotCheck);
    pluginLoadersLayout->addWidget(m_bukkitCheck);
    pluginLoadersLayout->addWidget(m_purpurCheck);
    pluginLoadersLayout->addWidget(m_spongeCheck);
    pluginLoadersLayout->addWidget(m_velocityCheck);
    pluginLoadersLayout->addWidget(m_waterfallCheck);
    pluginLoadersLayout->addWidget(m_bungeecordCheck);

    layout->addWidget(pluginLoadersGroup);

    // Summary group
    m_summaryGroup = new QGroupBox(tr("Configuration Summary"), this);
    auto summaryLayout = new QVBoxLayout(m_summaryGroup);
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(m_summaryGroup);

    layout->addStretch();

    // Connect signals for live update
    connect(m_minecraftVersionEdit, &QLineEdit::textChanged, this, &ServerModLoaderPage::updateSummary);
    connect(m_neoforgeCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_forgeCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_fabricCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_quiltCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);

    connect(m_paperCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_spigotCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_bukkitCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_purpurCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_spongeCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_velocityCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_waterfallCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);
    connect(m_bungeecordCheck, &QCheckBox::toggled, this, &ServerModLoaderPage::updateSummary);

    load();
}

ServerModLoaderPage::~ServerModLoaderPage() {}

QString ServerModLoaderPage::displayName() const { return tr("Mod Loader"); }

QIcon ServerModLoaderPage::icon() const { return QIcon::fromTheme("settings"); }

QString ServerModLoaderPage::id() const { return "modloader"; }

QString ServerModLoaderPage::helpPage() const { return "Server-Mod-Loader"; }

bool ServerModLoaderPage::shouldDisplay() const { return true; }

bool ServerModLoaderPage::apply()
{
    // Save Minecraft version
    m_instance->setMinecraftVersion(m_minecraftVersionEdit->text());

    // Save mod loader types
    ModPlatform::ModLoaderTypes types = ModPlatform::ModLoaderTypes();
    if (m_neoforgeCheck->isChecked())
        types |= ModPlatform::NeoForge;
    if (m_forgeCheck->isChecked())
        types |= ModPlatform::Forge;
    if (m_fabricCheck->isChecked())
        types |= ModPlatform::Fabric;
    if (m_quiltCheck->isChecked())
        types |= ModPlatform::Quilt;

    m_instance->setModLoaderTypes(types);

    // Save plugin loader types
    ModPlatform::PluginLoaderTypes pluginTypes = ModPlatform::PluginLoaderTypes();
    if (m_paperCheck->isChecked())
        pluginTypes |= ModPlatform::Paper;
    if (m_spigotCheck->isChecked())
        pluginTypes |= ModPlatform::Spigot;
    if (m_bukkitCheck->isChecked())
        pluginTypes |= ModPlatform::Bukkit;
    if (m_purpurCheck->isChecked())
        pluginTypes |= ModPlatform::Purpur;
    if (m_spongeCheck->isChecked())
        pluginTypes |= ModPlatform::Sponge;
    if (m_velocityCheck->isChecked())
        pluginTypes |= ModPlatform::Velocity;
    if (m_waterfallCheck->isChecked())
        pluginTypes |= ModPlatform::Waterfall;
    if (m_bungeecordCheck->isChecked())
        pluginTypes |= ModPlatform::BungeeCord;

    m_instance->setPluginLoaderTypes(pluginTypes);

    return true;
}

void ServerModLoaderPage::load()
{
    m_minecraftVersionEdit->setText(m_instance->getMinecraftVersion());

    // Load mod loader types
    auto types = m_instance->getModLoaderTypes();
    m_neoforgeCheck->setChecked(types & ModPlatform::NeoForge);
    m_forgeCheck->setChecked(types & ModPlatform::Forge);
    m_fabricCheck->setChecked(types & ModPlatform::Fabric);
    m_quiltCheck->setChecked(types & ModPlatform::Quilt);

    // Load plugin loader types
    auto pluginTypes = m_instance->getPluginLoaderTypes();
    m_paperCheck->setChecked(pluginTypes & ModPlatform::Paper);
    m_spigotCheck->setChecked(pluginTypes & ModPlatform::Spigot);
    m_bukkitCheck->setChecked(pluginTypes & ModPlatform::Bukkit);
    m_purpurCheck->setChecked(pluginTypes & ModPlatform::Purpur);
    m_spongeCheck->setChecked(pluginTypes & ModPlatform::Sponge);
    m_velocityCheck->setChecked(pluginTypes & ModPlatform::Velocity);
    m_waterfallCheck->setChecked(pluginTypes & ModPlatform::Waterfall);
    m_bungeecordCheck->setChecked(pluginTypes & ModPlatform::BungeeCord);

    updateSummary();
}

void ServerModLoaderPage::updateSummary()
{
    QStringList loaders;
    if (m_neoforgeCheck->isChecked())
        loaders << "NeoForge";
    if (m_forgeCheck->isChecked())
        loaders << "Forge";
    if (m_fabricCheck->isChecked())
        loaders << "Fabric";
    if (m_quiltCheck->isChecked())
        loaders << "Quilt";

    QStringList pluginLoaders;
    if (m_paperCheck->isChecked())
        pluginLoaders << "Paper";
    if (m_spigotCheck->isChecked())
        pluginLoaders << "Spigot";
    if (m_bukkitCheck->isChecked())
        pluginLoaders << "Bukkit";
    if (m_purpurCheck->isChecked())
        pluginLoaders << "Purpur";
    if (m_spongeCheck->isChecked())
        pluginLoaders << "Sponge";
    if (m_velocityCheck->isChecked())
        pluginLoaders << "Velocity";
    if (m_waterfallCheck->isChecked())
        pluginLoaders << "Waterfall";
    if (m_bungeecordCheck->isChecked())
        pluginLoaders << "BungeeCord";

    QString version = m_minecraftVersionEdit->text();
    if (version.isEmpty())
        version = tr("(not set)");

    QString summary;
    QStringList summaryParts;

    summaryParts << tr("Minecraft Version: %1").arg(version);

    if (!loaders.isEmpty()) {
        summaryParts << tr("✓ Mod Loaders: %1").arg(loaders.join(", "));
    } else {
        summaryParts << tr("⚠ No mod loaders selected");
    }

    if (!pluginLoaders.isEmpty()) {
        summaryParts << tr("✓ Plugin Loaders: %1 (for future plugin downloads)").arg(pluginLoaders.join(", "));
    }

    summary = summaryParts.join("\n");

    if (loaders.isEmpty() && pluginLoaders.isEmpty()) {
        summary += tr("\n\n⚠ You should select at least one mod loader or plugin loader to enable downloading content.");
    }

    m_summaryLabel->setText(summary);
}
