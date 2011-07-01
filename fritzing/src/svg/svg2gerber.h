/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2011 Fachhochschule Potsdam - http://fh-potsdam.de

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

#ifndef SVG2GERBER_H
#define SVG2GERBER_H

#include <QString>
#include <QDomElement>
#include <QObject>
#include <QMatrix>

class SVG2gerber : public QObject
{
    Q_OBJECT

public:
    SVG2gerber();

public:
	enum ForWhy {
		ForNormal,
		ForOutline,
		ForMask,
		ForDrill
	};

	int convert(const QString & svgStr, bool doubleSided, const QString & mainLayerName, ForWhy, QSizeF boardSize);
    QString getGerber();

protected:
    QDomDocument m_SVGDom;
    QString m_gerber_header;
    QString m_gerber_paths;
    QString m_drill_slots;
	QSizeF m_boardSize;

    qreal m_pathstart_x;
    qreal m_pathstart_y;

protected:

    void normalizeSVG();
    void convertShapes2paths(QDomNode);
    void flattenSVG(QDomNode);
    QMatrix parseTransform(QDomElement);

    QDomElement ellipse2path(QDomElement);

    void copyStyles(QDomElement, QDomElement);

    int renderGerber(bool doubleSided, const QString & mainLayerName, ForWhy);
    int allPaths2gerber(ForWhy);
    QString path2gerber(QDomElement);
	void handleOblongPath(QDomElement & path, int & dcode_index);
	QString standardAperture(QDomElement & element, QHash<QString, QString> & apertureMap, QString & current_dcode, int & dcode_index, qreal stroke_width);
	int flipx(qreal x);
	int flipy(qreal y);
	qreal flipxNoRound(qreal x);
	qreal flipyNoRound(qreal y);



protected slots:
    void path2gerbCommandSlot(QChar command, bool relative, QList<double> & args, void * userData);


};

#endif // SVG2GERBER_H
