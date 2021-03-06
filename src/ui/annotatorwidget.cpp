/*************************************************************************
 *  Copyright (C) 2020 by Caio Jordão Carvalho <caiojcarvalho@gmail.com> *
 *                                                                       *
 *  This program is free software; you can redistribute it and/or        *
 *  modify it under the terms of the GNU General Public License as       *
 *  published by the Free Software Foundation; either version 3 of       *
 *  the License, or (at your option) any later version.                  *
 *                                                                       *
 *  This program is distributed in the hope that it will be useful,      *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 *  GNU General Public License for more details.                         *
 *                                                                       *
 *  You should have received a copy of the GNU General Public License    *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 *************************************************************************/

#include "annotatorwidget.h"
#include "ui_annotatorwidget.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMouseEvent>

#include <QDebug>

AnnotatorWidget::AnnotatorWidget(QWidget* parent):
    QWidget(parent),
    m_ui(new Ui::AnnotatorWidget),
    m_currentImage(nullptr),
    m_shape(marK::Shape::Polygon),
    m_scaleW(0.0),
    m_scaleH(0.0)
{
    m_ui->setupUi(this);

    QGraphicsScene *scene = new QGraphicsScene;
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_ui->graphicsView->setScene(scene);
    m_ui->graphicsView->setMinimumSize(860, 600);

    m_ui->graphicsView->installEventFilter(this);

    setMouseTracking(true);
}

AnnotatorWidget::~AnnotatorWidget()
{
}

// TODO: improve me and remove this loop
QVector<Polygon> AnnotatorWidget::savedPolygons() const
{
    QVector<Polygon> copyPolygons(m_savedPolygons);

    for (Polygon& polygon : copyPolygons) {
        for (QPointF& point : polygon) {
            point -= m_currentImage->pos();
            point = scaledPoint(point);
        }
    }

    return copyPolygons;
}

void AnnotatorWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_currentImage != nullptr) {
        QPoint point = event->pos();
        QPointF clickedPoint = m_ui->graphicsView->mapToScene(point);
        bool isImageClicked = m_ui->graphicsView->scene()->itemAt(clickedPoint, m_ui->graphicsView->transform()) == m_currentImage;

        if (m_shape == marK::Shape::Polygon) {
            auto savedPolClicked = std::find_if(
                m_savedPolygons.begin(), m_savedPolygons.end(),
                [&](const Polygon& pol) {
                    return pol.containsPoint(clickedPoint, Qt::OddEvenFill);
                }
            );
            bool isSavedPolClicked = savedPolClicked != m_savedPolygons.end();
            if (isSavedPolClicked) {
                m_currentPolygon = *savedPolClicked;
                m_savedPolygons.erase(savedPolClicked);
                m_currentPolygon.pop_back();
            }

            bool isPolFirstPtClicked = false;
            if (!m_currentPolygon.empty()) {
                QPointF cPolFirstPt = m_currentPolygon.first();
                QRectF cPolFirstPtRect(cPolFirstPt, QPointF(cPolFirstPt.x() + 10, cPolFirstPt.y() + 10));
                isPolFirstPtClicked = cPolFirstPtRect.contains(clickedPoint);
                if (isPolFirstPtClicked)
                    clickedPoint = QPointF(cPolFirstPt);
            }

            if (isSavedPolClicked || isPolFirstPtClicked || isImageClicked) {
                m_currentPolygon << clickedPoint;

                if (m_currentPolygon.size() > 1 && m_currentPolygon.isClosed()) {
                    m_savedPolygons << m_currentPolygon;
                    m_currentPolygon.clear();
                }

                repaint();
            }
        }
        else if (m_shape == marK::Shape::Rectangle) {
            if (isImageClicked) {
                if (m_currentPolygon.empty())
                    m_currentPolygon << clickedPoint;
                else {
                    QPointF firstPt = m_currentPolygon.first();
                    m_currentPolygon << QPointF(clickedPoint.x(), firstPt.y()) << clickedPoint << QPointF(firstPt.x(), clickedPoint.y()) << firstPt;
                    m_savedPolygons << m_currentPolygon;
                    m_currentPolygon.clear();
                }

                repaint();
            }
        }
    }

    QWidget::mousePressEvent(event);
}

void AnnotatorWidget::repaint()
{
    for (QGraphicsItem* item : m_items)
        m_ui->graphicsView->scene()->removeItem(item);

    m_items.clear();

    for (Polygon& polygon : m_savedPolygons)
        paintPolygon(polygon);

    paintPolygon(m_currentPolygon);
}

void AnnotatorWidget::paintPolygon(Polygon& polygon)
{
    QGraphicsScene *scene = m_ui->graphicsView->scene();

    if (polygon.size() > 1 && polygon.isClosed()) {
        QColor color(polygon.polygonClass()->color());
        color.setAlpha(35);

        QGraphicsPolygonItem *pol = scene->addPolygon(polygon, QPen(polygon.polygonClass()->color(), 2), QBrush(color));

        m_items << pol;
    }
    else {
        for (auto it = polygon.begin(); it != polygon.end(); ++it) {
            QGraphicsItem* item;

            if (it == polygon.begin()) {
                QBrush brush(polygon.polygonClass()->color());

                item = scene->addRect((*it).x(), (*it).y(), 10, 10, QPen(brush, 2), brush);
            }
            else
                item = scene->addLine(QLineF(*(it - 1), *it), QPen(QBrush(polygon.polygonClass()->color()), 2));

            m_items << item;
        }
    }
}

void AnnotatorWidget::undo()
{
    if (!m_currentPolygon.empty()) {
        m_currentPolygon.pop_back();

        repaint();
    }
}

void AnnotatorWidget::reset()
{
    m_savedPolygons.clear();
    m_currentPolygon.clear();
    repaint();
}

void AnnotatorWidget::changeItem(QString itemPath)
{
    // TODO: create a temp file where the polygons from this image will be temporary stored, so they can be loaded again when
    // the image is reopened
    m_savedPolygons.clear();
    m_items.clear();
    m_currentPolygon.clear();

    QGraphicsScene *scene = m_ui->graphicsView->scene();
    scene->setSceneRect(0, 0, 850, 640);
    clearScene();

    QPixmap image(itemPath);
    QPixmap scaledImage;

    if (image.height() >= 1280)
        scaledImage = image.scaledToHeight(int(1280 * 0.8));
    if (image.width() >= 960)
        scaledImage = image.scaledToWidth(int(960 * 0.8));

    if (!scaledImage.isNull()) {
        m_scaleW = qreal(scaledImage.width()) / qreal(image.width());
        m_scaleH = qreal(scaledImage.height()) / qreal(image.height());
        image = scaledImage;
    }

    QGraphicsPixmapItem *pixmapItem = scene->addPixmap(image);

    int x_scene = int(scene->width() / 2);
    int y_scene = int(scene->height() / 2);
    int x_image = int(image.width() / 2);
    int y_image = int(image.height() / 2);

    pixmapItem->setPos(x_scene - x_image, y_scene - y_image);

    m_currentImage = pixmapItem;
}

void AnnotatorWidget::clearScene()
{
    m_ui->graphicsView->scene()->clear();
}
