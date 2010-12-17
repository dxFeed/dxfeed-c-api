// Implementation of the IDXSubscription interface and the related stuff

#include "Subscription.h"
#include "DispatchImpl.h"
#include "ConnectionPointImpl.h"
#include "Guids.h"
#include "EventFactory.h"
#include "Interfaces.h"

#include <ComDef.h>

#include <set>

/* -------------------------------------------------------------------------- */
/*
 *	DXSubscription class
 */
/* -------------------------------------------------------------------------- */

class DXSubscription : private IDXSubscription, private DefIDispatchImpl,
                       private DefIConnectionPointImpl, private IDispBehaviorCustomizer {
    friend struct DefDXSubscriptionFactory;
    
    typedef std::set<IDispatch*> listener_set_t;
    typedef std::vector<VARIANTARG> variant_vector_t;
    
private:    
    
    virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef () { return AddRefImpl(); }
    virtual ULONG STDMETHODCALLTYPE Release () { ULONG res = ReleaseImpl(); if (res == 0) delete this; return res; }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount (UINT *pctinfo) {
        return GetTypeInfoCountImpl(pctinfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
        return GetTypeInfoImpl(iTInfo, lcid, ppTInfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames (REFIID riid, LPOLESTR *rgszNames,
                                                     UINT cNames, LCID lcid, DISPID *rgDispId) {
        return GetIDsOfNamesImpl(riid, rgszNames, cNames, lcid, rgDispId);
    }
    virtual HRESULT STDMETHODCALLTYPE Invoke (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                              DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                              EXCEPINFO *pExcepInfo, UINT *puArgErr) {
        return InvokeImpl(this, dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }
    
    virtual HRESULT STDMETHODCALLTYPE AddSymbol (BSTR symbol);
    virtual HRESULT STDMETHODCALLTYPE AddSymbols (SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbol (BSTR symbol);
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbols (SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE GetSymbols (SAFEARRAY** symbols);
    virtual HRESULT STDMETHODCALLTYPE SetSymbols (SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE ClearSymbols ();
    virtual HRESULT STDMETHODCALLTYPE GetEventTypes (INT* eventTypes);
    
private:

    DXSubscription (dxf_connection_t connection, int eventTypes, IUnknown* parent);
    
    static void OnNewData (int eventType, dxf_const_string_t symbol, const dxf_event_data_t* data,
                           int dataCount, void* userData);
    void ClearListeners ();
    void CreateListenerParams (IDispatch* subscription, int eventType, _bstr_t& symbol, IDispatch* dataCollection,
                               variant_vector_t& storage, OUT DISPPARAMS& params);
private:

    // IConnectionPoint interface implementation

    virtual HRESULT STDMETHODCALLTYPE Advise (IUnknown *pUnkSink, DWORD *pdwCookie);
    virtual HRESULT STDMETHODCALLTYPE Unadvise (DWORD dwCookie);

private:

    // IDispBehaviorCustomizer interface implementation

    virtual void OnBeforeDelete ();
    
private:

    int m_eventTypes;
    dxf_subscription_t m_subscrHandle;
    
    CriticalSection m_listenerGuard;
    listener_set_t m_listeners;
    DISPID m_listenerMethodId;
};

/* -------------------------------------------------------------------------- */

DXSubscription::DXSubscription (dxf_connection_t connection, int eventTypes, IUnknown* parent)
: DefIDispatchImpl(IID_IDXSubscription, parent)
, DefIConnectionPointImpl(DIID_IDXEventListener)
, m_eventTypes(0)
, m_subscrHandle(NULL)
, m_listenerMethodId(0) {
    SetBehaviorCustomizer(this);
    
    if (dxf_create_subscription(connection, eventTypes, &m_subscrHandle) == DXF_FAILURE) {
        throw "Failed to create a subscription";
    }
    
    if (DispUtils::GetMethodId(DIID_IDXEventListener, _bstr_t("OnNewData"), OUT m_listenerMethodId) != S_OK) {
        m_listenerMethodId = 0;
    }
    
    if (dxf_attach_event_listener(m_subscrHandle, OnNewData, this) != DXF_SUCCESS) {
        throw "Failed to attach an internal event listener";
    }
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::QueryInterface (REFIID riid, void **ppvObject) {
    if (ppvObject == NULL) {
        return E_POINTER;
    }

    *ppvObject = NULL;

    if (IsEqualIID(riid, IID_IConnectionPointContainer)) {
        *ppvObject = static_cast<IConnectionPointContainer*>(this);

        AddRef();

        return NOERROR;
    }

    return QueryInterfaceImpl(this, riid, ppvObject);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::AddSymbol (BSTR symbol) {
    _bstr_t symbolWrapper;
    HRESULT hr = E_FAIL;
    
    try {
        symbolWrapper = _bstr_t(symbol, false);
        
        if (dxf_add_symbol(m_subscrHandle, (const wchar_t*)symbolWrapper) == DXF_SUCCESS) {
            hr = S_OK;
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    }
    
    symbolWrapper.Detach();
    
    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::AddSymbols (SAFEARRAY* symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }
    
    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    HRESULT hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);
    
    if (hr != S_OK) {
        return hr;
    }
    
    if (dxf_add_symbols(m_subscrHandle, &(symbolArray[0]), int(symbolArray.size())) == DXF_FAILURE) {
        return E_FAIL;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::RemoveSymbol (BSTR symbol) {
    _bstr_t symbolWrapper;
    HRESULT hr = E_FAIL;

    try {
        symbolWrapper = _bstr_t(symbol, false);

        if (dxf_remove_symbol(m_subscrHandle, (const wchar_t*)symbolWrapper) == DXF_SUCCESS) {
            hr = S_OK;
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    }

    symbolWrapper.Detach();

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::RemoveSymbols (SAFEARRAY* symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }

    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    HRESULT hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);

    if (hr != S_OK) {
        return hr;
    }

    if (dxf_remove_symbols(m_subscrHandle, &(symbolArray[0]), int(symbolArray.size())) == DXF_FAILURE) {
        return E_FAIL;
    }

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::GetSymbols (SAFEARRAY** symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }
    
    dxf_const_string_t* symbolArray;
    int symbolCount;
    
    if (dxf_get_symbols(m_subscrHandle, &symbolArray, &symbolCount) == DXF_FAILURE) {
        return E_FAIL;
    }
    
    HRESULT hr = DispUtils::StringArrayToSafeArray(symbolArray, symbolCount, *symbols);
    
    if (hr != S_OK) {
        symbols = NULL;
        
        return hr;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::SetSymbols (SAFEARRAY* symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }

    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    HRESULT hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);

    if (hr != S_OK) {
        return hr;
    }

    if (dxf_set_symbols(m_subscrHandle, &(symbolArray[0]), int(symbolArray.size())) == DXF_FAILURE) {
        return E_FAIL;
    }

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::ClearSymbols () {
    if (dxf_clear_symbols(m_subscrHandle) == DXF_FAILURE) {
        return E_FAIL;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::GetEventTypes (INT* eventTypes) {
    if (eventTypes == NULL) {
        return E_POINTER;
    }
    
    if (dxf_get_subscription_event_types(m_subscrHandle, eventTypes) == DXF_FAILURE) {
        *eventTypes = 0;
        
        return E_FAIL;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXSubscription::OnNewData (int eventType, dxf_const_string_t symbol, const dxf_event_data_t* data,
                                int dataCount, void* userData) {
    if (userData == NULL) {
        return;
    }
    
    DXSubscription* thisPtr = reinterpret_cast<DXSubscription*>(userData);
    
    AutoCriticalSection acs(thisPtr->m_listenerGuard);
    
    if (thisPtr->m_listeners.empty()) {
        return;
    }
    
    IDXEventDataCollection* dataCollection = DefDXEventDataCollectionFactory::CreateInstance(eventType, data, dataCount,
                                                                                             static_cast<IDXSubscription*>(thisPtr));
    if (dataCollection == NULL) {
        return;
    }
    
    IUnknownWrapper dcw(dataCollection);
    
    try {
        listener_set_t::const_iterator it = thisPtr->m_listeners.begin();
        listener_set_t::const_iterator itEnd = thisPtr->m_listeners.end();
        variant_vector_t storage;
        DISPPARAMS listenerParams;
        _bstr_t symbolWrapper(symbol);

        thisPtr->CreateListenerParams(static_cast<IDXSubscription*>(thisPtr), eventType, symbolWrapper,
                                      dataCollection, storage, listenerParams);

        for (; it != itEnd; ++it) {
            (*it)->Invoke(thisPtr->m_listenerMethodId, IID_NULL, 0, DISPATCH_METHOD, &listenerParams, NULL, NULL, NULL);
        }
    } catch (...) {    
    }
}

/* -------------------------------------------------------------------------- */

void DXSubscription::ClearListeners () {
    AutoCriticalSection acs(m_listenerGuard);
    
    listener_set_t::iterator it = m_listeners.begin();
    listener_set_t::iterator itEnd = m_listeners.end();
    
    for (; it != itEnd; ++it) {
        (*it)->Release();
        *it = NULL;
    }
}

/* -------------------------------------------------------------------------- */

void DXSubscription::CreateListenerParams (IDispatch* subscription, int eventType, _bstr_t& symbol,
                                           IDispatch* dataCollection, variant_vector_t& storage,
                                           OUT DISPPARAMS& params) {
    storage.clear();
    storage.reserve(4);
    
    VARIANTARG paramArg;
    
    // the parameters must be packed in a reverse order
    
    ::VariantInit(&paramArg);
    paramArg.vt = VT_DISPATCH;
    paramArg.pdispVal = dataCollection;
    storage.push_back(paramArg);
    
    ::VariantInit(&paramArg);
    paramArg.vt = VT_BSTR;
    paramArg.bstrVal = symbol.GetBSTR();
    storage.push_back(paramArg);
    
    ::VariantInit(&paramArg);
    paramArg.vt = VT_INT;
    paramArg.intVal = eventType;
    storage.push_back(paramArg);
    
    ::VariantInit(&paramArg);
    paramArg.vt = VT_DISPATCH;
    paramArg.pdispVal = subscription;
    storage.push_back(paramArg);
    
    params.rgvarg = &storage[0];
    params.rgdispidNamedArgs = NULL;
    params.cArgs = 4;
    params.cNamedArgs = 0;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::Advise (IUnknown *pUnkSink, DWORD *pdwCookie) {
    if (pUnkSink == NULL || pdwCookie == NULL) {
        return E_POINTER;
    }

    *pdwCookie = 0;
    
    IDispatch* listener = NULL;

    if (pUnkSink->QueryInterface(DIID_IDXEventListener, reinterpret_cast<void**>(&listener)) != S_OK) {
        return CONNECT_E_CANNOTCONNECT;
    }
    
    AutoCriticalSection acs(m_listenerGuard);
    
    if (m_listeners.find(listener) != m_listeners.end()) {
        // this listener is already added
        
        return S_OK;
    }
    
    if (m_listenerMethodId == 0) {        
        return DISP_E_UNKNOWNNAME;
    }
    
    m_listeners.insert(listener);
    *pdwCookie = reinterpret_cast<DWORD>(listener);

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSubscription::Unadvise (DWORD dwCookie) {
    AutoCriticalSection acs(m_listenerGuard);

    IDispatch* listener = reinterpret_cast<IDispatch*>(dwCookie);
    listener_set_t::iterator it = m_listeners.find(listener);

    if (it == m_listeners.end()) {
        return E_POINTER;
    }

    listener->Release();
    m_listeners.erase(it);

    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXSubscription::OnBeforeDelete () {
    dxf_close_subscription(m_subscrHandle);
    
    ClearListeners();
}

/* -------------------------------------------------------------------------- */
/*
 *	DefDXSubscriptionFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDXSubscription* DefDXSubscriptionFactory::CreateInstance (dxf_connection_t connection, int eventTypes,
                                                           IUnknown* parent) {
    IDXSubscription* subscription = NULL;
    
    try {
        subscription = new DXSubscription(connection, eventTypes, parent);
        
        subscription->AddRef();
    } catch (...) {    
    }
    
    return subscription;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXEventDataCollection class
 
 *  the default implementation of the IDXEventDataCollection interface
 */
/* -------------------------------------------------------------------------- */

class DXEventDataCollection : private IDXEventDataCollection, private DefIDispatchImpl {
    friend struct DefDXEventDataCollectionFactory;
    
private:

    virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject) {
        return QueryInterfaceImpl(this, riid, ppvObject);
    }
    virtual ULONG STDMETHODCALLTYPE AddRef () { return AddRefImpl(); }
    virtual ULONG STDMETHODCALLTYPE Release () { ULONG res = ReleaseImpl(); if (res == 0) delete this; return res; }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount (UINT *pctinfo) { return GetTypeInfoCountImpl(pctinfo); }
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
        return GetTypeInfoImpl(iTInfo, lcid, ppTInfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames (REFIID riid, LPOLESTR *rgszNames,
                                                     UINT cNames, LCID lcid, DISPID *rgDispId) {
        return GetIDsOfNamesImpl(riid, rgszNames, cNames, lcid, rgDispId);
    }
    virtual HRESULT STDMETHODCALLTYPE Invoke (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                              DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                              EXCEPINFO *pExcepInfo, UINT *puArgErr) {
        return InvokeImpl(this, dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }
    
    HRESULT STDMETHODCALLTYPE GetEventCount (INT* count);
    HRESULT STDMETHODCALLTYPE GetEvent (INT index, IDispatch** eventData);
    
private:

    DXEventDataCollection (int eventType, const dxf_event_data_t* eventData, int eventCount, IUnknown* parent);
    
private:

    int m_eventType;
    dxf_event_data_t m_eventData;
    int m_eventCount;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXEventDataCollection methods implementation
 */
/* -------------------------------------------------------------------------- */

DXEventDataCollection::DXEventDataCollection (int eventType, const dxf_event_data_t* eventData, int eventCount, IUnknown* parent)
: DefIDispatchImpl(IID_IDXEventDataCollection, parent)
, m_eventType(eventType)
, m_eventData(reinterpret_cast<dxf_event_data_t>(const_cast<dxf_event_data_t*>(eventData)))
, m_eventCount(eventCount) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXEventDataCollection::GetEventCount (INT* count) {
    if (count == NULL) {
        return E_POINTER;
    }
    
    *count = m_eventCount;
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXEventDataCollection::GetEvent (INT index, IDispatch** eventData) {
    if (eventData == NULL) {
        return E_POINTER;
    }
    
    *eventData = NULL;
    
    if (index < 0 || index >= m_eventCount) {
        return E_FAIL;
    }
    
    dxf_event_data_t singleEventData = dx_get_event_data_item(m_eventType, m_eventData, index);
    
    if ((*eventData = EventDataFactory::CreateInstance(m_eventType, singleEventData, this)) == NULL) {
        return E_FAIL;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DefDXEventDataCollectionFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDXEventDataCollection* DefDXEventDataCollectionFactory::CreateInstance (int eventType, const dxf_event_data_t* eventData,
                                                                         int eventCount, IUnknown* parent) {
    IDXEventDataCollection* dataCollection = NULL;
    
    try {
        dataCollection = new DXEventDataCollection(eventType, eventData, eventCount, parent);
        
        dataCollection->AddRef();
    } catch (...) {    
    }
    
    return dataCollection;
}