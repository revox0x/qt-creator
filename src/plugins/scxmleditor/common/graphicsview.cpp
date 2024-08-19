// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "baseitem.h"
#include "connectableitem.h"
#include "graphicsscene.h"
#include "graphicsview.h"
#include "sceneutils.h"
#include "scxmleditortr.h"
#include "scxmleditortr.h"
#include "scxmluifactory.h"
#include "shapeprovider.h"

#include <coreplugin/icore.h>

#include <QDebug>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>


using namespace ScxmlEditor::PluginInterface;
using namespace ScxmlEditor::Common;

GraphicsView::GraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    setTransformationAnchor(AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(RubberBandDrag);
    setRubberBandSelectionMode(Qt::ContainsItemShape);
    setBackgroundBrush(QBrush(QColor(0xef, 0xef, 0xef)));
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &GraphicsView::updateView);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &GraphicsView::updateView);
}

void GraphicsView::initLayoutItem()
{
    if (!scene())
        return;

    QRectF r = rect();
    if (!m_layoutItem) {
        m_layoutItem = new LayoutItem(r);
        scene()->addItem(m_layoutItem);
    } else {
        m_layoutItem->setBoundingRect(r);
    }
}

void GraphicsView::setGraphicsScene(ScxmlEditor::PluginInterface::GraphicsScene *s)
{
    // Disconnect old connections
    if (scene())
        scene()->disconnect(this);

    setScene(s);

    if (scene())
        connect(scene(), &PluginInterface::GraphicsScene::sceneRectChanged, this, &GraphicsView::sceneRectHasChanged);

    initLayoutItem();
}

void GraphicsView::sceneRectHasChanged(const QRectF &r)
{
    m_minZoomValue = qMin(rect().width() / r.width(), rect().height() / r.height());
    updateView();
}

void GraphicsView::updateView()
{
    emit viewChanged(mapToScene(rect()));
    emit zoomPercentChanged(qBound(0, int((transform().m11() - m_minZoomValue) / (m_maxZoomValue - m_minZoomValue) * 100), 100));
    if (auto graphicsScene = qobject_cast<GraphicsScene*>(scene()))
        graphicsScene->checkItemsVisibility(transform().m11());
}

void GraphicsView::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    // Init layout item if necessary
    initLayoutItem();
    updateView();
}

void GraphicsView::zoomTo(int value)
{
    // Scale to zoom
    qreal targetScale = m_minZoomValue + (m_maxZoomValue - m_minZoomValue) * ((qreal)value / 100.0);
    qreal scaleFactor = targetScale / transform().m11();
    scale(scaleFactor, scaleFactor);
    if (auto graphicsScene = qobject_cast<GraphicsScene*>(scene()))
        graphicsScene->checkItemsVisibility(transform().m11());
}

void GraphicsView::zoomIn()
{
    if (transform().m11() < m_maxZoomValue) {
        scale(1.1, 1.1);
        updateView();
    }
}

void GraphicsView::zoomOut()
{
    if (transform().m11() > m_minZoomValue) {
        scale(1.0 / 1.1, 1.0 / 1.1);
        updateView();
    }
}

void GraphicsView::wheelEvent(QWheelEvent *event)
{
    if (Qt::ControlModifier & event->modifiers()) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else
            zoomOut();
    } else
        QGraphicsView::wheelEvent(event);
}

void GraphicsView::setPanning(bool pan)
{
    setDragMode(pan ? ScrollHandDrag : RubberBandDrag);
}

void GraphicsView::magnifierClicked(double zoomLevel, const QPointF &p)
{
    emit magnifierChanged(false);
    qreal scaleFactor = zoomLevel / transform().m11();
    scale(scaleFactor, scaleFactor);
    centerOn(p);
    updateView();
}

QImage GraphicsView::grabView()
{
    return grab(rect().adjusted(0, 0, -10, -10)).toImage();
}

void GraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    emit panningChanged(event->modifiers() == Qt::ShiftModifier);
    QGraphicsView::keyReleaseEvent(event);
}

void GraphicsView::keyPressEvent(QKeyEvent *event)
{
    emit panningChanged(event->modifiers() == Qt::ShiftModifier);
    QGraphicsView::keyPressEvent(event);
}

void GraphicsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->data("dragType") == "Shape")
        event->accept();
    else
        event->ignore();
}

void GraphicsView::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_shapeProvider && m_document && event->mimeData()->data("dragType") == "Shape") {

        int groupIndex = event->mimeData()->data("groupIndex").toInt();
        int shapeIndex = event->mimeData()->data("shapeIndex").toInt();

        ScxmlTag *targetTag = nullptr;

        QList<QGraphicsItem*> parentItems = items(event->position().toPoint());
        QPointF sceneP = mapToScene(event->position().toPoint());
        for (int i = 0; i < parentItems.count(); ++i) {
            auto item = static_cast<BaseItem*>(parentItems[i]);
            if (item && item->type() >= TransitionType && item->containsScenePoint(sceneP)) {
                targetTag = item->tag();
                break;
            }
        }

        if (!targetTag)
            targetTag = m_document->rootTag();

        if (m_shapeProvider->canDrop(groupIndex, shapeIndex, targetTag))
            event->accept();
        else
            event->ignore();
    } else {
        event->ignore();
    }
}

void GraphicsView::dropEvent(QDropEvent *event)
{
    if (m_shapeProvider && m_document && event->mimeData()->data("dragType") == "Shape") {
        event->accept();

        int groupIndex = event->mimeData()->data("groupIndex").toInt();
        int shapeIndex = event->mimeData()->data("shapeIndex").toInt();

        ScxmlTag *targetTag = nullptr;
        QPointF targetPos = mapToScene(event->position().toPoint());

        QList<QGraphicsItem*> parentItems = items(event->position().toPoint());
        for (int i = 0; i < parentItems.count(); ++i) {
            auto item = static_cast<const BaseItem*>(parentItems[i]);
            if (item && item->type() >= StateType) {
                targetPos = item->mapFromScene(targetPos);
                targetTag = item->tag();
                break;
            }
        }

        if (!targetTag)
            targetTag = m_document->rootTag();

        if (m_shapeProvider->canDrop(groupIndex, shapeIndex, targetTag)) {
            if (auto graphicsScene = qobject_cast<GraphicsScene*>(scene()))
                graphicsScene->unselectAll();
            m_document->setCurrentTag(targetTag);
            QByteArray scxmlData = m_shapeProvider->scxmlCode(groupIndex, shapeIndex, targetTag);
            if (!scxmlData.isEmpty() && !m_document->pasteData(scxmlData, targetPos, targetPos))
                QMessageBox::warning(Core::ICore::dialogParent(), Tr::tr("SCXML Generation Failed"),
                                     m_document->lastError());
        }
    } else {
        event->ignore();
    }
}

void GraphicsView::paintEvent(QPaintEvent *event)
{
    if (m_drawingEnabled) {
        QGraphicsView::paintEvent(event);
    } else {
        QPainter painter(viewport());
        painter.save();
        painter.drawText(rect(), Qt::AlignCenter, Tr::tr("Loading document..."));
        painter.restore();
    }
}

void GraphicsView::setDrawingEnabled(bool enabled)
{
    setHorizontalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(enabled ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff);
    m_drawingEnabled = enabled;
}

void GraphicsView::setUiFactory(ScxmlUiFactory *factory)
{
    if (factory)
        m_shapeProvider = static_cast<ShapeProvider*>(factory->object("shapeProvider"));
}

void GraphicsView::setDocument(ScxmlDocument *document)
{
    m_document = document;
}

void GraphicsView::fitSceneToView()
{
    if (!scene())
        return;

    fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
    updateView();
}

void GraphicsView::zoomToItem(QGraphicsItem *item)
{
    if (!item)
        return;

    qreal scaleFactor = 1.0 / transform().m11();
    scale(scaleFactor, scaleFactor);
    centerOn(item);
    updateView();
}

void GraphicsView::centerToItem(QGraphicsItem *item)
{
    centerOn(item);
    updateView();
}

void GraphicsView::moveToPoint(const QPointF &p)
{
    centerOn(p);
    updateView();
}

double GraphicsView::minZoomValue() const
{
    return m_minZoomValue;
}

double GraphicsView::maxZoomValue() const
{
    return m_maxZoomValue;
}
