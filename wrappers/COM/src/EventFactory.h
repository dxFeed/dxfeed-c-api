// Definition of the event data interfaces and the event factory class

#pragma once

#include "DXFeed.h"

#include <ObjBase.h>

/* -------------------------------------------------------------------------- */
/*
 *	EventDataFactory class
 */
/* -------------------------------------------------------------------------- */

struct EventDataFactory {
    static IDispatch* CreateInstance (int eventType, dxf_event_data_t eventData, IUnknown* parent);
};