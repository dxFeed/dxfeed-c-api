// Implementation of the sink classes

#include "Sinks.h"
#include "TypeLibraryManager.h"

#include "Guids.h"
#include "Interfaces.h"
#include "EventData.h"

#include <iostream>

#include <comutil.h>

using namespace std;

/* -------------------------------------------------------------------------- */
/*
 *	GenericSinkImpl methods implementation
 */
/* -------------------------------------------------------------------------- */

GenericSinkImpl::GenericSinkImpl (REFGUID riid)
: m_riid(riid)
, m_typeInfo(NULL)
, m_typeInfoRes(E_FAIL) {
    TypeLibraryManager* tlm = TypeLibraryMgrStorage::GetContent();

    if (tlm == NULL) {
        throw "No type library";
    }

    m_typeInfoRes = tlm->GetTypeInfo(riid, m_typeInfo);
}

/* -------------------------------------------------------------------------- */

HRESULT GenericSinkImpl::QueryInterfaceImpl (IDispatch* objPtr, REFIID riid, void **ppvObject) {
    if (ppvObject == NULL) {
        return E_POINTER;
    }

    *ppvObject = NULL;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDispatch) || IsEqualIID(riid, m_riid)) {
        *ppvObject = objPtr;

        AddRefImpl();

        return NOERROR;
    }

    return E_NOINTERFACE;
}

/* -------------------------------------------------------------------------- */

LONG GenericSinkImpl::AddRefImpl () {
    return 1;
}

/* -------------------------------------------------------------------------- */

LONG GenericSinkImpl::ReleaseImpl () {
    return 1;
}

/* -------------------------------------------------------------------------- */

HRESULT GenericSinkImpl::GetTypeInfoCountImpl (UINT *pctinfo) {
    if (pctinfo == NULL) {
        return E_POINTER;
    }

    *pctinfo = 0;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT GenericSinkImpl::GetTypeInfoImpl (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
    if (ppTInfo == NULL) {
        return E_POINTER;
    }

    *ppTInfo = NULL;

    return E_NOTIMPL;
}

/* -------------------------------------------------------------------------- */

HRESULT GenericSinkImpl::GetIDsOfNamesImpl (REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId) {
    return E_NOTIMPL;
}

/* -------------------------------------------------------------------------- */

HRESULT GenericSinkImpl::InvokeImpl (IDispatch* objPtr,
                                     DISPID dispIdMember, REFIID riid, LCID lcid,
                                     WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                     EXCEPINFO *pExcepInfo, UINT *puArgErr) {
    if (!IsEqualIID(riid, IID_NULL)) {
        return DISP_E_UNKNOWNINTERFACE;
    }

    if (m_typeInfoRes != S_OK) {
        return m_typeInfoRes;
    }

    return ::DispInvoke(objPtr, m_typeInfo, dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

/* -------------------------------------------------------------------------- */
/*
 *	DXConnectionTerminationNotifier methods implementation
 */
/* -------------------------------------------------------------------------- */

DXConnectionTerminationNotifier::DXConnectionTerminationNotifier ()
: GenericSinkImpl(DIID_IDXConnectionTerminationNotifier) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnectionTerminationNotifier::OnConnectionTerminated (IDispatch* connection) {
    cout << "Connection terminated!!!!\n";
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXEventListener methods implementation
 */
/* -------------------------------------------------------------------------- */

DXEventListener::DXEventListener ()
: GenericSinkImpl(DIID_IDXEventListener) {
}

/* -------------------------------------------------------------------------- */

struct Alerter {
    Alerter ()
    : m_trigger(true) {}
    void disarm () { m_trigger = false; }
    ~Alerter () {
        if (m_trigger) {
            cout << "DATA PROCESSING ERROR!!!\n";
        }
    }
    
private:
 
    bool m_trigger; 
};

HRESULT STDMETHODCALLTYPE DXEventListener::OnNewData (IDispatch* subscription, INT eventType, BSTR symbol,
                                                      IDispatch* dataCollection) {
    Alerter a;
    
    if (subscription == NULL || dataCollection == NULL) {
        return E_POINTER;
    }
    
    _bstr_t symWrp(symbol, false);
    cout << "\t\tData received: symbol is " << std::string((const char*)symWrp).c_str();
    symWrp.Detach();
    
    if (eventType == DXF_ET_TRADE) {
        cout << ", event type is Trade";
    } else if (eventType == DXF_ET_CANDLE) {
        cout << ", event type is Candle";
    }
    
    IDXEventDataCollection* dc = reinterpret_cast<IDXEventDataCollection*>(dataCollection);
    
    INT dataCount;
    HRESULT res = dc->GetEventCount(&dataCount);
    
    if (res != S_OK) {
        return res;
    }
    
    cout << ", event data count = " << dataCount << "\n";
    
    for (int i = 0; i < dataCount; ++i) {
        IDispatch* eventData = NULL;
        
        if ((res = dc->GetEvent(i, &eventData)) != S_OK) {
            return res;
        }
        
        ComReleaser edr(eventData);
        
        if (eventType == DXF_ET_TRADE) {
            IDXTrade* t = (IDXTrade*)eventData;
            DOUBLE price;
            
            if ((res = t->GetPrice(&price)) != S_OK) {
                return res;
            }
            
            cout << "\t\t\tevent[" << i << "].price = " << price << std::endl;
        } else if (eventType == DXF_ET_CANDLE) {
            IDXCandle* c = (IDXCandle*)eventData;
            DOUBLE open, high, low, close;

            if ((res = c->GetOpen(&open)) != S_OK ||
                (res = c->GetHigh(&high)) != S_OK ||
                (res = c->GetLow(&low)) != S_OK ||
                (res = c->GetClose(&close)) != S_OK) {
                return res;
            }

            cout << "\t\tevent[" << i << "].open = " << open 
                << "\tevent[" << i << "].high = " << high
                << std::endl
                << "\t\tevent[" << i << "].low = " << low
                << "\tevent[" << i << "].close = " << close
                << std::endl;
        }
    }
    
    a.disarm();
    
    return S_OK;
}