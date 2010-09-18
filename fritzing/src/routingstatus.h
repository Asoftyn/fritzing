/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2010 Fachhochschule Potsdam - http://fh-potsdam.de

Fritzing is free software: you can redistribute it and/or modify\
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

$Revision: 4434 $:
$Author: cohen@irascible.com $:
$Date: 2010-09-17 23:18:17 +0200 (Fri, 17 Sep 2010) $

********************************************************************/

#ifndef ROUTINGSTATUS_H
#define ROUTINGSTATUS_H

#include "items/itembase.h"

struct RoutingStatus {
	int m_netCount;
	int m_netRoutedCount;
	int m_connectorsLeftToRoute;
	int m_jumperWireCount;
	int m_jumperItemCount;
	ConnectorPairHash m_unroutedConnectors;

public:
	void zero() {
		m_unroutedConnectors.clear();
		m_netCount = m_netRoutedCount = m_connectorsLeftToRoute = m_jumperWireCount = m_jumperItemCount = 0;
	}

	bool operator!=(const RoutingStatus &other) const {
		return 
			(m_netCount != other.m_netCount) ||
			(m_netRoutedCount != other.m_netRoutedCount) ||
			(m_connectorsLeftToRoute != other.m_connectorsLeftToRoute) ||
			(m_jumperWireCount != other.m_jumperWireCount) ||
			(m_jumperItemCount != other.m_jumperItemCount);
	}
};


#endif // ROUTINGSTATUS_H
