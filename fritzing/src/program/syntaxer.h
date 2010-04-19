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

$Revision$:
$Author$:
$Date$

********************************************************************/


#ifndef SYNTAXER_H_
#define SYNTAXER_H_

#include <QDomDocument>
#include <QObject>

#include "trienode.h"

class CommentInfo 
{
public:
	CommentInfo(const QString & start, const QString & end, Qt::CaseSensitivity);

public:
	bool m_multiLine;
	QString m_start;
	QString m_end;
	int m_index;
	Qt::CaseSensitivity m_caseSensitive;
};

class Syntaxer : public QObject
{
	Q_OBJECT


public:
	Syntaxer();
	virtual ~Syntaxer();

	bool loadSyntax(const QString & filename);
	bool matches(const QString & string, TrieLeaf * & leaf);
	const CommentInfo * getCommentInfo(int ix);
	bool matchCommentStart(const QString & text, int offset, int & result, const CommentInfo * & resultCommentInfo);
	const QString & extensions();

public:
	static QString parseForName(const QString & filename);

protected:
	void loadList(QDomElement & list);


protected:
	TrieNode * m_trieRoot;
	QString m_name;
	QString m_extensions;
	QList<CommentInfo *> m_commentInfo;
};

class SyntaxerTrieLeaf : public TrieLeaf
{
public:
	SyntaxerTrieLeaf(QString name);
	~SyntaxerTrieLeaf();

protected:
	QString m_name;
};

#endif /* SYNTAXER_H_ */
