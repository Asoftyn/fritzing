/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2012 Fachhochschule Potsdam - http://fh-potsdam.de

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************

$Revision$:
$Author$:
$Date$

********************************************************************/

#ifndef DRC_H
#define DRC_H

#include <QList>
#include <QObject>
#include <QImage>
#include <QDomDocument>
#include <QGraphicsPixmapItem>

#include "../svg/svgfilesplitter.h"
#include "../viewlayer.h"

class DRC : public QObject
{
	Q_OBJECT

public:
	DRC(class PCBSketchWidget *, class ItemBase * board);
	virtual ~DRC(void);

	bool start(QString & message, QStringList & messages);

public slots:
	void cancel();

signals:
	void setMaximumProgress(int);
	void setProgressValue(int);
	void wantTopVisible();
	void wantBottomVisible();	
	void wantBothVisible();
	void setProgressMessage(const QString &);

protected:
    void makeHoles(QDomDocument *, QImage *, QRectF & sourceRes, ViewLayer::ViewLayerSpec);
    bool makeBoard(QImage *, QRectF & sourceRes);
    void splitNet(QDomDocument *, QList<class ConnectorItem *> & , QImage * minusImage, QImage * plusImage, QRectF & sourceRes, ViewLayer::ViewLayerSpec viewLayerSpec, double keepout, int index);
    void renderOne(QDomDocument * masterDoc, QImage * image, QRectF & sourceRes);
    void markSubs(QDomElement & root, const QString & mark);
    void splitSubs(QDomElement & root, const QString & mark1, const QString & mark2, const QStringList & svgIDs);
    void updateDisplay(double dpi);
	bool startAux(QString & message, QStringList & messages);
	
protected:
	class PCBSketchWidget * m_sketchWidget;
    class ItemBase * m_board;
	double m_keepout;
    QImage * m_plusImage;
    QImage * m_minusImage;
    QImage * m_displayImage;
    QGraphicsPixmapItem * m_displayItem;
    QHash<ViewLayer::ViewLayerSpec, QDomDocument *> m_masterDocs;
    bool m_cancelled;
    int m_maxProgress;
};

#endif
