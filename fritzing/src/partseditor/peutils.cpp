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

#include "peutils.h"
#include "hashpopulatewidget.h"
#include "../debugdialog.h"
#include "../utils/graphicsutils.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QLabel>

QString PEUtils::Units = "mm";
const int PEUtils::Spacing = 10;


/////////////////////////////////////////////////////////

QString PEUtils::convertUnitsStr(double val)
{
    return QString::number(PEUtils::convertUnits(val));
}

double PEUtils::convertUnits(double val)
{
    if (Units.compare("in") == 0) {
        return val / GraphicsUtils::SVGDPI;
    }
    else if (Units.compare("mm") == 0) {
        return val * 25.4 / GraphicsUtils::SVGDPI;
    }

    return val;
}

double PEUtils::unconvertUnits(double val)
{
    if (Units.compare("in") == 0) {
        return val * GraphicsUtils::SVGDPI;
    }
    else if (Units.compare("mm") == 0) {
        return val * GraphicsUtils::SVGDPI / 25.4;
    }

    return val;
}

QWidget * PEUtils::makeConnectorForm(const QDomElement & connector, int index, QObject * slotHolder, bool alternating)
{
    QFrame * frame = new QFrame();
    if (alternating) {
        frame->setObjectName(index % 2 == 0 ? "NewPartsEditorConnector0Frame" : "NewPartsEditorConnector1Frame");
    }
    else {
        frame->setObjectName("NewPartsEditorConnectorFrame");
    }
    QVBoxLayout * mainLayout = new QVBoxLayout();
    mainLayout->setMargin(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

    QFrame * nameFrame = new QFrame();
    QHBoxLayout * nameLayout = new QHBoxLayout();

    QLabel * justLabel = new QLabel(QObject::tr("<b>id:</b>"));
	justLabel->setObjectName("NewPartsEditorLabel");
    nameLayout->addWidget(justLabel);

    justLabel = new QLabel(connector.attribute("id"));
	justLabel->setObjectName("NewPartsEditorLabel");
    nameLayout->addWidget(justLabel);
    nameLayout->addSpacing(Spacing);

    justLabel = new QLabel(QObject::tr("<b>Name:</b>"));
	justLabel->setObjectName("NewPartsEditorLabel");
    nameLayout->addWidget(justLabel);

    QLineEdit * nameEdit = new QLineEdit();
    nameEdit->setText(connector.attribute("name"));
	QObject::connect(nameEdit, SIGNAL(editingFinished()), slotHolder, SLOT(nameEntry()));
	nameEdit->setObjectName("NewPartsEditorLineEdit");
    nameEdit->setStatusTip(QObject::tr("Set the connectors's title"));
    nameEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    nameEdit->setProperty("index", index);
    nameEdit->setProperty("type", "name");
    nameEdit->setProperty("id", connector.attribute("id"));
    nameLayout->addWidget(nameEdit);
    nameLayout->addSpacing(Spacing);

    Connector::ConnectorType ctype = Connector::Male;
    if (connector.attribute("type").compare("female", Qt::CaseInsensitive) == 0) ctype = Connector::Female;
    else if (connector.attribute("type").compare("pad", Qt::CaseInsensitive) == 0) ctype = Connector::Pad;

    QRadioButton * radioButton = new QRadioButton(MaleSymbolString); 
	QObject::connect(radioButton, SIGNAL(clicked()), slotHolder, SLOT(typeEntry()));
    radioButton->setObjectName("NewPartsEditorRadio");
    if (ctype == Connector::Male) radioButton->setChecked(true); 
    radioButton->setProperty("value", Connector::Male);
    radioButton->setProperty("index", index);
    radioButton->setProperty("type", "radio");
    nameLayout->addWidget(radioButton);

    radioButton = new QRadioButton(FemaleSymbolString); 
	QObject::connect(radioButton, SIGNAL(clicked()), slotHolder, SLOT(typeEntry()));
    radioButton->setObjectName("NewPartsEditorRadio");
    if (ctype == Connector::Female) radioButton->setChecked(true); 
    radioButton->setProperty("value", Connector::Female);
    radioButton->setProperty("index", index);
    radioButton->setProperty("type", "radio");
    nameLayout->addWidget(radioButton);

    radioButton = new QRadioButton(QObject::tr("SMD-pad")); 
	QObject::connect(radioButton, SIGNAL(clicked()), slotHolder, SLOT(typeEntry()));
    radioButton->setObjectName("NewPartsEditorRadio");
    if (ctype == Connector::Pad) radioButton->setChecked(true); 
    radioButton->setProperty("value", Connector::Pad);
    nameLayout->addWidget(radioButton);
    radioButton->setProperty("index", index);
    radioButton->setProperty("type", "radio");
    nameLayout->addSpacing(Spacing);

    HashRemoveButton * hashRemoveButton = new HashRemoveButton(NULL, NULL, NULL);
    hashRemoveButton->setProperty("index", index);
	QObject::connect(hashRemoveButton, SIGNAL(clicked(HashRemoveButton *)), slotHolder, SLOT(removeConnector()));
    nameLayout->addWidget(hashRemoveButton);

    nameFrame->setLayout(nameLayout);
    mainLayout->addWidget(nameFrame);

    QFrame * descriptionFrame = new QFrame();
    QHBoxLayout * descriptionLayout = new QHBoxLayout();

    justLabel = new QLabel(QObject::tr("<b>Description:</b>"));
	justLabel->setObjectName("NewPartsEditorLabel");
    descriptionLayout->addWidget(justLabel);

    QLineEdit * descriptionEdit = new QLineEdit();
    QDomElement description = connector.firstChildElement("description");
    descriptionEdit->setText(description.text());
	QObject::connect(descriptionEdit, SIGNAL(editingFinished()), slotHolder, SLOT(descriptionEntry()));
	descriptionEdit->setObjectName("NewPartsEditorLineEdit");
    descriptionEdit->setStatusTip(QObject::tr("Set the connectors's description"));
    descriptionEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    descriptionEdit->setProperty("index", index);
    descriptionEdit->setProperty("type", "description");
    descriptionLayout->addWidget(descriptionEdit);

    descriptionFrame->setLayout(descriptionLayout);
    mainLayout->addWidget(descriptionFrame);
    frame->setLayout(mainLayout);
    return frame;
}
