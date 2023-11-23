/*
    SPDX-FileCopyrightText: 2017 Marco Martin <notmart@gmail.com>
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
#pragma once

#include "surface_p.h"
#include "xdgforeign_v2.h"

#include <qwayland-server-xdg-foreign-unstable-v2.h>

#include <QPointer>

namespace KWin
{
class XdgExportedV2Interface;
class XdgImportedV2Interface;
class XdgImporterV2Interface;
class XdgExporterV2Interface;

class XdgForeignV2InterfacePrivate
{
public:
    XdgForeignV2InterfacePrivate(Display *display, XdgForeignV2Interface *q);

    XdgForeignV2Interface *q;
    std::unique_ptr<XdgExporterV2Interface> exporter;
    std::unique_ptr<XdgImporterV2Interface> importer;
};

class XdgExporterV2Interface : public QObject, public QtWaylandServer::zxdg_exporter_v2
{
    Q_OBJECT

public:
    XdgExporterV2Interface(Display *display, XdgForeignV2Interface *foreign);

    XdgExportedSurface *exportedSurface(const QString &handle) const;
    XdgExportedSurface *exportSurface(SurfaceInterface *surface);

protected:
    void zxdg_exporter_v2_destroy(Resource *resource) override;
    void zxdg_exporter_v2_export_toplevel(Resource *resource, uint32_t id, wl_resource *surface) override;

private:
    void addExported(XdgExportedSurface *exported);

    QHash<QString, XdgExportedSurface *> m_exportedSurfaces;
};

class XdgImporterV2Interface : public QObject, public QtWaylandServer::zxdg_importer_v2
{
    Q_OBJECT

public:
    XdgImporterV2Interface(Display *display, XdgForeignV2Interface *foreign);

    void link(XdgImportedV2Interface *parent, SurfaceInterface *child);
    void unlink(XdgImportedV2Interface *parent, SurfaceInterface *child);

    SurfaceInterface *transientFor(SurfaceInterface *surface);

protected:
    void zxdg_importer_v2_destroy(Resource *resource) override;
    void zxdg_importer_v2_import_toplevel(Resource *resource, uint32_t id, const QString &handle) override;

private:
    XdgForeignV2Interface *m_foreign;
    QHash<SurfaceInterface *, XdgImportedV2Interface *> m_parents; // child->parent hash
    QHash<XdgImportedV2Interface *, SurfaceInterface *> m_children; // parent->child hash
};

class XdgExportedV2Interface : public XdgExportedSurface, public QtWaylandServer::zxdg_exported_v2
{
    Q_OBJECT

public:
    explicit XdgExportedV2Interface(SurfaceInterface *surface, wl_resource *resource);

protected:
    void zxdg_exported_v2_destroy(Resource *resource) override;
    void zxdg_exported_v2_destroy_resource(Resource *resource) override;
};

class XdgDummyImportedV2Interface : public QtWaylandServer::zxdg_imported_v2
{
public:
    explicit XdgDummyImportedV2Interface(wl_resource *resource);

protected:
    void zxdg_imported_v2_destroy(Resource *resource) override;
    void zxdg_imported_v2_destroy_resource(Resource *resource) override;
};

class XdgImportedV2Interface : public QObject, QtWaylandServer::zxdg_imported_v2
{
    Q_OBJECT
public:
    explicit XdgImportedV2Interface(XdgExportedSurface *exported, wl_resource *resource);

    SurfaceInterface *child() const;
    SurfaceInterface *surface() const;

Q_SIGNALS:
    void childChanged(KWin::SurfaceInterface *child);

private Q_SLOTS:
    void handleExportedDestroyed();

private:
    XdgExportedSurface *m_exported;
    QPointer<SurfaceInterface> m_child;

protected:
    void zxdg_imported_v2_set_parent_of(Resource *resource, wl_resource *surface) override;
    void zxdg_imported_v2_destroy(Resource *resource) override;
    void zxdg_imported_v2_destroy_resource(Resource *resource) override;
};
}
