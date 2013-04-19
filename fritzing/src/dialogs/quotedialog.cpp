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

#include "quotedialog.h"
#include "../debugdialog.h"

#include <QHeaderView>
#include <QDesktopServices>
#include <QStyledItemDelegate>
#include <QPen>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFile>
#include <QUrl>
#include <QFrame>

//////////////////////////////////////////////////////

static CountCost TheCountCost[QuoteDialog::MessageCount];
static double TheArea;
static int TheBoardCount;
static QList<int> Counts;

double hundredths(double d) {
    double h = (int) (d * 100);
    return h / ((double) 100);
}

//////////////////////////////////////////////////////

// from http://qt.onyou.ch/2010/07/08/hide-vertical-grid-lines-in-qtableview/

// custom item delegate to draw grid lines around cells 
class CustomDelegate : public QStyledItemDelegate 
{ 
public: 
    CustomDelegate(QTableView* tableView); 
protected: 
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const; 
private: 
    QPen _gridPen; 
}; 
 
CustomDelegate::CustomDelegate(QTableView* tableView) 
{ 
    // create grid pen 
    int gridHint = tableView->style()->styleHint(QStyle::SH_Table_GridLineColor, new QStyleOptionViewItemV4()); 
    QColor gridColor(gridHint); 
    _gridPen = QPen(gridColor, 0, tableView->gridStyle()); 
} 
 
void CustomDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const 
{ 
    QStyledItemDelegate::paint(painter, option, index); 
 
    if (index.row() < 2) {
        QPen oldPen = painter->pen(); 
        painter->setPen(_gridPen); 
 

        // paint vertical lines 
        //painter->drawLine(option.rect.topRight(), option.rect.bottomRight()); //vertical lines are disabled
        // paint horizontal lines  
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight()); //draw only horizontal line
 
        painter->setPen(oldPen);
    }
} 

//////////////////////////////////////////////////////

QuoteDialog::QuoteDialog(bool full, QWidget *parent) : QDialog(parent) 
{
    QFile styleSheet(":/resources/styles/fritzing.qss");
    if (!styleSheet.open(QIODevice::ReadOnly)) {
        DebugDialog::debug("Unable to open :/resources/styles/fritzing.qss");
    } else {
    	this->setStyleSheet(styleSheet.readAll());
    }

    if (Counts.isEmpty()) {
        Counts << 1 << 2 << 5 << 10;
    }

	setWindowTitle(tr("Fab Quote"));

	QVBoxLayout * vLayout = new QVBoxLayout(this);

    QLabel * label = new QLabel(tr("Order your PCB from Fritzing Fab"));
    label->setObjectName("quoteOrder");
    vLayout->addWidget(label);

	m_messageLabel = new QLabel("");
    m_messageLabel->setObjectName("quoteMessage");
	vLayout->addWidget(m_messageLabel);

    m_tableWidget = new QTableWidget(3, MessageCount + 1);
    m_tableWidget->setObjectName("quoteTable");
    m_tableWidget->setShowGrid(false);

    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    m_tableWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_tableWidget->horizontalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    m_tableWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_tableWidget->setItemDelegate(new CustomDelegate(m_tableWidget));
    QStringList labels;
    labels << tr("Copies") << tr("Price per board") << tr("Price");
    int ix = 0;
    foreach (QString labl, labels) {
        QTableWidgetItem * item = new QTableWidgetItem(labl);
        item->setFlags(0);
        m_tableWidget->setItem(ix, 0, item);
        ix += 1;
    }

	vLayout->addWidget(m_tableWidget);

    label = new QLabel(tr("Please note that prices do not include shipping, possible additional taxes, or the checking fee."));
    label->setObjectName("quoteAdditional");
	vLayout->addWidget(label);
    if (!full) label->setVisible(false);

    label = new QLabel(tr("For more information on pricing see <a href='http://fab.fritzing.org/pricing'>http://fab.fritzing.org/pricing</a>."));
    label->setObjectName("quoteAdditional");
	vLayout->addWidget(label);
    label->setOpenExternalLinks(true);
    if (!full) label->setVisible(false);

    vLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Minimum, QSizePolicy::Expanding));

    QFrame * frame = new QFrame;
    QHBoxLayout * buttonLayout = new QHBoxLayout();

    QPushButton * pushButton = new QPushButton(tr("Visit Fritzing Fab"));
    pushButton->setObjectName("quoteVisitButton");
    connect(pushButton, SIGNAL(clicked()), this, SLOT(visitFritzingFab()));

    QPixmap pixmap(":resources/images/icons/visitFritzingFab.png");
    pushButton->setMaximumSize(pixmap.rect().size());
    pushButton->setMinimumSize(pixmap.rect().size());

    buttonLayout->addWidget(pushButton);
    buttonLayout->addSpacing(90);
    label = new QLabel();
	QPixmap order(":/resources/images/icons/toolbarOrderEnabled_icon.png");
	label->setPixmap(order);
    buttonLayout->addWidget(label);

    buttonLayout->addSpacerItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    frame->setLayout(buttonLayout);

	vLayout->addWidget(frame);
    if (!full) frame->setVisible(false);

    QDialogButtonBox * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
	buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	vLayout->addWidget(buttonBox);
    if (!full) buttonBox->setVisible(false);

    this->setLayout(vLayout);
}

QuoteDialog::~QuoteDialog() {
}

void QuoteDialog::setCountCost(int index, int count, double cost) {
    DebugDialog::debug(QString("quote dialog set count cost %1 %2").arg(count).arg(cost));

    if (index < 0) return;
    if (index >= MessageCount) return;

    TheCountCost[index].count = count;
    TheCountCost[index].cost = cost;
}

void QuoteDialog::setArea(double area, int boardCount) {
    TheBoardCount = boardCount;
    TheArea = area;
}

void QuoteDialog::setText() {
    DebugDialog::debug("quote dialog set text");
    QString msg = tr("The total area of the %n boards in this sketch is %1 cm%3 (%2 in%3).\n", "", TheBoardCount)
        .arg(hundredths(TheArea))
        .arg(hundredths(TheArea / (2.54 * 2.54)))
        .arg(QChar(178))
        ;
    msg += tr("Use Fritzing Fab to produce a PCB from your sketch and take advantage of the quantity discount we offer.");
    m_messageLabel->setText(msg);    
 
    for (int i = 0; i < MessageCount; i++) {
        int count = TheCountCost[i].count;
        double cost = TheCountCost[i].cost;
        if (count == 0) continue;
        if (cost == 0) continue;

        QTableWidgetItem * item = new QTableWidgetItem(QString::number(count));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setFlags(0);
        m_tableWidget->setItem(0, i + 1, item);
        item = new QTableWidgetItem(QString("%1").arg(hundredths(cost/count), 13, 'F', 2));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setFlags(0);
        m_tableWidget->setItem(1, i + 1, item);
        item = new QTableWidgetItem(QString("%1").arg(hundredths(cost), 13, 'F', 2));   
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        item->setFlags(0);
        m_tableWidget->setItem(2, i + 1, item);

    }

    // this is some bullshit because QTableWidget seems to allocate a huge amount of whitespace at right and bottom of the table
    int w = 10;
    for (int i = 0; i < m_tableWidget->columnCount(); i++) w += m_tableWidget->columnWidth(i); 
    int h = 4;
    for (int i = 0; i < m_tableWidget->rowCount(); i++) h += m_tableWidget->rowHeight(i);
    m_tableWidget->setMaximumSize(QSize(w, h));
    m_tableWidget->setMinimumSize(QSize(w, h));
}

void QuoteDialog::visitFritzingFab() {
    QDesktopServices::openUrl(QString("http://fab.fritzing.org/fritzing-fab"));
}

QString QuoteDialog::countArgs() {
    QString countArgs;
    foreach (int c, Counts) {
        countArgs += QString::number(c) + ",";
    }
    countArgs.chop(1);
    return countArgs;
}
