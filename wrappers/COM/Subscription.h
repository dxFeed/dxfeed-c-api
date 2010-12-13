// Definition of the subscription interface

#pragma once

#include "DXFeed.h"

#include <ObjBase.h>

/* -------------------------------------------------------------------------- */
/*
 *	IDXSubscrption interface
 */
/* -------------------------------------------------------------------------- */

struct IDXSubscription : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE AddSymbol (BSTR symbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddSymbols (SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbol (BSTR symbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbols (SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSymbols (SAFEARRAY** symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSymbols (SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearSymbols () = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventTypes (INT* eventTypes) = 0;
};

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
 *	IDXEventDataCollection interface
 */
/* -------------------------------------------------------------------------- */

struct IDXEventDataCollection : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetEventCount (INT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEvent (INT index, IDispatch** eventData) = 0;
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

/* -------------------------------------------------------------------------- */
/*
 *	IDXEventListener sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXEventListener : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnNewData (IDispatch* subscription, INT eventType, BSTR symbol,
                                                 IDispatch* dataCollection) = 0;
};