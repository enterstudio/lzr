
#include "point.h"
#include "settings.h"


Point::Point(FrameEditorState* s, lzr::Point p) : QGraphicsObject(0)
{
    init();
    setPos(p.x, p.y);
    color = QColor(p.r, p.g, p.b, p.i);
    state = s;
}

Point::Point(FrameEditorState* s, QPointF p, QColor c) : QGraphicsObject(0)
{
    init();
    setPos(p);
    color = c;
    state = s;
}

void Point::init()
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setAcceptHoverEvents(true);
    hovered = false;
}

void Point::set_color(const QColor& c)
{
    color = c;
}

QColor Point::get_color() const
{
    return color;
}

lzr::Point Point::to_LZR() const
{
    return lzr::Point(x(),
                      y(),
                      color.red(),
                      color.green(),
                      color.blue(),
                      color.alpha());
}

QRectF Point::boundingRect() const
{
    return QRectF(-POINT_RADIUS, -POINT_RADIUS, POINT_DIAMETER, POINT_DIAMETER);
}


void Point::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if(hovered)
    {
        painter->setPen(POINT_HANDLE_COLOR);
        painter->drawEllipse(-POINT_RADIUS, -POINT_RADIUS, POINT_DIAMETER, POINT_DIAMETER);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawEllipse(-POINT_INNER_RADIUS, -POINT_INNER_RADIUS, POINT_INNER_DIAMETER, POINT_INNER_DIAMETER);
}


QVariant Point::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if(scene() && change == ItemPositionChange)
    {
        // Keep the item inside the scene rect.
        QPointF pos = state->grid->constrain_and_maybe_snap(value.toPointF());

        //if the point changed, return the changed point
        if(value.toPointF() != pos)
        {
            return pos;
        }
    }
    else if(scene() && change == ItemPositionHasChanged)
    {
        //moving the child should trigger a change in the parent,
        //to redraw the connecting lines
        emit changed();
    }

    //call default handler
    return QGraphicsItem::itemChange(change, value);
}


void Point::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    hovered = true;
    return QGraphicsItem::hoverEnterEvent(event);
}


void Point::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    hovered = false;
    return QGraphicsItem::hoverEnterEvent(event);
}
