#pragma once

#include <QDialog>
#include <QObject>
#include <QStringList>
#include <QWidget>
#include <optional>

namespace Ui {
class UntrustedModsDialog;
}

class UntrustedModsDialog : public QDialog {
    Q_OBJECT
   public:
    // Routes headless imports through CLI/MCP/Neo UI interaction without constructing a QWidget.
    // nullopt means no explicit choice was available (including non-interactive clients).
    static std::optional<bool> confirm(const QStringList& paths, QWidget* parent = nullptr);
    explicit UntrustedModsDialog(const QStringList& paths, QWidget* parent = nullptr);
    ~UntrustedModsDialog() override;

   private:
    Ui::UntrustedModsDialog* m_ui;
};
