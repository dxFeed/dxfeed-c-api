// Implementation of the event data accessor interfaces and the event factory

#include "EventFactory.h"
#include "DispatchImpl.h"
#include "Guids.h"
#include "Interfaces.h"

#include <ComDef.h>

/* -------------------------------------------------------------------------- */
/*
 *	Common stuff
 */
/* -------------------------------------------------------------------------- */

#define CHECK_PTR(ptr) \
    do { \
        if (ptr == NULL) { \
            return E_POINTER; \
        } \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	DXTrade class
 
 *  default implementation of the IDXTrade interface
 */
/* -------------------------------------------------------------------------- */

class DXTrade : private IDXTrade, private DefIDispatchImpl {
    friend struct EventDataFactory;
    
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
    
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetTick(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetChange(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayVolume (LONGLONG* value);
    
private:

    DXTrade (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_trade_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTrade methods implementation
 */
/* -------------------------------------------------------------------------- */

DXTrade::DXTrade (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXTrade, parent)
, m_data(reinterpret_cast<dxf_trade_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetTime (LONGLONG* value) {
    CHECK_PTR(value);
    
    *value = m_data->time;
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetExchangeCode (SHORT* value) {
    CHECK_PTR(value);
    
    *value = m_data->exchange_code;
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetSize (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetTick(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->tick;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetChange(DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->change;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetDayVolume (LONGLONG* value) {
    CHECK_PTR(value);

    *value = *((LONGLONG*)&m_data->day_volume);

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXQuote class

 *  default implementation of the IDXQuote interface
 */
/* -------------------------------------------------------------------------- */

class DXQuote : private IDXQuote, private DefIDispatchImpl {
    friend struct EventDataFactory;

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

    virtual HRESULT STDMETHODCALLTYPE GetBidTime (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetBidExchangeCode (SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetBidSize (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetAskTime (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetAskExchangeCode (SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetAskSize (LONGLONG* value);
    
private:

    DXQuote (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_quote_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXQuote methods implementation
 */
/* -------------------------------------------------------------------------- */

DXQuote::DXQuote (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXQuote, parent)
, m_data(reinterpret_cast<dxf_quote_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidTime (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->bid_time;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidExchangeCode (SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->bid_exchange_code;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->bid_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidSize (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->bid_size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskTime (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->ask_time;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskExchangeCode (SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->ask_exchange_code;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->ask_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskSize (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->ask_size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXSummary class

 *  default implementation of the IDXSummary interface
 */
/* -------------------------------------------------------------------------- */

class DXSummary : private IDXSummary, private DefIDispatchImpl {
    friend struct EventDataFactory;

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

    virtual HRESULT STDMETHODCALLTYPE GetDayId(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayOpenPrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayHighPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayLowPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayClosePrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayId(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetOpenInterest (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetFlags(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchange(SHORT* value);

private:

    DXSummary (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_summary_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXSummary methods implementation
 */
/* -------------------------------------------------------------------------- */

DXSummary::DXSummary (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXSummary, parent)
, m_data(reinterpret_cast<dxf_summary_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayId(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->day_id;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayOpenPrice(DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->day_open_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayHighPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->day_high_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayLowPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->day_low_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayClosePrice(DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->day_close_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetPrevDayId(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->prev_day_id;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetPrevDayClosePrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->prev_day_close_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetOpenInterest (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->open_interest;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetFlags(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->flags;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetExchange(SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->exchange_code;

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXProfile class

 *  default implementation of the IDXProfile interface
 */
/* -------------------------------------------------------------------------- */

class DXProfile : private IDXProfile, private DefIDispatchImpl {
    friend struct EventDataFactory;

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

    virtual HRESULT STDMETHODCALLTYPE GetBeta(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetEps(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDivFreq(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetExdDivAmount(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetExdDiveDate(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE Get52HighPrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE Get52LowPrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetShares(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetDescription (BSTR* value);
    virtual HRESULT STDMETHODCALLTYPE GetFlags(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetStatusReason(BSTR* value);
    virtual HRESULT STDMETHODCALLTYPE GetHaltStartTime(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetHaltEndTime(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetHighLimitPrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetLowLimitPrice(DOUBLE* value);

private:

    DXProfile (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_profile_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXProfile methods implementation
 */
/* -------------------------------------------------------------------------- */

DXProfile::DXProfile (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXProfile, parent)
, m_data(reinterpret_cast<dxf_profile_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT DXProfile::GetBeta(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->beta;

    return S_OK;
}

HRESULT DXProfile::GetEps(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->eps;

    return S_OK;
}

HRESULT DXProfile::GetDivFreq(LONGLONG * value)
{
    CHECK_PTR(value);

    *value = m_data->div_freq;

    return S_OK;
}

HRESULT DXProfile::GetExdDivAmount(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->exd_div_amount;

    return S_OK;
}

HRESULT DXProfile::GetExdDiveDate(LONGLONG * value)
{
    CHECK_PTR(value);

    *value = m_data->exd_div_date;

    return S_OK;
}

HRESULT DXProfile::Get52HighPrice(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->_52_high_price;

    return S_OK;
}

HRESULT DXProfile::Get52LowPrice(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->_52_low_price;

    return S_OK;
}

HRESULT DXProfile::GetShares(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->shares;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DXProfile::GetDescription (BSTR* value) {
    CHECK_PTR(value);
    
    HRESULT hr = S_OK;
    
    try {
        _bstr_t descrWrapper(m_data->description);
        
        *value = descrWrapper.Detach();
    } catch (const _com_error& e) {
        hr = e.Error();
    }
    
    return hr;
}

HRESULT DXProfile::GetFlags(LONGLONG * value)
{
    CHECK_PTR(value);

    *value = m_data->flags;

    return S_OK;
}

HRESULT DXProfile::GetStatusReason(BSTR * value)
{
    CHECK_PTR(value);

    HRESULT hr = S_OK;

    try {
        _bstr_t descrWrapper(m_data->status_reason);

        *value = descrWrapper.Detach();
    }
    catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

HRESULT DXProfile::GetHaltStartTime(LONGLONG * value)
{
    CHECK_PTR(value);

    *value = m_data->halt_start_time;

    return S_OK;
}

HRESULT DXProfile::GetHaltEndTime(LONGLONG * value)
{
    CHECK_PTR(value);

    *value = m_data->halt_end_time;

    return S_OK;
}

HRESULT DXProfile::GetHighLimitPrice(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->high_limit_price;

    return S_OK;
}

HRESULT DXProfile::GetLowLimitPrice(DOUBLE * value)
{
    CHECK_PTR(value);

    *value = m_data->low_limit_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
*	DXOrder class

*  default implementation of the IDXOrder interface
*/
/* -------------------------------------------------------------------------- */

class DXOrder : private IDXOrder, private DefIDispatchImpl {
    friend struct EventDataFactory;

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

    virtual HRESULT STDMETHODCALLTYPE GetIndex (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetSide (INT* value);
    virtual HRESULT STDMETHODCALLTYPE GetLevel (INT* value);
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetMarketMaker (BSTR* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value);

private:

    DXOrder (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_order_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXOrder methods implementation
 */
/* -------------------------------------------------------------------------- */

DXOrder::DXOrder (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXOrder, parent)
, m_data(reinterpret_cast<dxf_order_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetIndex (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->index;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSide (INT* value) {
    CHECK_PTR(value);

    *value = m_data->side;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetLevel (INT* value) {
    CHECK_PTR(value);

    *value = m_data->level;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetTime (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->time;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetExchangeCode (SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->exchange_code;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetMarketMaker (BSTR* value) {
    CHECK_PTR(value);

    HRESULT hr = S_OK;

    try {
        _bstr_t wrapper(m_data->market_maker);

        *value = wrapper.Detach();
    } catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSize (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXTimeAndSale class

 *  default implementation of the IDXTimeAndSale interface
 */
/* -------------------------------------------------------------------------- */

class DXTimeAndSale : private IDXTimeAndSale, private DefIDispatchImpl {
    friend struct EventDataFactory;

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

    virtual HRESULT STDMETHODCALLTYPE GetEventId (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice (DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchangeSaleCondition (BSTR* value);
    virtual HRESULT STDMETHODCALLTYPE GetTradeFlag (VARIANT_BOOL* value);
    virtual HRESULT STDMETHODCALLTYPE GetType (INT* value);

private:

    DXTimeAndSale (dxf_event_data_t data, IUnknown* parent);

private:

    dxf_time_and_sale_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTimeAndSale methods implementation
 */
/* -------------------------------------------------------------------------- */

DXTimeAndSale::DXTimeAndSale (dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXTimeAndSale, parent)
, m_data(reinterpret_cast<dxf_time_and_sale_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetEventId (LONGLONG* value) {
    CHECK_PTR(value);
    
    *value = m_data->event_id;
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetTime (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->time;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetExchangeCode (SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->exchange_code;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetSize (LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetBidPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->bid_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetAskPrice (DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->ask_price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetExchangeSaleCondition (BSTR* value) {
    CHECK_PTR(value);

    HRESULT hr = S_OK;

    try {
        _bstr_t wrapper(m_data->exchange_sale_conditions);

        *value = wrapper.Detach();
    } catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetTradeFlag (VARIANT_BOOL* value) {
    CHECK_PTR(value);

    *value = m_data->is_trade ? VARIANT_TRUE : VARIANT_FALSE;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetType (INT* value) {
    CHECK_PTR(value);

    *value = m_data->type;

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
*	DXTradeETH class

*  default implementation of the IDXTradeETH interface
*/
/* -------------------------------------------------------------------------- */

class DXTradeETH : private IDXTradeETH, private DefIDispatchImpl {
    friend struct EventDataFactory;

private:

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
        return QueryInterfaceImpl(this, riid, ppvObject);
    }
    virtual ULONG STDMETHODCALLTYPE AddRef() { return AddRefImpl(); }
    virtual ULONG STDMETHODCALLTYPE Release() { ULONG res = ReleaseImpl(); if (res == 0) delete this; return res; }

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo) { return GetTypeInfoCountImpl(pctinfo); }
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

    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetFlags(INT* value);
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value);
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value);
    virtual HRESULT STDMETHODCALLTYPE GetDayVolume(LONGLONG* value);

private:

    DXTradeETH(dxf_event_data_t data, IUnknown* parent);

private:

    dxf_trade_eth_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
*	DXTrade methods implementation
*/
/* -------------------------------------------------------------------------- */

DXTradeETH::DXTradeETH(dxf_event_data_t data, IUnknown* parent)
    : DefIDispatchImpl(IID_IDXTradeETH, parent)
    , m_data(reinterpret_cast<dxf_trade_eth_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetTime(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->time;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetFlags(INT* value) {
    CHECK_PTR(value);

    *value = m_data->flags;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetExchangeCode(SHORT* value) {
    CHECK_PTR(value);

    *value = m_data->exchange;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetPrice(DOUBLE* value) {
    CHECK_PTR(value);

    *value = m_data->price;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetSize(LONGLONG* value) {
    CHECK_PTR(value);

    *value = m_data->size;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTradeETH::GetDayVolume(LONGLONG* value) {
    CHECK_PTR(value);

    *value = *((LONGLONG*)&m_data->eth_volume);

    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	EventDataFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDispatch* EventDataFactory::CreateInstance (int eventType, dxf_event_data_t eventData, IUnknown* parent) {
    try {
        switch (eventType) {
        case DXF_ET_TRADE: return static_cast<IDXTrade*>(new DXTrade(eventData, parent));
        case DXF_ET_QUOTE: return static_cast<IDXQuote*>(new DXQuote(eventData, parent));
        case DXF_ET_SUMMARY: return static_cast<IDXSummary*>(new DXSummary(eventData, parent));
        case DXF_ET_PROFILE: return static_cast<IDXProfile*>(new DXProfile(eventData, parent));
        case DXF_ET_ORDER: return static_cast<IDXOrder*>(new DXOrder(eventData, parent));
        case DXF_ET_TIME_AND_SALE: return static_cast<IDXTimeAndSale*>(new DXTimeAndSale(eventData, parent));
        case DXF_ET_TRADE_ETH: return static_cast<IDXTradeETH*>(new DXTradeETH(eventData, parent));
        default: return NULL;
        }
    } catch (...) {
        return NULL;
    }
}