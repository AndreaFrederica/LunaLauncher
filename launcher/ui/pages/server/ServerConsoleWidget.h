/*
 *  Luna Launcher - Minecraft Launcher
 *  Copyright (C) 2025 AndreaFrederica <andreafrederica@outlook.com>
*/
#pragma once
#include <QWidget>

class QTermWidget;

class ServerConsoleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ServerConsoleWidget(QWidget *parent = nullptr);
    virtual ~ServerConsoleWidget();

    void writeData(const QByteArray &data);
    int columns() const;
    int lines() const;
    void clear();

signals:
    void sendData(const char *data, int size);
    void termSizeChange(int lines, int columns);

private:
    QTermWidget *m_console;
};
