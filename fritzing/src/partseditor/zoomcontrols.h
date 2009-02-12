/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2009 Fachhochschule Potsdam - http://fh-potsdam.de

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

$Revision: 1490 $:
$Author: cohen@irascible.com $:
$Date: 2008-11-13 13:10:48 +0100 (Thu, 13 Nov 2008) $

********************************************************************/

#ifndef ZOOMCONTROLS_H_
#define ZOOMCONTROLS_H_

#include <QGraphicsProxyWidget>
#include <QLabel>
#include <QBoxLayout>

#include "../sketchwidget.h"
#include "../help/inotseeninminiview.h"


class GraphicsZoomControls : public QGraphicsProxyWidget, public INotSeenInMiniView {
public:
	enum ZoomType {ZoomIn, ZoomOut};
	GraphicsZoomControls(SketchWidget *view);
};

class ZoomButton : public QLabel {
	Q_OBJECT

	public:
		ZoomButton(QBoxLayout::Direction dir, GraphicsZoomControls::ZoomType type, SketchWidget* view, QWidget *parent);

	signals:
		void clicked();

	protected slots:
		void zoom();

	protected:
		void enterEvent(QEvent *event);
		void leaveEvent(QEvent *event);
		void mousePressEvent(QMouseEvent *event);

		SketchWidget *m_owner;
		qreal m_step;
		GraphicsZoomControls::ZoomType m_type;
};

class ZoomControlsPrivate : public QFrame {
public:
	ZoomControlsPrivate(SketchWidget*, QBoxLayout::Direction = QBoxLayout::TopToBottom, QWidget *parent=0);

	ZoomButton *zoomInButton();
	ZoomButton *zoomOutButton();

protected:
	ZoomButton *m_zoomInButton;
	ZoomButton *m_zoomOutButton;

	QBoxLayout *m_boxLayout;
};

class ZoomControls : public ZoomControlsPrivate {
	Q_OBJECT
	public:
		ZoomControls(SketchWidget *view, QWidget *parent);

	protected slots:
		void updateLabel(qreal zoom);

	protected:
		QLabel *m_zoomLabel;
};

#endif /* ZOOMCONTROLS_H_ */
