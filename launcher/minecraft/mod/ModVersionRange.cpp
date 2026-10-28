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
 *
 * Ported from R:/mc_mod_preflight_demo/mc_mod_preflight.py (lines ~300-494).
 */

#include "ModVersionRange.h"

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariant>
#include <QtGlobal>

namespace ModVersionRange {

namespace {

const QRegularExpression kVersionPartRe(QStringLiteral("(\\d+)|([^.+\\-]+)"));

// A token is either an int (numeric segment) or a lowercased string segment.
struct Token {
    bool isInt;
    qint64 i = 0;
    QString s;
};

// Python: version_tokens() — split into numeric vs textual tokens.
QList<Token> versionTokens(QString v)
{
    v = v.trimmed();
    if (v.startsWith('v') || v.startsWith('V'))
        v = v.mid(1);

    QList<Token> out;
    auto it = kVersionPartRe.globalMatch(v);
    while (it.hasNext()) {
        auto m = it.next();
        QString numPart = m.captured(1);
        QString strPart = m.captured(2);
        if (!numPart.isEmpty()) {
            Token t;
            t.isInt = true;
            t.i = numPart.toLongLong();
            out.append(t);
        } else if (!strPart.isEmpty()) {
            Token t;
            t.isInt = false;
            t.s = strPart.toLower();
            out.append(t);
        }
    }
    if (out.isEmpty()) {
        Token t;
        t.isInt = true;
        t.i = 0;
        out.append(t);
    }
    return out;
}

// Python: bump_for_caret()
QString bumpForCaret(const QString& base)
{
    QRegularExpression numRe(QStringLiteral("\\d+"));
    QList<qint64> nums;
    auto it = numRe.globalMatch(base);
    while (it.hasNext() && nums.size() < 3) {
        nums.append(it.next().captured(0).toLongLong());
    }
    while (nums.size() < 3)
        nums.append(0);
    qint64 major = nums[0], minor = nums[1], patch = nums[2];
    if (major > 0)
        return QString::number(major + 1) + ".0.0";
    if (minor > 0)
        return "0." + QString::number(minor + 1) + ".0";
    return "0.0." + QString::number(patch + 1);
}

// Python: bump_for_tilde()
QString bumpForTilde(const QString& base)
{
    QRegularExpression numRe(QStringLiteral("\\d+"));
    QList<qint64> nums;
    auto it = numRe.globalMatch(base);
    while (it.hasNext())
        nums.append(it.next().captured(0).toLongLong());
    if (nums.size() <= 1)
        return nums.isEmpty() ? QStringLiteral("1.0.0") : (QString::number(nums[0] + 1) + ".0.0");
    qint64 major = nums[0], minor = nums[1];
    return QString::number(major) + "." + QString::number(minor + 1) + ".0";
}

// Python: exact_or_prefix_match()
bool exactOrPrefixMatch(const QString& version, const QString& expected)
{
    QString e = expected.trimmed();
    if (e.isEmpty() || e == "*")
        return true;
    if (e.contains('*') || e.contains('x') || e.contains('X')) {
        auto parts = e.split(QRegularExpression(QStringLiteral("[.+-]")), Qt::SkipEmptyParts);
        auto vparts = version.split(QRegularExpression(QStringLiteral("[.+-]")), Qt::SkipEmptyParts);
        for (int i = 0; i < parts.size(); ++i) {
            const auto& p = parts[i];
            if (p == "*" || p == "x" || p == "X" || p.isEmpty())
                return true;
            if (i >= vparts.size() || p != vparts[i])
                return false;
        }
        return true;
    }
    return compareVersions(version, e) == 0;
}

// Python: satisfies_fabric_atom()
bool satisfiesFabricAtom(const QString& versionIn, const QString& atomIn)
{
    QString atom = atomIn.trimmed();
    QString version = versionIn;
    if (atom.isEmpty() || atom == "*")
        return true;
    if (atom.startsWith('^')) {
        QString base = atom.mid(1).trimmed();
        return compareVersions(version, base) >= 0 && compareVersions(version, bumpForCaret(base)) < 0;
    }
    if (atom.startsWith('~')) {
        QString base = atom.mid(1).trimmed();
        return compareVersions(version, base) >= 0 && compareVersions(version, bumpForTilde(base)) < 0;
    }
    if (atom.contains('*') || atom.contains('x') || atom.contains('X'))
        return exactOrPrefixMatch(version, atom);

    QRegularExpression mRe(QStringLiteral("(>=|<=|>|<|=|==)?\\s*(.+)"));
    auto m = mRe.match(atom);
    if (!m.hasMatch())
        return exactOrPrefixMatch(version, atom);
    QString op = m.captured(1);
    if (op.isEmpty())
        op = "=";
    QString rhs = m.captured(2).trimmed();
    int cmp = compareVersions(version, rhs);
    if (op == "=" || op == "==")
        return cmp == 0;
    if (op == ">=")
        return cmp >= 0;
    if (op == "<=")
        return cmp <= 0;
    if (op == ">")
        return cmp > 0;
    if (op == "<")
        return cmp < 0;
    return false;
}

// Python: split_maven_ranges()
QStringList splitMavenRanges(const QString& rangeText)
{
    QString text = rangeText.trimmed();
    QStringList ranges;
    int i = 0;
    int n = text.size();
    while (i < n) {
        QChar ch = text[i];
        if (ch != '[' && ch != '(') {
            int j = i + 1;
            while (j < n && text[j] != '[' && text[j] != '(')
                ++j;
            QString token = text.mid(i, j - i).trimmed();
            token.remove(',');
            if (!token.isEmpty())
                ranges.append(token);
            i = j;
            continue;
        }
        int start = i;
        QChar endChar = (ch == '[') ? ']' : ')';
        ++i;
        int depth = 1;
        while (i < n) {
            if (text[i] == endChar) {
                --depth;
                if (depth == 0) {
                    ++i;
                    break;
                }
            }
            ++i;
        }
        ranges.append(text.mid(start, i - start).trimmed());
        while (i < n && (text[i] == ',' || text[i] == ' '))
            ++i;
    }
    if (ranges.isEmpty())
        ranges.append(text);
    return ranges;
}

}  // namespace

int compareVersions(const QString& aIn, const QString& bIn)
{
    auto ta = versionTokens(aIn);
    auto tb = versionTokens(bIn);
    int n = qMax(ta.size(), tb.size());
    // Python fills missing positions with the literal integer 0.
    Token zero;
    zero.isInt = true;
    zero.i = 0;
    for (int i = 0; i < n; ++i) {
        const Token& aRef = i < ta.size() ? ta[i] : zero;
        const Token& bRef = i < tb.size() ? tb[i] : zero;

        if (aRef.isInt && bRef.isInt) {
            if (aRef.i != bRef.i)
                return aRef.i < bRef.i ? -1 : 1;
            continue;
        }
        if (aRef.isInt && !bRef.isInt)
            return 1;  // numeric segment is newer than textual pre-release
        if (!aRef.isInt && bRef.isInt)
            return -1;
        // both strings
        int c = aRef.s.compare(bRef.s);
        if (c != 0)
            return c < 0 ? -1 : 1;
    }
    return 0;
}

bool satisfiesMavenRange(const QString& versionIn, const QString& rangeIn)
{
    if (rangeIn.isEmpty())
        return true;
    QString text = rangeIn.trimmed();
    if (text.isEmpty() || text == "*" || text.compare("none", Qt::CaseInsensitive) == 0)
        return true;

    for (const QString& r : splitMavenRanges(text)) {
        if (r.isEmpty())
            continue;
        QChar first = r.isEmpty() ? QChar() : r.at(0);
        QChar last = r.at(r.size() - 1);
        if ((first == '[' || first == '(') && (last == ']' || last == ')')) {
            bool includeLow = first == '[';
            bool includeHigh = last == ']';
            QString body = r.mid(1, r.size() - 2).trimmed();
            if (!body.contains(',')) {
                if (body.isEmpty())
                    continue;
                if (compareVersions(versionIn, body) == 0)
                    return true;
                continue;
            }
            int comma = body.indexOf(',');
            QString low = body.left(comma).trimmed();
            QString high = body.mid(comma + 1).trimmed();
            bool ok = true;
            if (!low.isEmpty()) {
                int cmpLow = compareVersions(versionIn, low);
                ok = ok && (includeLow ? cmpLow >= 0 : cmpLow > 0);
            }
            if (!high.isEmpty()) {
                int cmpHigh = compareVersions(versionIn, high);
                ok = ok && (includeHigh ? cmpHigh <= 0 : cmpHigh < 0);
            }
            if (ok)
                return true;
        } else {
            if (exactOrPrefixMatch(versionIn, r))
                return true;
        }
    }
    return false;
}

bool satisfiesFabricRange(const QString& versionIn, const QString& rangeIn)
{
    if (rangeIn.isEmpty())
        return true;
    QString text = rangeIn.trimmed();
    if (text.isEmpty() || text == "*")
        return true;

    // Fabric arrays are the official OR form, but support common textual ORs.
    auto ors = text.split(QStringLiteral("||"), Qt::SkipEmptyParts);
    for (const QString& clauseRaw : ors) {
        QStringList atoms;
        for (const QString& a : clauseRaw.split(' ', Qt::SkipEmptyParts))
            atoms.append(a);
        if (atoms.isEmpty())
            continue;
        bool allOk = true;
        for (const QString& atom : atoms) {
            if (!satisfiesFabricAtom(versionIn, atom)) {
                allOk = false;
                break;
            }
        }
        if (allOk)
            return true;
    }
    return false;
}

bool satisfiesRange(QStringView loader, const QString& version, const QString& range)
{
    if (range.isEmpty())
        return true;
    QString l = loader.toString().toLower();
    if (l == "forge" || l == "neoforge")
        return satisfiesMavenRange(version, range);
    return satisfiesFabricRange(version, range);
}

}  // namespace ModVersionRange
