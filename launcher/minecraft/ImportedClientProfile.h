// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QJsonObject>
#include <QString>

namespace ImportedClientProfile {

// Local bytes are authoritative. A profile checksum is evidence, not an
// instruction to replace a modified client with the official download.
QJsonObject inspectClient(const QString& path, const QJsonObject& profile, QString* error = nullptr);
bool verifyClient(const QString& path, const QJsonObject& record, QString* error = nullptr);
QJsonObject localMainJar(const QJsonObject& record, const QString& filename);
void inferJavaRequirement(QJsonObject& profile, const QString& librariesRoot, const QString& clientJar);

} // namespace ImportedClientProfile
