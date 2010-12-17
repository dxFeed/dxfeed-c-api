// Definition of the subscription interface

#pragma once

#include "DXFeed.h"

#include <ObjBase.h>

struct IDXSubscription;
struct IDXEventDataCollection;

/* -------------------------------------------------------------------------- */
/*
 *	DefDXSubscriptionFactory class
 
 *  creates the instances implementing the IDXSubscription interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXSubscriptionFactory {
    static IDXSubscription* CreateInstance (dxf_connection_t connection, int eventTypes, IUnknown* parent);
};

/* -------------------------------------------------------------------------- */
/*
 *	DefDXEventDataCollectionFactory class
 */
/* -------------------------------------------------------------------------- */

struct DefDXEventDataCollectionFactory {
    static IDXEventDataCollection* CreateInstance (int eventType, const dxf_event_data_t* eventData,
                                                   int eventCount, IUnknown* parent);
};