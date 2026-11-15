// SPDX-License-Identifier: GPL-3.0-only
#include "ExternalProvenancePage.h"

#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "VanillaInstance.h"

ExternalProvenancePage::ExternalProvenancePage(BaseInstance* instance, QWidget* parent)
    : QWidget(parent), m_instance(instance)
{
    auto* layout = new QVBoxLayout(this);
    const auto* vanilla = dynamic_cast<const VanillaInstance*>(m_instance);
    if (!vanilla) {
        layout->addWidget(new QLabel(tr("此实例不是外部原版格式实例。"), this));
        return;
    }
    const auto& provenance = vanilla->descriptor().provenance;
    auto* summary = new QLabel(this);
    summary->setWordWrap(true);
    summary->setText(provenance.mixed
                         ? tr("检测到多个启动器或目录格式的修改痕迹。Luna 保留所有来源证据，不会因为主来源字段覆盖其他来源。")
                         : tr("以下来源由目录结构和版本 JSON 证据推断。置信度不是安全认证，只用于解释识别结果。"));
    layout->addWidget(summary);

    auto* tree = new QTreeWidget(this);
    tree->setColumnCount(4);
    tree->setHeaderLabels({ tr("来源"), tr("置信度"), tr("命中证据"), tr("能力") });
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(false);
    for (const auto& provider : provenance.providers) {
        auto* item = new QTreeWidgetItem(tree);
        item->setText(0, provider.displayName.isEmpty() ? provider.providerId : provider.displayName);
        item->setText(1, QStringLiteral("%1%").arg(provider.confidence));
        auto evidence = provider.matchedPaths;
        evidence.append(provider.matchedFields);
        item->setText(2, evidence.isEmpty() ? tr("标准版本 JSON") : evidence.join(QStringLiteral(", ")));
        QStringList capabilities;
        if (provider.canReadInstanceSettings) capabilities.append(tr("可读取设置"));
        if (provider.canEditCanonicalMetadata) capabilities.append(tr("可编辑规范元数据"));
        if (capabilities.isEmpty()) capabilities.append(tr("仅检测"));
        item->setText(3, capabilities.join(QStringLiteral("、")));
    }
    tree->header()->setStretchLastSection(true);
    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
    layout->addWidget(tree, 1);
}

QIcon ExternalProvenancePage::icon() const
{
    return QIcon::fromTheme(QStringLiteral("dialog-information"));
}

bool ExternalProvenancePage::shouldDisplay() const
{
    return dynamic_cast<const VanillaInstance*>(m_instance) != nullptr;
}

