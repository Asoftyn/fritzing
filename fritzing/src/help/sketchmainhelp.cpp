/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-08 Fachhochschule Potsdam - http://fh-potsdam.de

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

#include <QHBoxLayout>
#include <QGraphicsScene>

#include "sketchmainhelp.h"
#include "../expandinglabel.h"

qreal SketchMainHelp::OpacityLevel = 0.5;

SketchMainHelpCloseButton::SketchMainHelpCloseButton(const QString &imagePath, QWidget *parent)
	:QLabel(parent)
{
	QString pathAux = ":/resources/images/inViewHelpCloseButton%1.png";
	setPixmap(QPixmap(pathAux.arg(imagePath)));
}

void SketchMainHelpCloseButton::mousePressEvent(QMouseEvent * event) {
	emit clicked();
	QLabel::mousePressEvent(event);
}


//////////////////////////////////////////////////////////////

SketchMainHelpPrivate::SketchMainHelpPrivate (
		const QString &viewString,
		const QString &imagePath,
		const QString &htmlText,
		SketchMainHelp *parent)
	: QFrame()
{
	m_parent = parent;

	QFrame *main = new QFrame(this);
	QHBoxLayout *mainLayout = new QHBoxLayout(main);
	QLabel *imageLabel = new QLabel(this);
	imageLabel->setPixmap(QPixmap(imagePath));
	ExpandingLabel *textLabel = new ExpandingLabel(this);
	textLabel->setLabelText(htmlText);
	textLabel->setToolTip("");
	mainLayout->setSpacing(2);
	mainLayout->setMargin(2);
	mainLayout->addWidget(imageLabel);
	mainLayout->addWidget(textLabel);
	setFixedWidth(430);

	QVBoxLayout *layout = new QVBoxLayout(this);
	SketchMainHelpCloseButton *closeBtn = new SketchMainHelpCloseButton(viewString,this);
	connect(closeBtn, SIGNAL(clicked()), this, SLOT(doClose()));
	layout->addWidget(closeBtn);
	layout->addWidget(main);
	layout->setSpacing(2);
	layout->setMargin(2);

	m_shouldGetTransparent = false;
}

void SketchMainHelpPrivate::doClose() {
	m_parent->doClose();
}

void SketchMainHelpPrivate::enterEvent(QEvent * event) {
	if(m_shouldGetTransparent) {
		setWindowOpacity(1.0);
	}
	QFrame::enterEvent(event);
}

void SketchMainHelpPrivate::leaveEvent(QEvent * event) {
	if(m_shouldGetTransparent) {
		setWindowOpacity(SketchMainHelp::OpacityLevel);
	}
	QFrame::leaveEvent(event);
}


//////////////////////////////////////////////////////////////

SketchMainHelp::SketchMainHelp (
		const QString &viewString,
		const QString &imagePath,
		const QString &htmlText
	) : QGraphicsProxyWidget()
{
	m_son = new SketchMainHelpPrivate(viewString, imagePath, htmlText, this);
	setWidget(m_son);
}


void SketchMainHelp::doClose() {
	scene()->removeItem(this);
}

void SketchMainHelp::applyAlpha() {
	m_son->setWindowOpacity(OpacityLevel);
	m_son->m_shouldGetTransparent = true;
}
