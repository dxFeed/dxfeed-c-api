// Definition of the DXFeedCOM C++ interfaces

#pragma once

#include <ObjBase.h>

typedef enum {
    DXEF_tx_pending = 0x01,
    DXEF_remove_event = 0x02,
    DXEF_snapshot_begin = 0x04,
    DXEF_snapshot_end = 0x08,
    DXEF_snapshot_snip = 0x10,
    DXEF_remove_symbol = 0x20
} DXFEventFlags;

/* -------------------------------------------------------------------------- */
/*
 *	IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct IDXFeed : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CreateConnection(BSTR address, IDispatch** connection) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastError(INT* code) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastErrorDescr(BSTR* descr) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitLogger(BSTR file, VARIANT_BOOL overwrite,
                                                  VARIANT_BOOL showTimezone, VARIANT_BOOL verbose) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCandleSymbol(IDispatch** candleSymbol) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

struct IDXCandleSymbol;

struct IDXConnection : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CreateSubscription(INT eventTypes, IDispatch** subscription) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastEvent(INT eventType, BSTR symbol, IDispatch** eventData) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSubscriptionTimed(INT eventTypes, LONGLONG time, IDispatch** subscription) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSnapshot(INT eventType, BSTR symbol, BSTR source, LONGLONG time, BOOL incremental, IDispatch** snapshot) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCandleSnapshot(IDXCandleSymbol* symbol, LONGLONG time, BOOL incremental, IDispatch** snapshot) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXSubscrption interface
 */
/* -------------------------------------------------------------------------- */

struct IDXSubscription : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE AddSymbol(BSTR symbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddSymbols(SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbol(BSTR symbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveSymbols(SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSymbols(SAFEARRAY** symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSymbols(SAFEARRAY* symbols) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearSymbols() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventTypes(INT* eventTypes) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddCandleSymbol(IDXCandleSymbol* symbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveCandleSymbol(IDXCandleSymbol* symbol) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXEventDataCollection interface
 */
/* -------------------------------------------------------------------------- */

struct IDXEventDataCollection : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetEventCount(ULONGLONG* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEvent(ULONGLONG index, IDispatch** eventData) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXTrade interface

 *  defines the trade data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXTrade : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTick(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChange(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayVolume(LONGLONG* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXQuote interface

 *  defines the quote data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXQuote : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetBidTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskSize(LONGLONG* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXSummary interface

 *  defines the summary data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXSummary : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetDayId(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayOpenPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayHighPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayLowPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayClosePrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayId(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOpenInterest(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFlags(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchange(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayClosePriceType(CHAR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePriceType(CHAR* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXProfile interface

 *  defines the profile data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXProfile : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetBeta(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEps(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDivFreq(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExdDivAmount(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExdDiveDate(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get52HighPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE Get52LowPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShares(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDescription(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFlags(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatusReason(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHaltStartTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHaltEndTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHighLimitPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLowLimitPrice(DOUBLE* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXOrder interface

 *  defines the order data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXOrder : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSide(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLevel(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMarketMaker(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrderSource(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCount(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTimeSequence(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetScope(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXTimeAndSale interface

 *  defines the time'n'sale data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXTimeAndSale : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetEventId(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeSaleCondition (BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAgressorSide(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpreadLeg(VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTradeFlag(VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetValidTick(VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetType(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXCandle interface

*  defines the candle data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXCandle : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCount(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOpen(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHigh(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLow(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetClose(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolume(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVwap(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidVolume(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskVolume(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOpenInterest(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetImpVolacility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXTradeETH interface

 *  defines the TradeETH data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXTradeETH : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayVolume(LONGLONG* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXSpreadOrder interface

 *  defines the spread order data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXSpreadOrder : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSide(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLevel(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode(SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOrderSource(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCount(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpreadSymbol(BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTimeSequence(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetScope(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXGreeks interface

*  defines the greeks data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXGreeks : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGreeksPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDelta(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetGamma(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheta(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRho(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVega(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXTheoPrice interface

*  defines the theo price data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXTheoPrice : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTheoTime(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoUnderlyingPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoDelta(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoGamma(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoDividend(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTheoInterest(DOUBLE* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXUnderlying interface

*  defines the underlying data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXUnderlying : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrontVolatility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBackVolatility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPutCallRatio(DOUBLE* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXSeries interface

*  defines the series data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXSeries : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetExpiration(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolatility(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPutCallRatio(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetForwardPrice(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDividend(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInterest(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsRemoved(VARIANT_BOOL* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXConfiguration interface

*  defines the series data accessor
*/
/* -------------------------------------------------------------------------- */

struct IDXConfiguration : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetStringObject(BSTR* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXConnectionTerminationNotifier sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXConnectionTerminationNotifier : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnConnectionTerminated(IDispatch* connection) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXEventListener sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXEventListener : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnNewData(IDispatch* subscription, INT eventType, BSTR symbol,
                                                 IDispatch* dataCollection) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXIncrementalEventListener sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXIncrementalEventListener : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnNewSnapshot(IDispatch* subscription, INT eventType, BSTR symbol,
                                                 IDispatch* dataCollection) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnUpdate(IDispatch* subscription, INT eventType, BSTR symbol,
                                                 IDispatch* dataCollection) = 0;
};

/* -------------------------------------------------------------------------- */
/*
*	IDXCandleSymbol sink interface
*/
/* -------------------------------------------------------------------------- */

struct IDXCandleSymbol : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE get_BaseSymbol(BSTR* baseSymbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_BaseSymbol(BSTR baseSymbol) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ExchangeCode(WCHAR* exchangeCode) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ExchangeCode(WCHAR exchangeCode) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Price(INT* price) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Price(INT price) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Session(INT* session) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Session(INT session) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PeriodType(INT* periodType) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_PeriodType(INT periodType) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PeriodValue(DOUBLE* periodValue) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_PeriodValue(DOUBLE periodValue) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Alignment(INT* alignment) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Alignment(INT alignment) = 0;
    virtual HRESULT STDMETHODCALLTYPE ToString(BSTR* value) = 0;
};