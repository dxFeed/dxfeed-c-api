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

const char* eventTypeToString(int eventType) {
    switch (eventType) {
        case DXF_ET_TRADE: return "Trade";
        case DXF_ET_QUOTE: return "Quote";
        case DXF_ET_SUMMARY: return "Summary";
        case DXF_ET_PROFILE: return "Profile";
        case DXF_ET_ORDER: return "Order";
        case DXF_ET_TIME_AND_SALE: return "Time&Sale";
        case DXF_ET_CANDLE: return "Candle";
        case DXF_ET_TRADE_ETH: return "TradeETH";
        case DXF_ET_SPREAD_ORDER: return "SpreadOrder";
        case DXF_ET_GREEKS: return "Greeks";
        case DXF_ET_THEO_PRICE: return "TheoPrice";
        case DXF_ET_UNDERLYING: return "Underlying";
        case DXF_ET_SERIES: return "Series";
        case DXF_ET_CONFIGURATION: return "Configuration";
        default: return "";
    }
}

HRESULT STDMETHODCALLTYPE DXEventListener::OnNewData (IDispatch* subscription, INT eventType, BSTR symbol,
                                                      IDispatch* dataCollection) {
    Alerter a;
    
    if (subscription == NULL || dataCollection == NULL) {
        return E_POINTER;
    }
    
    _bstr_t symWrp(symbol, false);
    cout << "\t\tData received: symbol is " << std::string((const char*)symWrp).c_str();
    symWrp.Detach();
    
    cout << ", event type is " << eventTypeToString(eventType);
    
    IDXEventDataCollection* dc = reinterpret_cast<IDXEventDataCollection*>(dataCollection);
    
    ULONGLONG dataCount;
    HRESULT res = dc->GetEventCount(&dataCount);
    
    if (res != S_OK) {
        return res;
    }
    
    cout << ", event data count = " << dataCount << "\n";
    
    for (ULONGLONG i = 0; i < dataCount; ++i) {
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
        } 

        if (eventType == DXF_ET_QUOTE) {
            IDXQuote* q = (IDXQuote*)eventData;
        }

        if (eventType == DXF_ET_SUMMARY) {
            IDXSummary* s = (IDXSummary*)eventData;
        }

        if (eventType == DXF_ET_PROFILE) {
            IDXProfile* p = (IDXProfile*)eventData;
        }

        if (eventType == DXF_ET_ORDER) {
            IDXOrder* o = (IDXOrder*)eventData;
        }

        if (eventType == DXF_ET_TIME_AND_SALE) {
            IDXTimeAndSale* ts = (IDXTimeAndSale*)eventData;
        }
        
        if (eventType == DXF_ET_CANDLE) {
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

        if (eventType == DXF_ET_TRADE_ETH) {
            IDXTradeETH* t = (IDXTradeETH*)eventData;
        }

        if (eventType == DXF_ET_SPREAD_ORDER) {
            IDXSpreadOrder* so = (IDXSpreadOrder*)eventData;
        }

        if (eventType == DXF_ET_GREEKS) {
            IDXGreeks* g = (IDXGreeks*)eventData;
        }

        if (eventType == DXF_ET_THEO_PRICE) {
            IDXTheoPrice* tp = (IDXTheoPrice*)eventData;
        }

        if (eventType == DXF_ET_UNDERLYING) {
            IDXUnderlying* u = (IDXUnderlying*)eventData;
        }

        if (eventType == DXF_ET_SERIES) {
            IDXSeries* s = (IDXSeries*)eventData;
        }

        if (eventType == DXF_ET_CONFIGURATION) {
            IDXConfiguration* c = (IDXConfiguration*)eventData;
        }

    }
    
    a.disarm();
    
    return S_OK;
}