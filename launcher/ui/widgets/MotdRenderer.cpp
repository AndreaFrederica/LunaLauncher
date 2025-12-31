#include "MotdRenderer.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

QString MotdRenderer::render(const QJsonValue &motd)
{
    return parseComponent(motd);
}

QString MotdRenderer::parseComponent(const QJsonValue &component, const QString &parentStyle)
{
    QString result;
    QString currentStyle = parentStyle;
    QString text;

    if (component.isString()) {
        text = component.toString().toHtmlEscaped();
        // Handle legacy color codes in string (e.g. §a) - simplified for now, just stripping or keeping
        // For a full implementation, we'd need to parse § codes.
        // Let's assume JSON format primarily, or simple string without codes for now.
        // If it contains codes, they will be displayed as is or we can strip them.
        return text; // Basic string return
    } else if (component.isObject()) {
        QJsonObject obj = component.toObject();

        // Styles
        if (obj.contains("color")) {
            currentStyle += QString("color:%1;").arg(mapColorToHex(obj["color"].toString()));
        }
        if (obj.contains("bold") && obj["bold"].toBool()) {
            currentStyle += "font-weight:bold;";
        }
        if (obj.contains("italic") && obj["italic"].toBool()) {
            currentStyle += "font-style:italic;";
        }
        if (obj.contains("underlined") && obj["underlined"].toBool()) {
            currentStyle += "text-decoration:underline;";
        }
        if (obj.contains("strikethrough") && obj["strikethrough"].toBool()) {
            currentStyle += "text-decoration:line-through;";
        }

        // Text
        if (obj.contains("text")) {
            text = obj["text"].toString().toHtmlEscaped();
        }

        // Apply style
        if (!text.isEmpty()) {
            if (!currentStyle.isEmpty()) {
                result += QString("<span style=\"%1\">%2</span>").arg(currentStyle, text);
            } else {
                result += text;
            }
        }

        // Extra
        if (obj.contains("extra")) {
            QJsonArray extra = obj["extra"].toArray();
            for (const auto &val : extra) {
                result += parseComponent(val, currentStyle); // Inherit style? Usually style resets or cascades.
                // In MC JSON, extra inherits from parent.
            }
        }
    } else if (component.isArray()) {
        QJsonArray arr = component.toArray();
        for (const auto &val : arr) {
            result += parseComponent(val, currentStyle);
        }
    }

    return result;
}

QString MotdRenderer::mapColorToHex(const QString &colorName)
{
    static const QMap<QString, QString> colorMap = {
        {"black", "#000000"},
        {"dark_blue", "#0000AA"},
        {"dark_green", "#00AA00"},
        {"dark_aqua", "#00AAAA"},
        {"dark_red", "#AA0000"},
        {"dark_purple", "#AA00AA"},
        {"gold", "#FFAA00"},
        {"gray", "#AAAAAA"},
        {"dark_gray", "#555555"},
        {"blue", "#5555FF"},
        {"green", "#55FF55"},
        {"aqua", "#55FFFF"},
        {"red", "#FF5555"},
        {"light_purple", "#FF55FF"},
        {"yellow", "#FFFF55"},
        {"white", "#FFFFFF"}
    };

    if (colorName.startsWith("#")) return colorName; // Hex code
    return colorMap.value(colorName, "#AAAAAA"); // Default gray
}
