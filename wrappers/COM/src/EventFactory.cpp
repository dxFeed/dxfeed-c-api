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
	virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetTick(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetChange(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayVolume(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetTimeNanos(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetRawFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayTurnover(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDirection(DXFDirection* value);
	virtual HRESULT STDMETHODCALLTYPE IsETH(VARIANT_BOOL *value);
	virtual HRESULT STDMETHODCALLTYPE GetScope(DXFOrderScope* value);

private:

	DXTrade(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_trade_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTrade methods implementation
 */
/* -------------------------------------------------------------------------- */

DXTrade::DXTrade(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXTrade, parent)
, m_data(reinterpret_cast<dxf_trade_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetExchangeCode(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetSize(LONGLONG* value) {
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

HRESULT STDMETHODCALLTYPE DXTrade::GetDayVolume(LONGLONG* value) {
	CHECK_PTR(value);

	*value = (LONGLONG)std::round(m_data->day_volume);

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetSequence(INT* value) {
	CHECK_PTR(value);

	*value = m_data->sequence;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetTimeNanos(INT* value) {
	CHECK_PTR(value);

	*value = m_data->time_nanos;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetRawFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->raw_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetDayTurnover(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->day_turnover;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::GetDirection(DXFDirection* value) {
	CHECK_PTR(value);

	*value = (DXFDirection)m_data->direction;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTrade::IsETH(VARIANT_BOOL *value) {
	CHECK_PTR(value);

	*value = m_data->is_eth ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DXTrade::GetScope(DXFOrderScope *value) {
	CHECK_PTR(value);

	*value = (DXFOrderScope)m_data->scope;

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

	virtual HRESULT STDMETHODCALLTYPE GetBidTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetBidExchangeCode(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetBidPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetBidSize(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskExchangeCode(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskSize(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetTimeNanos(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetScope(DXFOrderScope* value);

private:

	DXQuote(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_quote_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXQuote methods implementation
 */
/* -------------------------------------------------------------------------- */

DXQuote::DXQuote(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXQuote, parent)
, m_data(reinterpret_cast<dxf_quote_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->bid_time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidExchangeCode(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->bid_exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->bid_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetBidSize(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->bid_size;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->ask_time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetSequence(INT* value) {
	CHECK_PTR(value);

	*value = m_data->sequence;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetTimeNanos(INT* value) {
	CHECK_PTR(value);

	*value = m_data->time_nanos;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskExchangeCode(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->ask_exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->ask_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXQuote::GetAskSize(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->ask_size;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DXQuote::GetScope(DXFOrderScope *value) {
	CHECK_PTR(value);

	*value = (DXFOrderScope)m_data->scope;

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

	virtual HRESULT STDMETHODCALLTYPE GetDayId(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayOpenPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayHighPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayLowPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayClosePrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrevDayId(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrevDayVolume(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetOpenInterest(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetRawFlags(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetExchange(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetDayClosePriceType(DXFPriceType* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePriceType(DXFPriceType* value);
	virtual HRESULT STDMETHODCALLTYPE GetScope(DXFOrderScope* value);

private:

	DXSummary(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_summary_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXSummary methods implementation
 */
/* -------------------------------------------------------------------------- */

DXSummary::DXSummary(dxf_event_data_t data, IUnknown* parent)
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

HRESULT STDMETHODCALLTYPE DXSummary::GetDayHighPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->day_high_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayLowPrice(DOUBLE* value) {
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

HRESULT STDMETHODCALLTYPE DXSummary::GetPrevDayClosePrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->prev_day_close_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetPrevDayVolume(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->prev_day_volume;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetOpenInterest(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->open_interest;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetRawFlags(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->raw_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetExchange(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetDayClosePriceType(DXFPriceType* value) {
	CHECK_PTR(value);

	*value = (DXFPriceType)m_data->day_close_price_type;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSummary::GetPrevDayClosePriceType(DXFPriceType* value) {
	CHECK_PTR(value);

	*value = (DXFPriceType)m_data->prev_day_close_price_type;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DXSummary::GetScope(DXFOrderScope *value) {
	CHECK_PTR(value);

	*value = (DXFOrderScope)m_data->scope;

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

	virtual HRESULT STDMETHODCALLTYPE GetBeta(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetEps(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDivFreq(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetExdDivAmount(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetExdDiveDate(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE Get52HighPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE Get52LowPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetShares(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDescription(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetRawFlags(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetStatusReason(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetHaltStartTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetHaltEndTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetHighLimitPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetLowLimitPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetFreeFloat(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTradingStatus(DXFTradingStatus* value);
	virtual HRESULT STDMETHODCALLTYPE GetShortSaleRestiction(DXFShortSaleRestriction* value);

private:

	DXProfile(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_profile_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXProfile methods implementation
 */
/* -------------------------------------------------------------------------- */

DXProfile::DXProfile(dxf_event_data_t data, IUnknown* parent)
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

HRESULT STDMETHODCALLTYPE DXProfile::GetDescription(BSTR* value) {
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

HRESULT DXProfile::GetRawFlags(LONGLONG * value)
{
	CHECK_PTR(value);

	*value = m_data->raw_flags;

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

HRESULT DXProfile::GetFreeFloat(DOUBLE* value)
{
	CHECK_PTR(value);

	*value = m_data->free_float;

	return S_OK;
}

HRESULT DXProfile::GetTradingStatus(DXFTradingStatus* value)
{
	CHECK_PTR(value);

	*value = (DXFTradingStatus)m_data->trading_status;

	return S_OK;
}

HRESULT DXProfile::GetShortSaleRestiction(DXFShortSaleRestriction* value)
{
	CHECK_PTR(value);

	*value = (DXFShortSaleRestriction)m_data->ssr;

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

	virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetSide(DXFSide* value);
	virtual HRESULT STDMETHODCALLTYPE GetScope(DXFOrderScope* value);
	virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetMarketMaker(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetOrderSource(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetCount(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value);
	virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE GetTimeNanos(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetSpreadSymbol(BSTR* value);

private:

	DXOrder(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_order_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXOrder methods implementation
 */
/* -------------------------------------------------------------------------- */

DXOrder::DXOrder(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXOrder, parent)
, m_data(reinterpret_cast<dxf_order_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetIndex(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->index;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSide(DXFSide* value) {
	CHECK_PTR(value);

	*value = (DXFSide)m_data->side;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetScope(DXFOrderScope* value) {
	CHECK_PTR(value);

	*value = (DXFOrderScope)m_data->scope;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetExchangeCode(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetMarketMaker(BSTR* value) {
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

HRESULT STDMETHODCALLTYPE DXOrder::GetPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSize(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->size;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetOrderSource(BSTR* value) {
	CHECK_PTR(value);

	HRESULT hr = S_OK;

	try {
	_bstr_t wrapper(m_data->source);

	*value = wrapper.Detach();
	}
	catch (const _com_error& e) {
	hr = e.Error();
	}

	return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetCount(INT* value) {
	CHECK_PTR(value);

	*value = m_data->count;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetEventFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->event_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSequence(INT* value) {
	CHECK_PTR(value);

	*value = m_data->sequence;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::IsRemoved(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = (m_data->event_flags & DXEF_remove_event) != 0 || (m_data->size == 0) ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetTimeNanos(INT* value) {
	CHECK_PTR(value);

	*value = m_data->time_nanos;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXOrder::GetSpreadSymbol(BSTR* value) {
	CHECK_PTR(value);

	HRESULT hr = S_OK;

	try {
		_bstr_t wrapper(m_data->spread_symbol);

		*value = wrapper.Detach();
	}
	catch (const _com_error& e) {
		hr = e.Error();
	}

	return hr;
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

	virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value);
	virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetBidPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetExchangeSaleCondition(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetAgressorSide(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetSpreadLeg(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE GetETHTradeFlag(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE GetValidTick(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE GetType(DXFTNSType* value);
	virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE IsTradeThroughExempt(VARIANT_BOOL* value);
	virtual HRESULT STDMETHODCALLTYPE GetBuyer(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetSeller(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetRawFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetScope(DXFOrderScope* value);

private:

	DXTimeAndSale(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_time_and_sale_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTimeAndSale methods implementation
 */
/* -------------------------------------------------------------------------- */

DXTimeAndSale::DXTimeAndSale(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXTimeAndSale, parent)
, m_data(reinterpret_cast<dxf_time_and_sale_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetIndex(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->index;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetExchangeCode(SHORT* value) {
	CHECK_PTR(value);

	*value = m_data->exchange_code;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetSize(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->size;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetBidPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->bid_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetAskPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->ask_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetExchangeSaleCondition(BSTR* value) {
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

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetEventFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->event_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetAgressorSide(INT* value) {
	CHECK_PTR(value);

	*value = m_data->side;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetSpreadLeg(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = m_data->is_spread_leg ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetETHTradeFlag(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = m_data->is_eth_trade ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetValidTick(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = m_data->is_valid_tick ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetType(DXFTNSType* value) {
	CHECK_PTR(value);

	*value = (DXFTNSType)m_data->type;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::IsRemoved(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = (m_data->event_flags & DXEF_remove_event) != 0 ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::IsTradeThroughExempt(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = m_data->trade_through_exempt ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}
/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetBuyer(BSTR* value) {
	CHECK_PTR(value);

	HRESULT hr = S_OK;

	try {
		_bstr_t wrapper(m_data->buyer);

		*value = wrapper.Detach();
	}
	catch (const _com_error& e) {
		hr = e.Error();
	}

	return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetSeller(BSTR* value) {
	CHECK_PTR(value);

	HRESULT hr = S_OK;

	try {
		_bstr_t wrapper(m_data->seller);

		*value = wrapper.Detach();
	}
	catch (const _com_error& e) {
		hr = e.Error();
	}

	return hr;
}


/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetRawFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->raw_flags;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DXTimeAndSale::GetScope(DXFOrderScope *value) {
	CHECK_PTR(value);

	*value = (DXFOrderScope)m_data->scope;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXCandle class

 *  default implementation of the IDXCandle interface
 */
/* -------------------------------------------------------------------------- */

class DXCandle : private IDXCandle, private DefIDispatchImpl {
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
	virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value);
	virtual HRESULT STDMETHODCALLTYPE GetCount(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetOpen(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetHigh(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetLow(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetClose(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetVolume(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetVwap(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetBidVolume(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetAskVolume(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetOpenInterest(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetImpVolacility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value);

private:

	DXCandle(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_candle_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTCandle methods implementation
 */
/* -------------------------------------------------------------------------- */

DXCandle::DXCandle(dxf_event_data_t data, IUnknown* parent)
	: DefIDispatchImpl(IID_IDXCandle, parent)
	, m_data(reinterpret_cast<dxf_candle_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetSequence(INT* value) {
	CHECK_PTR(value);

	*value = m_data->sequence;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetCount(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->count;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetOpen(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->open;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetHigh(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->high;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetLow(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->low;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetClose(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->close;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetVolume(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->volume;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetVwap(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->vwap;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetBidVolume(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->bid_volume;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetAskVolume(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->ask_volume;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetIndex(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->index;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetOpenInterest(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->open_interest;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetImpVolacility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->imp_volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::GetEventFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->event_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandle::IsRemoved(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = (m_data->event_flags & DXEF_remove_event) != 0 ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXGreeks class

 *  default implementation of the IDXGreeks interface
 */
/* -------------------------------------------------------------------------- */

class DXGreeks : private IDXGreeks, private DefIDispatchImpl {
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
	virtual HRESULT STDMETHODCALLTYPE GetGreeksPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDelta(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetGamma(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheta(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetRho(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetVega(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value);

private:

	DXGreeks(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_greeks_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXGreeks methods implementation
 */
/* -------------------------------------------------------------------------- */

DXGreeks::DXGreeks(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXGreeks, parent)
, m_data(reinterpret_cast<dxf_greeks_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetGreeksPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetVolatility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetDelta(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->delta;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetGamma(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->gamma;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetTheta(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->theta;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetRho(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->rho;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetVega(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->vega;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetIndex(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->index;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::GetEventFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->event_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXGreeks::IsRemoved(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = (m_data->event_flags & DXEF_remove_event) != 0 ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXTheoPrice class

 *  default implementation of the IDXTheoPrice interface
 */
/* -------------------------------------------------------------------------- */

class DXTheoPrice : private IDXTheoPrice, private DefIDispatchImpl {
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

	virtual HRESULT STDMETHODCALLTYPE GetTheoTime(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoUnderlyingPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoDelta(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoGamma(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoDividend(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetTheoInterest(DOUBLE* value);

private:

	DXTheoPrice(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_theo_price_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXTheoPrice methods implementation
 */
/* -------------------------------------------------------------------------- */

DXTheoPrice::DXTheoPrice(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXTheoPrice, parent)
, m_data(reinterpret_cast<dxf_theo_price_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoTime(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->time;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoUnderlyingPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->underlying_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoDelta(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->delta;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoGamma(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->gamma;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoDividend(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->dividend;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXTheoPrice::GetTheoInterest(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->interest;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXUnderlying class

 *  default implementation of the IDXUnderlying interface
 */
/* -------------------------------------------------------------------------- */

class DXUnderlying : private IDXUnderlying, private DefIDispatchImpl {
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

	virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetFrontVolatility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetBackVolatility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetPutCallRatio(DOUBLE* value);

private:

	DXUnderlying(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_underlying_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXUnderlying methods implementation
 */
/* -------------------------------------------------------------------------- */

DXUnderlying::DXUnderlying(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXUnderlying, parent)
, m_data(reinterpret_cast<dxf_underlying_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXUnderlying::GetVolatility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXUnderlying::GetFrontVolatility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->front_volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXUnderlying::GetBackVolatility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->back_volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXUnderlying::GetPutCallRatio(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->put_call_ratio;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXSeries class

 *  default implementation of the IDXSeries interface
 */
/* -------------------------------------------------------------------------- */

class DXSeries : private IDXSeries, private DefIDispatchImpl {
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

	virtual HRESULT STDMETHODCALLTYPE GetExpiration(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetPutCallRatio(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetForwardPrice(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetDividend(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetInterest(DOUBLE* value);
	virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value);
	virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value);
	virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value);

private:

	DXSeries(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_series_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXSeries methods implementation
 */
/* -------------------------------------------------------------------------- */

DXSeries::DXSeries(dxf_event_data_t data, IUnknown* parent)
: DefIDispatchImpl(IID_IDXSeries, parent)
, m_data(reinterpret_cast<dxf_series_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetExpiration(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->expiration;

	return S_OK;
}

/* -------------------------------------------------------------------------- */


HRESULT STDMETHODCALLTYPE DXSeries::GetVolatility(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->volatility;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetPutCallRatio(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->put_call_ratio;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetForwardPrice(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->forward_price;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetDividend(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->dividend;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetInterest(DOUBLE* value) {
	CHECK_PTR(value);

	*value = m_data->interest;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetIndex(LONGLONG* value) {
	CHECK_PTR(value);

	*value = m_data->index;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::GetEventFlags(INT* value) {
	CHECK_PTR(value);

	*value = m_data->event_flags;

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXSeries::IsRemoved(VARIANT_BOOL* value) {
	CHECK_PTR(value);

	*value = (m_data->event_flags & DXEF_remove_event) != 0 ? VARIANT_TRUE : VARIANT_FALSE;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXConfiguration class

 *  default implementation of the IDXConfiguration interface
 */
/* -------------------------------------------------------------------------- */

class DXConfiguration : private IDXConfiguration, private DefIDispatchImpl {
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

	virtual HRESULT STDMETHODCALLTYPE GetStringObject(BSTR* value);
	virtual HRESULT STDMETHODCALLTYPE GetVersion(INT* value);

private:

	DXConfiguration(dxf_event_data_t data, IUnknown* parent);

private:

	dxf_configuration_t* m_data;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXConfiguration methods implementation
 */
/* -------------------------------------------------------------------------- */

DXConfiguration::DXConfiguration(dxf_event_data_t data, IUnknown* parent)
	: DefIDispatchImpl(IID_IDXConfiguration, parent)
	, m_data(reinterpret_cast<dxf_configuration_t*>(data)) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConfiguration::GetStringObject(BSTR* value) {
	CHECK_PTR(value);

	HRESULT hr = S_OK;

	try {
	_bstr_t wrapper(m_data->object);

	*value = wrapper.Detach();
	}
	catch (const _com_error& e) {
	hr = e.Error();
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE DXConfiguration::GetVersion(INT* value) {
	CHECK_PTR(value);

	*value = m_data->version;

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	EventDataFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDispatch* EventDataFactory::CreateInstance(int eventType, dxf_event_data_t eventData, IUnknown* parent) {
	IDispatch* instance = NULL;
	try {
	switch (eventType) {
	case DXF_ET_TRADE:
	instance = static_cast<IDXTrade*>(new DXTrade(eventData, parent));
	break;
	case DXF_ET_QUOTE:
	instance = static_cast<IDXQuote*>(new DXQuote(eventData, parent));
	break;
	case DXF_ET_SUMMARY:
	instance = static_cast<IDXSummary*>(new DXSummary(eventData, parent));
	break;
	case DXF_ET_PROFILE:
	instance = static_cast<IDXProfile*>(new DXProfile(eventData, parent));
	break;
	case DXF_ET_ORDER:
	instance = static_cast<IDXOrder*>(new DXOrder(eventData, parent));
	break;
	case DXF_ET_TIME_AND_SALE:
	instance = static_cast<IDXTimeAndSale*>(new DXTimeAndSale(eventData, parent));
	break;
	case DXF_ET_CANDLE:
	instance = static_cast<IDXCandle*>(new DXCandle(eventData, parent));
	break;
	case DXF_ET_TRADE_ETH:
	instance = static_cast<IDXTrade*>(new DXTrade(eventData, parent));
	break;
	case DXF_ET_SPREAD_ORDER:
	instance = static_cast<IDXOrder*>(new DXOrder(eventData, parent));
	break;
	case DXF_ET_GREEKS:
	instance = static_cast<IDXGreeks*>(new DXGreeks(eventData, parent));
	break;
	case DXF_ET_THEO_PRICE:
	instance = static_cast<IDXTheoPrice*>(new DXTheoPrice(eventData, parent));
	break;
	case DXF_ET_UNDERLYING:
	instance = static_cast<IDXUnderlying*>(new DXUnderlying(eventData, parent));
	break;
	case DXF_ET_SERIES:
	instance = static_cast<IDXSeries*>(new DXSeries(eventData, parent));
	break;
	case DXF_ET_CONFIGURATION:
	instance = static_cast<IDXConfiguration*>(new DXConfiguration(eventData, parent));
	break;
	default:
	return NULL;
	}
	} catch (...) {
	return NULL;
	}
	instance->AddRef();
	return instance;
}