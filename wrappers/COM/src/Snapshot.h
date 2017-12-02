// Definition of the subscription interface

#pragma once

#include "DXFeed.h"

#include <ObjBase.h>

struct IDXSubscription;

/* -------------------------------------------------------------------------- */
/*
 *	DefDXSnapshotFactory class

 *  creates the instances implementing the IDXSubscription interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXSnapshotFactory {
	static IDXSubscription* CreateSnapshot(dxf_connection_t connection, int eventType, BSTR symbol,
	BSTR source, LONGLONG time, BOOL incremental, IUnknown* parent);
	static IDXSubscription* CreateSnapshot(dxf_connection_t connection, IDXCandleSymbol* symbol,
	LONGLONG time, BOOL incremental, IUnknown* parent);
};
