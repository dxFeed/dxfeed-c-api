// Definition of the DXFeedCOM C++ interfaces

#pragma once

#include <ObjBase.h>

/* -------------------------------------------------------------------------- */
/*
 *	IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct IDXFeed : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CreateConnection (BSTR address, IDispatch** connection) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastError (INT* code) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastErrorDescr (BSTR* descr) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitLogger (BSTR file, VARIANT_BOOL overwrite,
                                                  VARIANT_BOOL showTimezone, VARIANT_BOOL verbose) = 0;
};

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
 *	IDXEventDataCollection interface
 */
/* -------------------------------------------------------------------------- */

struct IDXEventDataCollection : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetEventCount (INT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEvent (INT index, IDispatch** eventData) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXTrade interface

 *  defines the trade data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXTrade : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayVolume (LONGLONG* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXQuote interface

 *  defines the quote data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXQuote : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetBidTime (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidSize (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskTime (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskSize (LONGLONG* value) = 0;
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
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXProfile interface

 *  defines the profile data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXProfile : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetDescription (BSTR* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXOrder interface

 *  defines the order data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXOrder : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetIndex (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSide (INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLevel (INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMarketMaker (BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXTimeAndSale interface

 *  defines the time'n'sale data accessor
 */
/* -------------------------------------------------------------------------- */

struct IDXTimeAndSale : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetEventId (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTime (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSequence(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeSaleCondition (BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEventFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIndex(LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAgressorSide(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpreadLeg(VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTradeFlag (VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetValidTick(VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetType (INT* value) = 0;
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
    virtual HRESULT STDMETHODCALLTYPE GetOpenInterest(DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetImpVolacility(DOUBLE* value) = 0;
    
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
    virtual HRESULT STDMETHODCALLTYPE GetFlags(INT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpreadSymbol(BSTR* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDXConnectionTerminationNotifier sink interface
 */
/* -------------------------------------------------------------------------- */

struct IDXConnectionTerminationNotifier : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnConnectionTerminated (IDispatch* connection) = 0;
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