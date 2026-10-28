// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
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

#include "ModPreflightDialog.h"

#include <QBrush>
#include <QColor>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QIcon>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QPolygonF>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

static QIcon iconForSeverity(ProblemSeverity s)
{
    switch (s) {
        case ProblemSeverity::Error:
            return QIcon::fromTheme("status-bad");
        case ProblemSeverity::Warning:
            return QIcon::fromTheme("status-yellow");
        default:
            return QIcon::fromTheme("status-good");
    }
}

static QColor colorForSeverity(ProblemSeverity s, bool dark)
{
    switch (s) {
        case ProblemSeverity::Error:
            return dark ? QColor(255, 120, 120) : QColor(180, 30, 30);
        case ProblemSeverity::Warning:
            return dark ? QColor(240, 200, 100) : QColor(170, 110, 0);
        default:
            return dark ? QColor(140, 220, 140) : QColor(40, 130, 40);
    }
}

static const char* kindLabel(DepKind k)
{
    switch (k) {
        case DepKind::Required:
            return "required";
        case DepKind::Optional:
            return "optional";
        case DepKind::Recommended:
            return "recommended";
        case DepKind::Suggested:
            return "suggested";
        case DepKind::Conflict:
            return "conflict";
        case DepKind::Break:
            return "break";
        case DepKind::Incompatible:
            return "incompatible";
        case DepKind::Discouraged:
            return "discouraged";
        case DepKind::Embedded:
            return "embedded";
    }
    return "?";
}

ModPreflightDialog::ModPreflightDialog(QWidget* parent, ModPreflightTask::ResultPtr result) : QDialog(parent), m_result(result)
{
    setWindowTitle(tr("Validate Mods"));
    setMinimumSize(680, 480);

    auto* layout = new QVBoxLayout(this);

    m_summary = new QLabel(this);
    layout->addWidget(m_summary);

    auto* tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    m_issuesTree = new QTreeWidget(this);
    m_issuesTree->setHeaderLabels({ tr("Severity"), tr("Issue") });
    m_issuesTree->setColumnWidth(0, 90);
    m_issuesTree->setAlternatingRowColors(true);
    m_issuesTree->setWordWrap(true);
    m_issuesTree->setRootIsDecorated(true);
    tabs->addTab(m_issuesTree, tr("Issues (%1)").arg(m_result->issues.size()));

    m_depsTree = new QTreeWidget(this);
    m_depsTree->setHeaderLabels({ tr("Mod / Dependency"), tr("Range"), tr("Kind") });
    m_depsTree->setAlternatingRowColors(true);
    tabs->addTab(m_depsTree, tr("Dependency Tree"));

    m_graphView = new QGraphicsView(this);
    m_graphView->setRenderHint(QPainter::Antialiasing);
    m_graphView->setDragMode(QGraphicsView::ScrollHandDrag);
    tabs->addTab(m_graphView, tr("Dependency Graph"));

    auto* buttons = new QDialogButtonBox(this);
    auto* exportBtn = buttons->addButton(tr("Export Graph (.dot)..."), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(exportBtn, &QPushButton::clicked, this, &ModPreflightDialog::exportDependencyGraph);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);

    populateIssues();
    populateDependencyTree();
    populateDependencyGraph();
}

void ModPreflightDialog::populateIssues()
{
    bool dark = QGuiApplication::palette().color(QPalette::Window).valueF() < 0.5f;

    if (m_result->issues.isEmpty()) {
        auto* item = new QTreeWidgetItem({ tr("OK"), tr("No problems found. Mods look good to launch.") });
        item->setIcon(0, QIcon::fromTheme("status-good"));
        item->setForeground(1, colorForSeverity(ProblemSeverity::None, dark));
        m_issuesTree->addTopLevelItem(item);
        m_summary->setText(tr("<b>No problems found.</b> %1 mod(s) checked.").arg(m_result->installedMods.size()));
        return;
    }

    for (const auto& issue : m_result->issues) {
        QString sevText;
        switch (issue.severity) {
            case ProblemSeverity::Error:
                sevText = tr("Error");
                break;
            case ProblemSeverity::Warning:
                sevText = tr("Warning");
                break;
            default:
                sevText = tr("Info");
        }
        auto* top = new QTreeWidgetItem({ sevText, issue.message });
        top->setIcon(0, iconForSeverity(issue.severity));
        QColor c = colorForSeverity(issue.severity, dark);
        top->setForeground(0, c);
        top->setForeground(1, c);

        auto* codeItem = new QTreeWidgetItem({ "", tr("Code: %1").arg(issue.code) });
        if (!issue.fromModId.isEmpty()) {
            auto* fromItem = new QTreeWidgetItem({ "", tr("Mod: %1").arg(issue.fromModId) });
            top->addChild(fromItem);
        }
        top->addChild(codeItem);
        if (!issue.related.isEmpty()) {
            auto* relItem = new QTreeWidgetItem({ "", tr("Related: %1").arg(issue.related.join(", ")) });
            top->addChild(relItem);
        }
        m_issuesTree->addTopLevelItem(top);
        m_issuesTree->expandItem(top);
    }

    m_summary->setText(tr("<b>%1 error(s), %2 warning(s)</b> across %3 mod(s).")
                           .arg(m_result->errorCount)
                           .arg(m_result->warningCount)
                           .arg(m_result->installedMods.size()));
}

void ModPreflightDialog::populateDependencyTree()
{
    bool dark = QGuiApplication::palette().color(QPalette::Window).valueF() < 0.5f;

    // Group dependency edges by their declaring mod, then add each installed mod
    // as a top-level node with its declared dependencies as children.
    QHash<QString, QList<ModDependencyEdge>> edgesByFrom;
    for (const auto& pair : m_result->depEdges)
        edgesByFrom[pair.first].append(pair.second);

    for (const auto& mod : m_result->installedMods) {
        QStringList topText = { mod.modId, mod.version, mod.environment };
        auto* top = new QTreeWidgetItem(topText);
        top->setIcon(0, QIcon::fromTheme("jar"));

        const auto edges = edgesByFrom.value(mod.modId);
        for (const auto& e : edges) {
            QString range = e.versionRange.isEmpty() ? "*" : e.versionRange;
            auto* child = new QTreeWidgetItem({ QStringLiteral("→ %1").arg(e.toModId), range, QString::fromLatin1(kindLabel(e.kind)) });
            if (e.kind == DepKind::Break || e.kind == DepKind::Incompatible || e.kind == DepKind::Conflict)
                child->setForeground(2, colorForSeverity(ProblemSeverity::Error, dark));
            else if (e.kind == DepKind::Recommended || e.kind == DepKind::Suggested || e.kind == DepKind::Optional)
                child->setForeground(2, colorForSeverity(ProblemSeverity::Warning, dark));
            top->addChild(child);
        }
        m_depsTree->addTopLevelItem(top);
    }

    m_depsTree->expandAll();
    m_depsTree->resizeColumnToContents(0);
    m_depsTree->resizeColumnToContents(1);
}

// Graphviz DOT identifier escaping (mirrors the Python demo's dot_id/dot_quote).
static QString dotId(QString s)
{
    s.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), "_");
    return s;
}

static QString dotQuote(const QString& s)
{
    return "\"" + QString(s).replace('\\', "\\\\").replace('"', "\\\"") + "\"";
}

QString ModPreflightDialog::buildDotDocument() const
{
    // Group edges by declaring mod.
    QHash<QString, QList<ModDependencyEdge>> edgesByFrom;
    QSet<QString> allNodeIds;
    QHash<QString, QString> nodeLabel;  // id -> "name\nversion"
    for (const auto& mod : m_result->installedMods) {
        allNodeIds.insert(mod.modId);
        nodeLabel.insert(mod.modId, mod.modId + "\\n" + (mod.version.isEmpty() ? "?" : mod.version));
    }
    for (const auto& pair : m_result->depEdges) {
        edgesByFrom[pair.first].append(pair.second);
        allNodeIds.insert(pair.second.toModId);
        // Virtual nodes (minecraft, java, ...) get a plain label.
        if (!nodeLabel.contains(pair.second.toModId))
            nodeLabel.insert(pair.second.toModId, pair.second.toModId);
    }

    QString dot;
    dot += "digraph mods {\n";
    dot += "  rankdir=LR;\n";
    dot += "  node [shape=box];\n";
    for (const QString& id : allNodeIds)
        dot += "  " + dotId(id) + " [label=" + dotQuote(nodeLabel.value(id)) + "];\n";

    for (const auto& mod : m_result->installedMods) {
        const auto edges = edgesByFrom.value(mod.modId);
        for (const auto& e : edges) {
            QString range = e.versionRange.isEmpty() ? "*" : e.versionRange;
            QString style = "solid";
            QString color = "black";
            // Visual encoding: required=solid, optional/recommended/suggested=dashed,
            // break/incompatible/conflict=bold red.
            if (e.kind == DepKind::Required) {
                style = "solid";
            } else if (e.kind == DepKind::Optional || e.kind == DepKind::Recommended || e.kind == DepKind::Suggested ||
                       e.kind == DepKind::Embedded) {
                style = "dashed";
            } else if (e.kind == DepKind::Break || e.kind == DepKind::Incompatible || e.kind == DepKind::Conflict ||
                       e.kind == DepKind::Discouraged) {
                style = "bold";
                color = "#cc0000";
            }
            dot += "  " + dotId(mod.modId) + " -> " + dotId(e.toModId) + " [label=" + dotQuote(kindLabel(e.kind) + QString(" ") + range) +
                   ", style=" + style + ", color=\"" + color + "\"];\n";
        }
    }
    dot += "}\n";
    return dot;
}

void ModPreflightDialog::exportDependencyGraph()
{
    QString defaultPath = QDir::homePath() + "/mod-dependencies.dot";
    QString path = QFileDialog::getSaveFileName(this, tr("Export Dependency Graph"), defaultPath, tr("Graphviz DOT (*.dot)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write to %1.").arg(path));
        return;
    }
    f.write(buildDotDocument().toUtf8());
    f.close();

    QMessageBox::information(
        this, tr("Graph Exported"),
        tr("Dependency graph written to:\n%1\n\nRender it with Graphviz, e.g.:\n dot -Tpng \"%1\" -o deps.png").arg(path));
}

void ModPreflightDialog::populateDependencyGraph()
{
    bool dark = QGuiApplication::palette().color(QPalette::Window).valueF() < 0.5f;
    auto* scene = new QGraphicsScene(this);

    QHash<QString, QList<ModDependencyEdge>> edgesByFrom;
    QSet<QString> nodeIds;
    for (const auto& mod : m_result->installedMods)
        nodeIds.insert(mod.modId);
    for (const auto& pair : m_result->depEdges) {
        edgesByFrom[pair.first].append(pair.second);
        nodeIds.insert(pair.second.toModId);
    }

    // Layout: arrange installed mods on a grid (left columns), virtual targets
    // (minecraft/java/...) on the far right. Simple but readable for the typical
    // mod counts in an instance.
    QList<QString> installed;
    QList<QString> virtualNodes;
    for (const auto& mod : m_result->installedMods)
        installed.append(mod.modId);
    QSet<QString> installedSet(installed.begin(), installed.end());
    for (const QString& id : nodeIds) {
        if (!installedSet.contains(id))
            virtualNodes.append(id);
    }
    std::sort(virtualNodes.begin(), virtualNodes.end());

    const qreal cellW = 200.0;
    const qreal cellH = 80.0;
    const qreal ellipseW = 140.0;
    const qreal ellipseH = 46.0;

    QHash<QString, QPointF> nodePos;

    int n = installed.size();
    int cols = n > 6 ? 3 : (n > 0 ? qMin(n, 2) : 1);
    for (int i = 0; i < installed.size(); ++i) {
        int r = i / cols;
        int c = i % cols;
        nodePos.insert(installed[i], QPointF(c * cellW, r * cellH));
    }
    // Virtual nodes in a column to the right of the grid.
    qreal virtualX = (cols + 1) * cellW;
    for (int i = 0; i < virtualNodes.size(); ++i) {
        nodePos.insert(virtualNodes[i], QPointF(virtualX, i * cellH));
    }

    // Draw nodes.
    QBrush installedBrush(dark ? QColor(60, 90, 140) : QColor(210, 225, 245));
    QBrush virtualBrush(dark ? QColor(90, 90, 90) : QColor(225, 225, 225));
    QPen nodePen(dark ? QColor(200, 200, 200) : QColor(60, 60, 60));
    auto drawNode = [&](const QString& id, const QString& version, bool isVirtual) {
        QPointF p = nodePos.value(id);
        auto* item = new QGraphicsEllipseItem(p.x() - ellipseW / 2, p.y() - ellipseH / 2, ellipseW, ellipseH);
        item->setBrush(isVirtual ? virtualBrush : installedBrush);
        item->setPen(nodePen);
        auto* label = scene->addText(id + (version.isEmpty() ? QString() : "\n" + version));
        label->setDefaultTextColor(dark ? Qt::white : Qt::black);
        label->setPos(p.x() - label->boundingRect().width() / 2, p.y() - label->boundingRect().height() / 2);
        scene->addItem(item);
    };
    for (const auto& mod : m_result->installedMods)
        drawNode(mod.modId, mod.version, false);
    for (const QString& id : virtualNodes)
        drawNode(id, QString(), true);

    // Draw edges.
    for (const auto& mod : m_result->installedMods) {
        QPointF from = nodePos.value(mod.modId);
        for (const auto& e : edgesByFrom.value(mod.modId)) {
            QPointF to = nodePos.value(e.toModId);
            if (from.isNull() || to.isNull())
                continue;
            QPen pen(dark ? QColor(180, 180, 180) : QColor(80, 80, 80), 1.5);
            if (e.kind == DepKind::Break || e.kind == DepKind::Incompatible || e.kind == DepKind::Conflict || e.kind == DepKind::Discouraged) {
                pen.setColor(QColor(200, 40, 40));
                pen.setWidth(2.0);
                pen.setStyle(Qt::SolidLine);
            } else if (e.kind == DepKind::Optional || e.kind == DepKind::Recommended || e.kind == DepKind::Suggested ||
                       e.kind == DepKind::Embedded) {
                pen.setStyle(Qt::DashLine);
            }
            auto* line = scene->addLine(QLineF(from, to), pen);
            (void)line;
            // Arrowhead: a small triangle at the target node.
            qreal dx = to.x() - from.x();
            qreal dy = to.y() - from.y();
            qreal len = std::sqrt(dx * dx + dy * dy);
            if (len > 1.0) {
                qreal ux = dx / len;
                qreal uy = dy / len;
                QPointF tip(to.x() - ux * ellipseW / 2, to.y() - uy * ellipseH / 2);
                QPointF base(tip.x() - ux * 10, tip.y() - uy * 10);
                QPointF left(base.x() - uy * 5, base.y() + ux * 5);
                QPointF right(base.x() + uy * 5, base.y() - ux * 5);
                QPolygonF head;
                head << tip << left << right;
                auto* arrow = new QGraphicsPolygonItem(head, nullptr);
                arrow->setBrush(pen.color());
                arrow->setPen(Qt::NoPen);
                scene->addItem(arrow);
            }
        }
    }

    m_graphView->setScene(scene);
}
