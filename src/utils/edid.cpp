/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2015 Martin Flöser <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2019 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "edid.h"

#include "c_ptr.h"
#include "config-kwin.h"

#include <QFile>
#include <QStandardPaths>

#include <KLocalizedString>
#include <QCryptographicHash>

extern "C" {
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
}

namespace KWin
{

static QByteArray parsePnpId(const uint8_t *data)
{
    // Decode PNP ID from three 5 bit words packed into 2 bytes:
    //
    // | Byte |        Bit                    |
    // |      | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    // ----------------------------------------
    // |  1   | 0)| (4| 3 | 2 | 1 | 0)| (4| 3 |
    // |      | * |    Character 1    | Char 2|
    // ----------------------------------------
    // |  2   | 2 | 1 | 0)| (4| 3 | 2 | 1 | 0)|
    // |      | Character2|      Character 3  |
    // ----------------------------------------
    const uint offset = 0x8;

    char pnpId[4];
    pnpId[0] = 'A' + ((data[offset + 0] >> 2) & 0x1f) - 1;
    pnpId[1] = 'A' + (((data[offset + 0] & 0x3) << 3) | ((data[offset + 1] >> 5) & 0x7)) - 1;
    pnpId[2] = 'A' + (data[offset + 1] & 0x1f) - 1;
    pnpId[3] = '\0';

    return QByteArray(pnpId);
}

static QByteArray parseEisaId(const uint8_t *data)
{
    for (int i = 72; i <= 108; i += 18) {
        // Skip the block if it isn't used as monitor descriptor.
        if (data[i]) {
            continue;
        }
        if (data[i + 1]) {
            continue;
        }

        // We have found the EISA ID, it's stored as ASCII.
        if (data[i + 3] == 0xfe) {
            return QByteArray(reinterpret_cast<const char *>(&data[i + 5]), 13).trimmed();
        }
    }

    // If there isn't an ASCII EISA ID descriptor, try to decode PNP ID
    return parsePnpId(data);
}

static QByteArray parseVendor(const uint8_t *data)
{
    const auto pnpId = parsePnpId(data);

    // Map to vendor name
    QFile pnpFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("hwdata/pnp.ids")));
    if (pnpFile.exists() && pnpFile.open(QIODevice::ReadOnly)) {
        while (!pnpFile.atEnd()) {
            const auto line = pnpFile.readLine();
            if (line.startsWith(pnpId)) {
                return line.mid(4).trimmed();
            }
        }
    }

    return {};
}

Edid::Edid()
{
}

Edid::Edid(const void *data, uint32_t size)
{
    m_raw.resize(size);
    memcpy(m_raw.data(), data, size);

    const uint8_t *bytes = static_cast<const uint8_t *>(data);

    auto info = di_info_parse_edid(data, size);
    if (!info) {
        return;
    }
    const di_edid *edid = di_info_get_edid(info);
    const di_edid_vendor_product *productInfo = di_edid_get_vendor_product(edid);
    const di_edid_screen_size *screenSize = di_edid_get_screen_size(edid);

    m_physicalSize = QSize(screenSize->width_cm, screenSize->height_cm) * 10;
    m_eisaId = parseEisaId(bytes);
    UniqueCPtr<char> monitorName{di_info_get_model(info)};
    m_monitorName = QByteArray(monitorName.get());
    m_serialNumber = QByteArray::number(productInfo->serial);
    m_vendor = parseVendor(bytes);
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(m_raw);
    m_hash = QString::fromLatin1(hash.result().toHex());

    m_isValid = true;
    di_info_destroy(info);
}

bool Edid::isValid() const
{
    return m_isValid;
}

QSize Edid::physicalSize() const
{
    return m_physicalSize;
}

QByteArray Edid::eisaId() const
{
    return m_eisaId;
}

QByteArray Edid::monitorName() const
{
    return m_monitorName;
}

QByteArray Edid::serialNumber() const
{
    return m_serialNumber;
}

QByteArray Edid::vendor() const
{
    return m_vendor;
}

QByteArray Edid::raw() const
{
    return m_raw;
}

QString Edid::manufacturerString() const
{
    QString manufacturer;
    if (!m_vendor.isEmpty()) {
        manufacturer = QString::fromLatin1(m_vendor);
    } else if (!m_eisaId.isEmpty()) {
        manufacturer = QString::fromLatin1(m_eisaId);
    }
    return manufacturer;
}

QString Edid::nameString() const
{
    if (!m_monitorName.isEmpty()) {
        QString m = QString::fromLatin1(m_monitorName);
        if (!m_serialNumber.isEmpty()) {
            m.append('/');
            m.append(QString::fromLatin1(m_serialNumber));
        }
        return m;
    } else if (!m_serialNumber.isEmpty()) {
        return QString::fromLatin1(m_serialNumber);
    } else {
        return i18n("unknown");
    }
}

QString Edid::hash() const
{
    return m_hash;
}

} // namespace KWin
