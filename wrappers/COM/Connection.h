// Definition of the connection interface

#pragma once

#include <objbase.h>

/* -------------------------------------------------------------------------- */
/*
 *	IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

struct IDXConnection : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CreateSubscription (INT eventTypes, IDispatch** subscription) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastEvent (INT eventType, BSTR symbol, IDispatch** eventData) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	DefDXConnectionFactory class
 
 *  creates the instances implementing the IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXConnectionFactory {
    static IDXConnection* CreateInstance (const char* address);
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXConnectionTermination sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXConnectionTermination : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnConnectionTerminated (IDispatch* connection) = 0;
};