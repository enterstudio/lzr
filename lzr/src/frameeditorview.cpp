
#include <QtGui>
#include "frameeditorview.h"
#include "settings.h"


#define ZOOM_FACTOR 1.2


FrameEditor::FrameEditor(QWidget *parent) : QGraphicsView(parent)
{
    // setRenderHint(QPainter::Antialiasing);
    setFrameStyle(QFrame::NoFrame);
    //disable all scroll bars
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    //when points move, the connecting lines must be also redrawn
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void FrameEditor::reset()
{
    fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
}


void FrameEditor::resizeEvent(QResizeEvent* e)
{
    //resize
    QGraphicsView::resizeEvent(e);

    //make a new transformation matrix that's scaled correctly
    QTransform t;
    double s = qMin(width(), height()) / scene()->itemsBoundingRect().width(); // min(w, h) / 2.0
    t.scale(s, -s);
    setTransform(t);
}

void FrameEditor::keyPressEvent(QKeyEvent* e)
{
    if(!e->isAutoRepeat())
    {
        if(e->key() == EDITOR_PAN_KEY)
        {
            setInteractive(false);
            setDragMode(ScrollHandDrag);
        }
    }

    QGraphicsView::keyPressEvent(e);
}

void FrameEditor::keyReleaseEvent(QKeyEvent* e)
{
    if(!e->isAutoRepeat())
    {
        if(e->key() == EDITOR_PAN_KEY)
        {
            setInteractive(true);
            setDragMode(NoDrag);
        }
    }

    QGraphicsView::keyReleaseEvent(e);
}

void FrameEditor::wheelEvent(QWheelEvent* event)
{
    double factor = ZOOM_FACTOR;

    if(event->angleDelta().y() < 0)
        factor = 1.0 / ZOOM_FACTOR;

    scale(factor, factor);
}
