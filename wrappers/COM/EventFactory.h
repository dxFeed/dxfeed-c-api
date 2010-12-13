// Definition of the event data interfaces and the event factory class

#pragma once

#include "DXFeed.h"

#include <ObjBase.h>

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
    virtual HRESULT STDMETHODCALLTYPE GetDayHighPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayLowPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDayOpenPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrevDayClosePrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetOpenInterest (LONGLONG* value) = 0;
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
    virtual HRESULT STDMETHODCALLTYPE GetExchangeCode (SHORT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSize (LONGLONG* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBidPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAskPrice (DOUBLE* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetExchangeSaleCondition (BSTR* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetTradeFlag (VARIANT_BOOL* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetType (INT* value) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	EventDataFactory class
 */
/* -------------------------------------------------------------------------- */

struct EventDataFactory {
    static IDispatch* CreateInstance (int eventType, dxf_event_data_t eventData, IUnknown* parent);
};