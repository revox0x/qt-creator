// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "nodeinstance.h"

#include <modelnode.h>

#include <QDebug>
#include <QPainter>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

QT_BEGIN_NAMESPACE
void qt_blurImage(QPainter *painter, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0);
QT_END_NAMESPACE

namespace QmlDesigner {

class ProxyNodeInstanceData
{
public:
    ProxyNodeInstanceData()
    {}

    qint32 parentInstanceId{-1};
    ModelNode modelNode;
    QRectF boundingRect;
    QRectF boundingRectPixmap;
    QRectF contentItemBoundingRect;
    QPointF position;
    QSizeF size;
    QTransform transform;
    QTransform contentTransform;
    QTransform contentItemTransform;
    QTransform sceneTransform;
    int penWidth{1};
    bool isAnchoredBySibling{false};
    bool isAnchoredByChildren{false};
    bool hasContent{false};
    bool isMovable{false};
    bool isResizable{false};
    bool isInLayoutable{false};
    bool directUpdates{false};

    std::map<Utils::SmallString, QVariant, std::less<>> propertyValues;
    std::map<Utils::SmallString, bool, std::less<>> hasBindingForProperty;
    std::map<Utils::SmallString, bool, std::less<>> hasAnchors;
    std::map<Utils::SmallString, TypeName, std::less<>> instanceTypes;

    QPixmap renderPixmap;
    QPixmap blurredRenderPixmap;

    QString errorMessage;

    std::map<Utils::SmallString, std::pair<PropertyName, qint32>, std::less<>> anchors;
    QStringList allStates;
};

NodeInstance::NodeInstance() = default;

NodeInstance::NodeInstance(ProxyNodeInstanceData *dPointer)
    : d(dPointer)
{
}

NodeInstance NodeInstance::create(const ModelNode &node)
{
    auto d = new ProxyNodeInstanceData;

    d->modelNode = node;

    return NodeInstance(d);
}

NodeInstance::~NodeInstance() = default;

NodeInstance::NodeInstance(const NodeInstance &other) = default;

NodeInstance &NodeInstance::operator=(const NodeInstance &other) = default;

ModelNode NodeInstance::modelNode() const
{
    if (d)
        return d->modelNode;
    else
        return ModelNode();
}

qint32 NodeInstance::instanceId() const
{
    if (d)
        return d->modelNode.internalId();
    else
        return -1;
}

void NodeInstance::setDirectUpdate(bool directUpdates)
{
    if (d)
        d->directUpdates = directUpdates;
}

bool NodeInstance::directUpdates() const
{
    if (d)
        return d->directUpdates && !(d->transform.isRotating() || d->transform.isScaling() || hasAnchors());
    else
        return true;
}

void NodeInstance::setX(double x)
{
    if (d && directUpdates()) {
        double dx = x - d->transform.dx();
        d->transform.translate(dx, 0.0);
    }
}

void NodeInstance::setY(double y)
{
    if (d && directUpdates()) {
        double dy = y - d->transform.dy();
        d->transform.translate(0.0, dy);
    }
}

bool NodeInstance::hasAnchors() const
{
    return hasAnchor("anchors.fill")
            || hasAnchor("anchors.centerIn")
            || hasAnchor("anchors.top")
            || hasAnchor("anchors.left")
            || hasAnchor("anchors.right")
            || hasAnchor("anchors.bottom")
            || hasAnchor("anchors.horizontalCenter")
            || hasAnchor("anchors.verticalCenter")
            || hasAnchor("anchors.baseline");
}

QString NodeInstance::error() const
{
    return d->errorMessage;
}

bool NodeInstance::hasError() const
{
    return !d->errorMessage.isEmpty();
}

QStringList NodeInstance::allStateNames() const
{
    return d->allStates;
}

bool NodeInstance::isValid() const
{
    return instanceId() >= 0 && modelNode().isValid();
}

void NodeInstance::makeInvalid()
{
    if (d)
        d->modelNode = ModelNode();
}

QRectF NodeInstance::boundingRect() const
{
    if (isValid()) {
        if (d->boundingRectPixmap.isValid())
            return d->boundingRectPixmap;
        return d->boundingRect;
    } else
        return QRectF();
}

QRectF NodeInstance::contentItemBoundingRect() const
{
    if (isValid())
        return d->contentItemBoundingRect;
    else
        return QRectF();
}

bool NodeInstance::hasContent() const
{
    if (isValid())
        return d->hasContent;
    else
        return false;
}

bool NodeInstance::isAnchoredBySibling() const
{
    if (isValid())
        return d->isAnchoredBySibling;
    else
        return false;
}

bool NodeInstance::isAnchoredByChildren() const
{
    if (isValid())
        return d->isAnchoredByChildren;
    else
        return false;
}

bool NodeInstance::isMovable() const
{
    if (isValid())
        return d->isMovable;
    else
        return false;
}

bool NodeInstance::isResizable() const
{
    if (isValid())
        return d->isResizable;
    else
        return false;
}

QTransform NodeInstance::transform() const
{
    if (isValid())
        return d->transform;
    else
        return QTransform();
}

QTransform NodeInstance::contentTransform() const
{
    if (isValid())
        return d->contentTransform;
    else
        return QTransform();
}

QTransform NodeInstance::contentItemTransform() const
{
    if (isValid())
        return d->contentItemTransform;
    else
        return QTransform();
}
QTransform NodeInstance::sceneTransform() const
{
    if (isValid())
        return d->sceneTransform;
    else
        return QTransform();
}
bool NodeInstance::isInLayoutable() const
{
    if (isValid())
        return d->isInLayoutable;
    else
        return false;
}

QPointF NodeInstance::position() const
{
    if (isValid())
        return d->position;
    else
        return QPointF();
}

QSizeF NodeInstance::size() const
{
    if (isValid())
        return d->size;
    else
        return QSizeF();
}

int NodeInstance::penWidth() const
{
    if (isValid())
        return d->penWidth;
    else
        return 1;
}

namespace {

template<typename... Arguments>
auto value(const std::map<Arguments...> &dict,
           PropertyNameView key,
           typename std::map<Arguments...>::mapped_type defaultValue = {})
{
    if (auto found = dict.find(key); found != dict.end())
        return found->second;

    return defaultValue;
}

} // namespace

QVariant NodeInstance::property(PropertyNameView name) const
{
    if (isValid()) {
        if (auto found = d->propertyValues.find(name); found != d->propertyValues.end()) {
            return found->second;
        } else {
            // Query may be for a subproperty, e.g. scale.x
            const auto index = name.indexOf('.');
            if (index != -1) {
                PropertyNameView parentPropName = name.left(index);
                QVariant varValue = value(d->propertyValues, parentPropName);
                if (varValue.typeId() == QVariant::Vector2D) {
                    auto value = varValue.value<QVector2D>();
                    char subProp = name.right(1)[0];
                    float subValue = 0.f;
                    switch (subProp) {
                    case 'x':
                        subValue = value.x();
                        break;
                    case 'y':
                        subValue = value.y();
                        break;
                    default:
                        subValue = 0.f;
                        break;
                    }
                    return QVariant(subValue);
                } else if (varValue.typeId() == QVariant::Vector3D) {
                    auto value = varValue.value<QVector3D>();
                    char subProp = name.right(1)[0];
                    float subValue = 0.f;
                    switch (subProp) {
                    case 'x':
                        subValue = value.x();
                        break;
                    case 'y':
                        subValue = value.y();
                        break;
                    case 'z':
                        subValue = value.z();
                        break;
                    default:
                        subValue = 0.f;
                        break;
                    }
                    return QVariant(subValue);
                } else if (varValue.typeId() == QVariant::Vector4D) {
                    auto value = varValue.value<QVector4D>();
                    char subProp = name.right(1)[0];
                    float subValue = 0.f;
                    switch (subProp) {
                    case 'x':
                        subValue = value.x();
                        break;
                    case 'y':
                        subValue = value.y();
                        break;
                    case 'z':
                        subValue = value.z();
                        break;
                    case 'w':
                        subValue = value.w();
                        break;
                    default:
                        subValue = 0.f;
                        break;
                    }
                    return QVariant(subValue);
                }
            }
        }
    }

    return QVariant();
}

bool NodeInstance::hasProperty(PropertyNameView name) const
{
    if (isValid())
        return d->propertyValues.contains(name);

    return false;
}

bool NodeInstance::hasBindingForProperty(PropertyNameView name) const
{
    if (isValid())
        return value(d->hasBindingForProperty, name, false);

    return false;
}

TypeName NodeInstance::instanceType(PropertyNameView name) const
{
    if (isValid())
        return value(d->instanceTypes, name);

    return TypeName();
}

qint32 NodeInstance::parentId() const
{
    if (isValid())
        return d->parentInstanceId;
    else
        return false;
}

bool NodeInstance::hasAnchor(PropertyNameView name) const
{
    if (isValid())
        return value(d->hasAnchors, name, false);

    return false;
}

QPair<PropertyName, qint32> NodeInstance::anchor(PropertyNameView name) const
{
    if (isValid())
        return value(d->anchors, name, QPair<PropertyName, qint32>(PropertyName(), qint32(-1)));

    return QPair<PropertyName, qint32>(PropertyName(), -1);
}

void NodeInstance::setProperty(PropertyNameView name, const QVariant &value)
{
    const auto index = name.indexOf('.');
    if (index != -1) {
        PropertyNameView parentPropName = name.left(index);
        QVariant oldValue = QmlDesigner::value(d->propertyValues, parentPropName);
        QVariant newValueVar;
        bool update = false;
        if (oldValue.typeId() == QVariant::Vector2D) {
            QVector2D newValue;
            if (oldValue.typeId() == QVariant::Vector2D)
                newValue = oldValue.value<QVector2D>();
            if (name.endsWith(".x")) {
                newValue.setX(value.toFloat());
                update = true;
            } else if (name.endsWith(".y")) {
                newValue.setY(value.toFloat());
                update = true;
            }
            newValueVar = newValue;
        } else if (oldValue.typeId() == QVariant::Vector3D) {
            QVector3D newValue;
            if (oldValue.typeId() == QVariant::Vector3D)
                newValue = oldValue.value<QVector3D>();
            if (name.endsWith(".x")) {
                newValue.setX(value.toFloat());
                update = true;
            } else if (name.endsWith(".y")) {
                newValue.setY(value.toFloat());
                update = true;
            } else if (name.endsWith(".z")) {
                newValue.setZ(value.toFloat());
                update = true;
            }
            newValueVar = newValue;
        } else if (oldValue.typeId() == QVariant::Vector4D) {
            QVector4D newValue;
            if (oldValue.typeId() == QVariant::Vector4D)
                newValue = oldValue.value<QVector4D>();
            if (name.endsWith(".x")) {
                newValue.setX(value.toFloat());
                update = true;
            } else if (name.endsWith(".y")) {
                newValue.setY(value.toFloat());
                update = true;
            } else if (name.endsWith(".z")) {
                newValue.setZ(value.toFloat());
                update = true;
            } else if (name.endsWith(".w")) {
                newValue.setW(value.toFloat());
                update = true;
            }
            newValueVar = newValue;
        }
        if (update) {
            d->propertyValues.emplace(parentPropName, newValueVar);
            return;
        }
    }

    d->propertyValues.emplace(name, value);
}

QPixmap NodeInstance::renderPixmap() const
{
    return d->renderPixmap;
}

QPixmap NodeInstance::blurredRenderPixmap() const
{
    if (d->blurredRenderPixmap.isNull()) {
        d->blurredRenderPixmap = QPixmap(d->renderPixmap.size());
        QPainter blurPainter(&d->blurredRenderPixmap);
        QImage renderImage = d->renderPixmap.toImage();
        qt_blurImage(&blurPainter, renderImage, 8.0, false, false);
    }

    return d->blurredRenderPixmap;
}

void NodeInstance::setRenderPixmap(const QImage &image)
{
    d->renderPixmap = QPixmap::fromImage(image);

    d->blurredRenderPixmap = QPixmap();
}

bool NodeInstance::setError(const QString &errorMessage)
{
    if (d->errorMessage != errorMessage) {
        d->errorMessage = errorMessage;
        return true;
    }
    return false;
}

void NodeInstance::setParentId(qint32 instanceId)
{
    d->parentInstanceId = instanceId;
}

InformationName NodeInstance::setInformationSize(const QSizeF &size)
{
    if (d->size != size) {
        d->size = size;
        return Size;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationBoundingRect(const QRectF &rectangle)
{
    if (d->boundingRect != rectangle) {
        d->boundingRect = rectangle;
        return BoundingRect;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationBoundingRectPixmap(const QRectF &rectangle)
{
    if (d->boundingRectPixmap != rectangle) {
        d->boundingRectPixmap = rectangle;
        return BoundingRectPixmap;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationContentItemBoundingRect(const QRectF &rectangle)
{
    if (d->contentItemBoundingRect != rectangle) {
        d->contentItemBoundingRect = rectangle;
        return ContentItemBoundingRect;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationTransform(const QTransform &transform)
{
    if (!directUpdates() && d->transform != transform) {
        d->transform = transform;
        return Transform;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationContentTransform(const QTransform &contentTransform)
{
    if (d->contentTransform != contentTransform) {
        d->contentTransform = contentTransform;
        return ContentTransform;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationContentItemTransform(const QTransform &contentItemTransform)
{
    if (d->contentItemTransform != contentItemTransform) {
        d->contentItemTransform = contentItemTransform;
        return ContentItemTransform;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationPenWith(int penWidth)
{
    if (d->penWidth != penWidth) {
        d->penWidth = penWidth;
        return PenWidth;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationPosition(const QPointF &position)
{
    if (d->position != position) {
        d->position = position;
        return Position;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationIsInLayoutable(bool isInLayoutable)
{
    if (d->isInLayoutable != isInLayoutable) {
        d->isInLayoutable = isInLayoutable;
        return IsInLayoutable;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationSceneTransform(const QTransform &sceneTransform)
{
  if (d->sceneTransform != sceneTransform) {
        d->sceneTransform = sceneTransform;
        if (!directUpdates())
            return SceneTransform;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationIsResizable(bool isResizable)
{
    if (d->isResizable != isResizable) {
        d->isResizable = isResizable;
        return IsResizable;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationIsMovable(bool isMovable)
{
    if (d->isMovable != isMovable) {
        d->isMovable = isMovable;
        return IsMovable;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationIsAnchoredByChildren(bool isAnchoredByChildren)
{
    if (d->isAnchoredByChildren != isAnchoredByChildren) {
        d->isAnchoredByChildren = isAnchoredByChildren;
        return IsAnchoredByChildren;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationIsAnchoredBySibling(bool isAnchoredBySibling)
{
    if (d->isAnchoredBySibling != isAnchoredBySibling) {
        d->isAnchoredBySibling = isAnchoredBySibling;
        return IsAnchoredBySibling;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationHasContent(bool hasContent)
{
    if (d->hasContent != hasContent) {
        d->hasContent = hasContent;
        return HasContent;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationHasAnchor(PropertyNameView sourceAnchorLine, bool hasAnchor)
{
    if (auto found = d->hasAnchors.find(sourceAnchorLine);
        found == d->hasAnchors.end() || found->second != hasAnchor) {
        d->hasAnchors.emplace_hint(found, sourceAnchorLine, hasAnchor);
        return HasAnchor;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationAnchor(PropertyNameView sourceAnchorLine,
                                                   const PropertyName &targetAnchorLine,
                                                   qint32 targetInstanceId)
{
    std::pair<PropertyName, qint32> anchorPair = std::pair<PropertyName, qint32>(targetAnchorLine,
                                                                                 targetInstanceId);
    if (auto found = d->anchors.find(sourceAnchorLine);
        found == d->anchors.end() || found->second != anchorPair) {
        d->anchors.emplace_hint(found, sourceAnchorLine, anchorPair);
        return Anchor;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationInstanceTypeForProperty(PropertyNameView property,
                                                                    const TypeName &type)
{
    if (auto found = d->instanceTypes.find(property);
        found == d->instanceTypes.end() || found->second != type) {
        d->instanceTypes.emplace_hint(found, property, type);
        return InstanceTypeForProperty;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformationHasBindingForProperty(PropertyNameView property,
                                                                  bool hasProperty)
{
    if (auto found = d->hasBindingForProperty.find(property);
        found == d->hasBindingForProperty.end() || found->second != hasProperty) {
        d->hasBindingForProperty.emplace_hint(found, property, hasProperty);
        return HasBindingForProperty;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setAllStates(const QStringList &states)
{
    if (d->allStates != states) {
        d->allStates = states;
        return AllStates;
    }

    return NoInformationChange;
}

InformationName NodeInstance::setInformation(InformationName name, const QVariant &information, const QVariant &secondInformation, const QVariant &thirdInformation)
{
    switch (name) {
    case Size: return setInformationSize(information.toSizeF());
    case BoundingRect:
        return setInformationBoundingRect(information.toRectF());
    case BoundingRectPixmap:
        return setInformationBoundingRectPixmap(information.toRectF());
    case ContentItemBoundingRect: return setInformationContentItemBoundingRect(information.toRectF());
    case Transform: return setInformationTransform(information.value<QTransform>());
    case ContentTransform: return setInformationContentTransform(information.value<QTransform>());
    case ContentItemTransform: return setInformationContentItemTransform(information.value<QTransform>());
    case PenWidth: return setInformationPenWith(information.toInt());
    case Position: return setInformationPosition(information.toPointF());
    case IsInLayoutable: return setInformationIsInLayoutable(information.toBool());
    case SceneTransform: return setInformationSceneTransform(information.value<QTransform>());
    case IsResizable: return setInformationIsResizable(information.toBool());
    case IsMovable: return setInformationIsMovable(information.toBool());
    case IsAnchoredByChildren: return setInformationIsAnchoredByChildren(information.toBool());
    case IsAnchoredBySibling: return setInformationIsAnchoredBySibling(information.toBool());
    case HasContent: return setInformationHasContent(information.toBool());
    case HasAnchor: return setInformationHasAnchor(information.toByteArray(), secondInformation.toBool());break;
    case Anchor: return setInformationAnchor(information.toByteArray(), secondInformation.toByteArray(), thirdInformation.value<qint32>());
    case InstanceTypeForProperty: return setInformationInstanceTypeForProperty(information.toByteArray(), secondInformation.toByteArray());
    case HasBindingForProperty: return setInformationHasBindingForProperty(information.toByteArray(), secondInformation.toBool());
    case AllStates: return setAllStates(information.toStringList());
    case NoName:
    default: break;
    }

    return NoInformationChange;
}

bool operator ==(const NodeInstance &first, const NodeInstance &second)
{
    return first.instanceId() >= 0 && first.instanceId() == second.instanceId();
}

}
