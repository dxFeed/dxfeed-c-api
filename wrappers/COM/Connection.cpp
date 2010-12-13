// Implementation of the IDXConnection interface and the related stuff

#include "Connection.h"
#include "DispatchImpl.h"
#include "ConnectionPointImpl.h"
#include "Subscription.h"
#include "EventFactory.h"
#include "AuxAlgo.h"
#include "Guids.h"

#include "DXFeed.h"

#include <ComDef.h>
#include <crtdbg.h>

#include <string>

/* -------------------------------------------------------------------------- */
/*
 *	DXConnection class
 
 *  the default implementation of IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

class DXConnection : private IDXConnection, private DefIDispatchImpl,
                     private DefIConnectionPointImpl, private IDispBehaviorCustomizer {
    friend struct DefDXConnectionFactory;

    virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef () { return DefIDispatchImpl::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release () { ULONG res = ReleaseImpl(); if (res == 0) delete this; return res; }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount (UINT *pctinfo) { return DefIDispatchImpl::GetTypeInfoCount(pctinfo); }
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
        return DefIDispatchImpl::GetTypeInfo(iTInfo, lcid, ppTInfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames (REFIID riid, LPOLESTR *rgszNames,
                                                     UINT cNames, LCID lcid, DISPID *rgDispId) {
        return DefIDispatchImpl::GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
    }
    virtual HRESULT STDMETHODCALLTYPE Invoke (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                              DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                              EXCEPINFO *pExcepInfo, UINT *puArgErr) {
        return DefIDispatchImpl::Invoke(dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }
    
    virtual HRESULT STDMETHODCALLTYPE CreateSubscription (INT eventTypes, IDispatch** subscription);
    virtual HRESULT STDMETHODCALLTYPE GetLastEvent (INT eventType, BSTR symbol, IDispatch** eventData);

private:

    DXConnection (const char* address);
    
    static void OnConnectionTerminated (dxf_connection_t connection, void* user_data);
    void ClearNotifier ();
    
private:

    // IConnectionPoint interface implementation
    
    virtual HRESULT STDMETHODCALLTYPE Advise (IUnknown *pUnkSink, DWORD *pdwCookie);
    virtual HRESULT STDMETHODCALLTYPE Unadvise (DWORD dwCookie);

private:

    // IDispBehaviorCustomizer interface implementation
    
    virtual void OnBeforeDelete ();
                                              
private:

    dxf_connection_t m_connHandle;
    
    CriticalSection m_notifierGuard;
    IDispatch* m_termNotifier;
    DISPID m_notifierMethodId;
    VARIANTARG m_thisWrapper;
    DISPPARAMS m_notifierMethodParams;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXConnection methods implementation
 */
/* -------------------------------------------------------------------------- */

DXConnection::DXConnection (const char* address)
: DefIDispatchImpl(IID_IDXConnection)
, DefIConnectionPointImpl(DIID_IDXConnectionTerminationNotifier)
, m_connHandle(NULL)
, m_termNotifier(NULL)
, m_notifierMethodId(0) {
    if (dxf_create_connection(address, OnConnectionTerminated, reinterpret_cast<void*>(this), &m_connHandle) == DXF_FAILURE) {
        throw "Failed to establish connection";
    }
    
    SetBehaviorCustomizer(this);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::QueryInterface (REFIID riid, void **ppvObject) {
    if (ppvObject == NULL) {
        return E_POINTER;
    }
    
    *ppvObject = NULL;
    
    if (IsEqualIID(riid, IID_IConnectionPointContainer)) {
        *ppvObject = static_cast<IConnectionPointContainer*>(this);

        AddRef();
        
        return NOERROR;
    }

    return DefIDispatchImpl::QueryInterface(riid, ppvObject);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::CreateSubscription (INT eventTypes, IDispatch** subscription) {
    if (subscription == NULL) {
        return E_POINTER;
    }
    
    IUnknown* parent = static_cast<IDXConnection*>(this);
    
    if ((*subscription = DefDXSubscriptionFactory::CreateInstance(m_connHandle, eventTypes, parent)) == NULL) {
        return E_FAIL;
    }
    
    return NOERROR;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::GetLastEvent (INT eventType, BSTR symbol, IDispatch** eventData) {
    if (eventData == NULL) {
        return E_POINTER;
    }
    
    *eventData = NULL;
    
    _bstr_t symbolWrapper;
    HRESULT hr = E_FAIL;
    
    try {
        dxf_event_data_t eventDataHandle = NULL;
        
        symbolWrapper = _bstr_t(symbol, false);
        
        if (dxf_get_last_event(m_connHandle, eventType, (const wchar_t*)symbolWrapper, &eventDataHandle) == DXF_FAILURE) {
            throw "Failed to retrieve the last event data";
        }
        
        IUnknown* parent = static_cast<IDXConnection*>(this);
        
        if ((*eventData = EventDataFactory::CreateInstance(eventType, eventDataHandle, parent)) != NULL) {
            hr = NOERROR;
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    } catch (...) {    
    }
    
    symbolWrapper.Detach();
    
    return hr;
}

/* -------------------------------------------------------------------------- */

void DXConnection::OnConnectionTerminated (dxf_connection_t connection, void* user_data) {
    if (user_data == NULL) {
        return;
    }
    
    DXConnection* thisPtr = reinterpret_cast<DXConnection*>(user_data);
    
    _ASSERT(thisPtr->m_connHandle == connection);
    
    AutoCriticalSection acs(thisPtr->m_notifierGuard);
    
    if (thisPtr->m_termNotifier == NULL) {
        return;
    }
    
    thisPtr->AddRef(); // the pointer is passed as a function parameter, and it will be Release'd
    thisPtr->m_termNotifier->Invoke(thisPtr->m_notifierMethodId, IID_NULL, 0, DISPATCH_METHOD,
                                    &(thisPtr->m_notifierMethodParams), NULL, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

void DXConnection::ClearNotifier () {
    AutoCriticalSection acs(m_notifierGuard);
    
    if (m_termNotifier != NULL) {
        m_termNotifier->Release();
        m_termNotifier = NULL;
    }
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::Advise (IUnknown *pUnkSink, DWORD *pdwCookie) {
    if (pUnkSink == NULL || pdwCookie == NULL) {
        return E_POINTER;
    }
    
    *pdwCookie = 0;
    
    AutoCriticalSection acs(m_notifierGuard);
    
    if (m_termNotifier != NULL) {
        return CONNECT_E_ADVISELIMIT;
    }
    
    if (pUnkSink->QueryInterface(DIID_IDXConnectionTerminationNotifier, reinterpret_cast<void**>(&m_termNotifier)) != S_OK) {
        return CONNECT_E_CANNOTCONNECT;
    }
    
    if (DispUtils::GetMethodId(m_termNotifier, _bstr_t("OnConnectionTerminated"), OUT m_notifierMethodId) != S_OK) {
        ClearNotifier();
        
        return CONNECT_E_CANNOTCONNECT;
    }

    VariantInit(&m_thisWrapper);

    m_thisWrapper.vt = VT_DISPATCH;
    m_thisWrapper.pdispVal = static_cast<IDXConnection*>(this);

    m_notifierMethodParams.rgvarg = &m_thisWrapper;
    m_notifierMethodParams.rgdispidNamedArgs = NULL;
    m_notifierMethodParams.cArgs = 1;
    m_notifierMethodParams.cNamedArgs = 0;

    *pdwCookie = reinterpret_cast<DWORD>(m_termNotifier);
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::Unadvise (DWORD dwCookie) {
    AutoCriticalSection acs(m_notifierGuard);
    
    if (m_termNotifier == NULL) {
        return CONNECT_E_NOCONNECTION;
    }
    
    if (dwCookie != reinterpret_cast<DWORD>(m_termNotifier)) {
        return E_POINTER;
    }
    
    ClearNotifier();
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXConnection::OnBeforeDelete () {
    if (m_connHandle != NULL) {
        dxf_close_connection(m_connHandle);
        
        m_connHandle = NULL;
    }
    
    ClearNotifier();
}

/* -------------------------------------------------------------------------- */
/*
 *	DefDXConnectionFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDXConnection* DefDXConnectionFactory::CreateInstance (const char* address) {
    IDXConnection* connection = NULL;
    
    try {
        connection = new DXConnection(address);
        
        connection->AddRef();
    } catch (...) {        
    }
    
    return connection;
}