// Implementation of the IDXSubscription interface and the related stuff

#include <ComDef.h>

#include <map>

#include "ConnectionPointImpl.h"
#include "DispatchImpl.h"
#include "EventFactory.h"
#include "Guids.h"
#include "Interfaces.h"
#include "NativeCandleSymbol.h"
#include "Snapshot.h"
#include "Subscription.h"

/* -------------------------------------------------------------------------- */
/*
*	DXSnapshot class
*/
/* -------------------------------------------------------------------------- */

class DXSnapshot : private IDXSubscription, private DefIDispatchImpl,
    private DefIConnectionPointImpl, private IDispBehaviorCustomizer {
    friend struct DefDXSnapshotFactory;

    typedef std::map<DWORD, IDispatch*> listener_map_t;
    typedef std::vector<VARIANTARG> variant_vector_t;

private:

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return AddRefImpl(); }
    virtual ULONG STDMETHODCALLTYPE Release() { ULONG res = ReleaseImpl(); if (res == 0) delete this; return res; }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo) {
        return GetTypeInfoCountImpl(pctinfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
        return GetTypeInfoImpl(iTInfo, lcid, ppTInfo);
    }
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames,
        UINT cNames, LCID lcid, DISPID *rgDispId) {
        return GetIDsOfNamesImpl(riid, rgszNames, cNames, lcid, rgDispId);
    }
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
        DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr) {
        return InvokeImpl(this, dispIdMember, riid, lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    }

    virtual HRESULT STDMETHODCALLTYPE AddSymbol(BSTR symbol);
    virtual HRESULT STDMETHODCALLTYPE AddSymbols(SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbol(BSTR symbol);
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbols(SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE GetSymbols(SAFEARRAY** symbols);
    virtual HRESULT STDMETHODCALLTYPE SetSymbols(SAFEARRAY* symbols);
    virtual HRESULT STDMETHODCALLTYPE ClearSymbols();
    virtual HRESULT STDMETHODCALLTYPE GetEventTypes(INT* eventTypes);
    virtual HRESULT STDMETHODCALLTYPE AddCandleSymbol(IDXCandleSymbol* symbol);
    virtual HRESULT STDMETHODCALLTYPE RemoveCandleSymbol(IDXCandleSymbol* symbol);

private:

    DXSnapshot(dxf_connection_t connection, int eventType, BSTR symbol, BSTR source,
        LONGLONG time, BOOL incremental, IUnknown* parent);
    DXSnapshot(dxf_connection_t connection, IDXCandleSymbol* symbol, LONGLONG time,
        BOOL incremental, IUnknown* parent);

    static void OnNewData(const dxf_snapshot_data_ptr_t snapshotData, void* userData);
    static void OnNewDataInc(const dxf_snapshot_data_ptr_t snapshotData, int new_snapshot, void* userData);
    void ClearListeners();
    void CreateListenerParams(IDispatch* subscription, int eventType, _bstr_t& symbol, IDispatch* dataCollection,
        variant_vector_t& storage, OUT DISPPARAMS& params);
    static dx_event_id_t GetEventId(int eventType);
    HRESULT GetSymbol(_bstr_t& symbol);
    HRESULT HasSymbol(bool& result);
    void CloseSnapshot();
private:

    // IConnectionPoint interface implementation

    virtual HRESULT STDMETHODCALLTYPE Advise(IUnknown *pUnkSink, DWORD *pdwCookie);
    virtual HRESULT STDMETHODCALLTYPE Unadvise(DWORD dwCookie);

private:

    // IDispBehaviorCustomizer interface implementation

    virtual void OnBeforeDelete();

private:

    dxf_connection_t m_connection;
    int m_eventType;
    _bstr_t m_source;
    LONGLONG m_time;
    BOOL m_incremental;
    dxf_snapshot_t m_snapshotHandle;

    CriticalSection m_listenerGuard;
    listener_map_t m_listeners;
    DISPID m_listenerMethodId;
    DISPID m_incListenerSnapMethodId;
    DISPID m_incListenerUpMethodId;
    DWORD m_listener_next_id;
};

/* -------------------------------------------------------------------------- */

DXSnapshot::DXSnapshot(dxf_connection_t connection, int eventType, BSTR symbol, BSTR source,
    LONGLONG time, BOOL incremental, IUnknown* parent)
    : DefIDispatchImpl(IID_IDXSubscription, parent)
    , DefIConnectionPointImpl(DIID_IDXEventListener)
    , m_connection(connection)
    , m_eventType(eventType)
    , m_source(source)
    , m_time(time)
    , m_incremental(incremental)
    , m_snapshotHandle(NULL)
    , m_listenerMethodId(0)
    , m_incListenerSnapMethodId(0)
    , m_incListenerUpMethodId(0)
    , m_listener_next_id(1) {
    SetBehaviorCustomizer(this);

    _bstr_t symbolWrapper(symbol);
    if (dxf_create_snapshot(connection, GetEventId(eventType), (const wchar_t*)symbolWrapper, (const char*)m_source, time, &m_snapshotHandle) == DXF_FAILURE) {
        throw "Failed to create a snapshot";
    }

    if (incremental) {
        if (DispUtils::GetMethodId(DIID_IDXIncrementalEventListener, _bstr_t("OnNewSnapshot"), OUT m_incListenerSnapMethodId) != S_OK) {
            m_incListenerSnapMethodId = 0;
        }
        if (DispUtils::GetMethodId(DIID_IDXIncrementalEventListener, _bstr_t("OnUpdate"), OUT m_incListenerUpMethodId) != S_OK) {
            m_incListenerUpMethodId = 0;
        }
    } else {
        if (DispUtils::GetMethodId(DIID_IDXEventListener, _bstr_t("OnNewData"), OUT m_listenerMethodId) != S_OK) {
            m_listenerMethodId = 0;
        }
    }

    if (incremental) {
        if (dxf_attach_snapshot_inc_listener(m_snapshotHandle, OnNewDataInc, this) != DXF_SUCCESS) {
            CloseSnapshot();
            throw "Failed to attach an internal event listener";
        }
    } else {
        if (dxf_attach_snapshot_listener(m_snapshotHandle, OnNewData, this) != DXF_SUCCESS) {
            CloseSnapshot();
            throw "Failed to attach an internal event listener";
        }
    }
}

/* -------------------------------------------------------------------------- */

DXSnapshot::DXSnapshot(dxf_connection_t connection, IDXCandleSymbol* symbol, LONGLONG time,
    BOOL incremental, IUnknown* parent)
    : DefIDispatchImpl(IID_IDXSubscription, parent)
    , DefIConnectionPointImpl(DIID_IDXEventListener)
    , m_connection(connection)
    , m_source()
    , m_time(time)
    , m_incremental(incremental)
    , m_eventType(DXF_ET_CANDLE)
    , m_snapshotHandle(NULL)
    , m_listenerMethodId(0)
    , m_incListenerSnapMethodId(0)
    , m_incListenerUpMethodId(0)
    , m_listener_next_id(1) {
    SetBehaviorCustomizer(this);

    NativeCandleSymbol nativeCandleSymbol(symbol);
    if (dxf_create_candle_snapshot(connection, *nativeCandleSymbol, time, &m_snapshotHandle) == DXF_FAILURE) {
        throw "Failed to create a snapshot";
    }

    if (incremental) {
        if (DispUtils::GetMethodId(DIID_IDXIncrementalEventListener, _bstr_t("OnNewSnapshot"), OUT m_incListenerSnapMethodId) != S_OK) {
            m_incListenerSnapMethodId = 0;
        }
        if (DispUtils::GetMethodId(DIID_IDXIncrementalEventListener, _bstr_t("OnUpdate"), OUT m_incListenerUpMethodId) != S_OK) {
            m_incListenerUpMethodId = 0;
        }
    } else {
        if (DispUtils::GetMethodId(DIID_IDXEventListener, _bstr_t("OnNewData"), OUT m_listenerMethodId) != S_OK) {
            m_listenerMethodId = 0;
        }
    }

    if (incremental) {
        if (dxf_attach_snapshot_inc_listener(m_snapshotHandle, OnNewDataInc, this) != DXF_SUCCESS) {
            CloseSnapshot();
            throw "Failed to attach an internal event listener";
        }
    } else {
        if (dxf_attach_snapshot_listener(m_snapshotHandle, OnNewData, this) != DXF_SUCCESS) {
            CloseSnapshot();
            throw "Failed to attach an internal event listener";
        }
    }
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::QueryInterface(REFIID riid, void **ppvObject) {
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

HRESULT STDMETHODCALLTYPE DXSnapshot::AddSymbol(BSTR symbol) {
    bool hasSymbol;
    HRESULT hr = HasSymbol(hasSymbol);
    if (FAILED(hr))
        return hr;
    if (hasSymbol)
        return S_FALSE;

    if (symbol == NULL || SysStringLen(symbol) == 0)
        return E_INVALIDARG;

    _bstr_t symbolWrapper;

    hr = E_FAIL;
    try {
        symbolWrapper = _bstr_t(symbol, true);

        if (dxf_create_snapshot(m_connection, GetEventId(m_eventType), (const wchar_t*)symbolWrapper, (const char*)m_source, m_time, &m_snapshotHandle) == DXF_SUCCESS) {
            if (dxf_attach_snapshot_listener(m_snapshotHandle, OnNewData, this) == DXF_SUCCESS) {
                hr = S_OK;
            } else {
                CloseSnapshot();
            }
        }

    } catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::AddSymbols(SAFEARRAY* symbols) {
    bool hasSymbol;
    HRESULT hr = HasSymbol(hasSymbol);
    if (FAILED(hr))
        return hr;
    if (hasSymbol)
        return S_FALSE;

    if (symbols == NULL) {
        return E_POINTER;
    }

    if (symbols->cDims != 1)
        return E_INVALIDARG;

    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);

    if (hr != S_OK) {
        return hr;
    }

    return AddSymbol(_bstr_t(symbolArray[0]));
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::RemoveSymbol(BSTR symbol) {
    if (symbol == NULL || SysStringLen(symbol) == 0)
        return E_INVALIDARG;

    _bstr_t current;
    HRESULT hr = GetSymbol(current);
    if (FAILED(hr))
        return hr;

    if (wcscmp(symbol, (const wchar_t*)current) == 0)
        CloseSnapshot();

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::RemoveSymbols(SAFEARRAY* symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }

    _bstr_t current;
    HRESULT hr = GetSymbol(current);
    if (FAILED(hr))
        return hr;

    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);

    if (hr != S_OK) {
        return hr;
    }

    for (size_t i = 0; i < symbolArray.size(); i++) {
        _bstr_t symbol = _bstr_t(symbolArray[i]);
        if (wcscmp((const wchar_t*)current, symbol) == 0) {
            CloseSnapshot();
            break;
        }
    }

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::GetSymbols(SAFEARRAY** symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }

    dxf_string_t symbol = NULL;
    int symbolCount = 1;

    if (dxf_get_snapshot_symbol(m_snapshotHandle, &symbol) == DXF_FAILURE) {
        return E_FAIL;
    }

    HRESULT hr = DispUtils::StringArrayToSafeArray((dxf_const_string_t*)&symbol, symbolCount, *symbols);

    if (hr != S_OK) {
        symbols = NULL;

        return hr;
    }

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::SetSymbols(SAFEARRAY* symbols) {
    if (symbols == NULL) {
        return E_POINTER;
    }
    if (symbols->cDims != 1)
        return E_INVALIDARG;

    DispUtils::string_vector_t symbolStorage;
    DispUtils::string_array_t symbolArray;
    HRESULT hr = DispUtils::SafeArrayToStringArray(symbols, symbolStorage, symbolArray);

    if (hr != S_OK) {
        return hr;
    }

    CloseSnapshot();
    return AddSymbol(_bstr_t(symbolArray[0]));
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::ClearSymbols() {
    CloseSnapshot();
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::GetEventTypes(INT* eventTypes) {
    if (eventTypes == NULL) {
        return E_POINTER;
    }

    *eventTypes = m_eventType;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::AddCandleSymbol(IDXCandleSymbol* symbol) {
    bool hasSymbol;
    HRESULT hr = HasSymbol(hasSymbol);
    if (FAILED(hr))
        return hr;
    if (hasSymbol)
        return S_FALSE;

    hr = E_FAIL;
    try {
        NativeCandleSymbol nativeCandleSymbol(symbol);
        if (dxf_create_candle_snapshot(m_connection, *nativeCandleSymbol, m_time, &m_snapshotHandle) == DXF_SUCCESS) {
            if (dxf_attach_snapshot_listener(m_snapshotHandle, OnNewData, this) == DXF_SUCCESS) {
                hr = S_OK;
            } else {
                CloseSnapshot();
            }
        }

    } catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::RemoveCandleSymbol(IDXCandleSymbol* symbol) {
    _bstr_t current;
    HRESULT hr = GetSymbol(current);
    if (FAILED(hr))
        return hr;
    BSTR other;
    hr = symbol->ToString(&other);
    if (FAILED(hr))
        return hr;
    _bstr_t otherWrapper(other, false);
    if (wcscmp((const wchar_t*)current, other) == 0)
        CloseSnapshot();

    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXSnapshot::OnNewData(const dxf_snapshot_data_ptr_t snapshotData, void* userData) {
    if (userData == NULL) {
        return;
    }

    DXSnapshot* thisPtr = reinterpret_cast<DXSnapshot*>(userData);

    AutoCriticalSection acs(thisPtr->m_listenerGuard);

    if (thisPtr->m_listeners.empty()) {
        return;
    }

    IDXEventDataCollection* dataCollection = DefDXEventDataCollectionFactory::CreateInstance(
        snapshotData->event_type,
        snapshotData->records,
        snapshotData->records_count,
        static_cast<IDXSubscription*>(thisPtr));
    if (dataCollection == NULL) {
        return;
    }

    IUnknownWrapper dcw(dataCollection, false);

    try {
        listener_map_t::const_iterator it = thisPtr->m_listeners.begin();
        listener_map_t::const_iterator itEnd = thisPtr->m_listeners.end();
        variant_vector_t storage;
        DISPPARAMS listenerParams;
        _bstr_t symbolWrapper(snapshotData->symbol);

        thisPtr->CreateListenerParams(static_cast<IDXSubscription*>(thisPtr), snapshotData->event_type, symbolWrapper,
            dataCollection, storage, listenerParams);

        for (; it != itEnd; ++it) {
            (*it).second->Invoke(thisPtr->m_listenerMethodId, IID_NULL, 0, DISPATCH_METHOD, &listenerParams, NULL, NULL, NULL);
        }
    } catch (...) {
    }
}

void DXSnapshot::OnNewDataInc(const dxf_snapshot_data_ptr_t snapshotData, int new_snapshot, void* userData) {
    if (userData == NULL) {
        return;
    }

    DXSnapshot* thisPtr = reinterpret_cast<DXSnapshot*>(userData);

    AutoCriticalSection acs(thisPtr->m_listenerGuard);

    if (thisPtr->m_listeners.empty()) {
        return;
    }

    IDXEventDataCollection* dataCollection = DefDXEventDataCollectionFactory::CreateInstance(
        snapshotData->event_type,
        snapshotData->records,
        snapshotData->records_count,
        static_cast<IDXSubscription*>(thisPtr));
    if (dataCollection == NULL) {
        return;
    }

    IUnknownWrapper dcw(dataCollection, false);

    try {
        listener_map_t::const_iterator it = thisPtr->m_listeners.begin();
        listener_map_t::const_iterator itEnd = thisPtr->m_listeners.end();
        variant_vector_t storage;
        DISPPARAMS listenerParams;
        _bstr_t symbolWrapper(snapshotData->symbol);

        thisPtr->CreateListenerParams(static_cast<IDXSubscription*>(thisPtr), snapshotData->event_type, symbolWrapper,
            dataCollection, storage, listenerParams);

        for (; it != itEnd; ++it) {
            if (new_snapshot)
                (*it).second->Invoke(thisPtr->m_incListenerSnapMethodId, IID_NULL, 0, DISPATCH_METHOD, &listenerParams, NULL, NULL, NULL);
            else
                (*it).second->Invoke(thisPtr->m_incListenerUpMethodId, IID_NULL, 0, DISPATCH_METHOD, &listenerParams, NULL, NULL, NULL);
        }
    } catch (...) {
    }
}

/* -------------------------------------------------------------------------- */

void DXSnapshot::ClearListeners() {
    AutoCriticalSection acs(m_listenerGuard);

    listener_map_t::iterator it = m_listeners.begin();
    listener_map_t::iterator itEnd = m_listeners.end();

    for (; it != itEnd; ++it) {
        (*it).second->Release();
    }
    m_listeners.clear();
}

/* -------------------------------------------------------------------------- */

void DXSnapshot::CreateListenerParams(IDispatch* subscription, int eventType, _bstr_t& symbol,
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

dx_event_id_t DXSnapshot::GetEventId(int eventType) {
    unsigned int eventTypeValue = (unsigned int)eventType;
    if (eventType == 0)
        throw std::invalid_argument("The eventType is invalid");
    int id = 0;
    while ((eventTypeValue & 0x1) == 0) {
        id++;
        eventTypeValue >>= 1;
    }

    if (eventTypeValue > 1)
        throw std::invalid_argument("The eventType should contains only one event");

    return (dx_event_id_t)id;
}

/* -------------------------------------------------------------------------- */

HRESULT DXSnapshot::GetSymbol(_bstr_t& symbol) {
    if (m_snapshotHandle == NULL) {
        return E_POINTER;
    }
    dxf_string_t nativeSymbol;
    if (dxf_get_snapshot_symbol(m_snapshotHandle, &nativeSymbol) == DXF_FAILURE) {
        return E_FAIL;
    }
    symbol = _bstr_t(nativeSymbol);
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT DXSnapshot::HasSymbol(bool& result) {
    if (m_snapshotHandle == NULL) {
        result = false;
        return S_OK;
    }
    SAFEARRAY* symbols;
    HRESULT hr = GetSymbols(&symbols);
    if (FAILED(hr))
        return hr;
    result = symbols->cDims > 0;
    SafeArrayDestroy(symbols);
    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXSnapshot::CloseSnapshot() {
    if (m_snapshotHandle == NULL)
        return;
    dxf_detach_snapshot_listener(m_snapshotHandle, OnNewData);
    dxf_close_snapshot(m_snapshotHandle);
    m_snapshotHandle = NULL;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::Advise(IUnknown *pUnkSink, DWORD *pdwCookie) {
    if (pUnkSink == NULL || pdwCookie == NULL) {
        return E_POINTER;
    }

    *pdwCookie = 0;

    IDispatch* listener = NULL;

    if (m_incremental) {
        if (pUnkSink->QueryInterface(DIID_IDXIncrementalEventListener, reinterpret_cast<void**>(&listener)) != S_OK) {
            return CONNECT_E_CANNOTCONNECT;
        }
        if (m_incListenerSnapMethodId == 0 || m_incListenerUpMethodId == 0) {
            return DISP_E_UNKNOWNNAME;
        }
    } else {
        if (pUnkSink->QueryInterface(DIID_IDXEventListener, reinterpret_cast<void**>(&listener)) != S_OK) {
            return CONNECT_E_CANNOTCONNECT;
        }
        if (m_listenerMethodId == 0) {
            return DISP_E_UNKNOWNNAME;
        }
    }

    AutoCriticalSection acs(m_listenerGuard);

    listener_map_t::const_iterator it = m_listeners.begin();
    listener_map_t::const_iterator itEnd = m_listeners.end();
    for (; it != itEnd; ++it) {
        if (it->second == listener) {
            // this listener is already added

            return S_OK;
        }
    }

    *pdwCookie = m_listener_next_id++;
    m_listeners[*pdwCookie] = listener;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSnapshot::Unadvise(DWORD dwCookie) {
    AutoCriticalSection acs(m_listenerGuard);

    listener_map_t::iterator it = m_listeners.find(dwCookie);

    if (it == m_listeners.end()) {
        return E_POINTER;
    }

    it->second->Release();
    m_listeners.erase(it);

    return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXSnapshot::OnBeforeDelete() {
    ClearListeners();
    CloseSnapshot();
}

/* -------------------------------------------------------------------------- */
/*
*	DefDXSnapshotFactory methods implementation
*/
/* -------------------------------------------------------------------------- */

IDXSubscription* DefDXSnapshotFactory::CreateSnapshot(dxf_connection_t connection, int eventType,
    BSTR symbol, BSTR source, LONGLONG time, BOOL incremental, IUnknown* parent) {
    IDXSubscription* subscription = NULL;

    try {
        subscription = new DXSnapshot(connection, eventType, symbol, source, time, incremental, parent);

        subscription->AddRef();
    } catch (...) {
    }

    return subscription;
}

IDXSubscription* DefDXSnapshotFactory::CreateSnapshot(dxf_connection_t connection,
    IDXCandleSymbol* symbol, LONGLONG time, BOOL incremental, IUnknown* parent) {
    IDXSubscription* subscription = NULL;

    try {
        subscription = new DXSnapshot(connection, symbol, time, incremental, parent);

        subscription->AddRef();
    } catch (...) {
    }

    return subscription;
}
