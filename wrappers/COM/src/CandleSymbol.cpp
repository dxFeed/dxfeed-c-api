#include <comdef.h>
#include "DispatchImpl.h"
#include "Interfaces.h"
#include "Guids.h"

#include "CandleSymbol.h"

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
*	DXCandleSymbol class

*  default implementation of the IDXCandleSymbol interface
*/
/* -------------------------------------------------------------------------- */

class DXCandleSymbol : public IDXCandleSymbol, private DefIDispatchImpl {

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

    virtual HRESULT STDMETHODCALLTYPE get_BaseSymbol(BSTR* value);
    virtual HRESULT STDMETHODCALLTYPE put_BaseSymbol(BSTR value);
    virtual HRESULT STDMETHODCALLTYPE get_ExchangeCode(CHAR* value);
    virtual HRESULT STDMETHODCALLTYPE put_ExchangeCode(CHAR value);
    virtual HRESULT STDMETHODCALLTYPE get_Price(INT* value);
    virtual HRESULT STDMETHODCALLTYPE put_Price(INT value);
    virtual HRESULT STDMETHODCALLTYPE get_Session(INT* value);
    virtual HRESULT STDMETHODCALLTYPE put_Session(INT value);
    virtual HRESULT STDMETHODCALLTYPE get_PeriodType(INT* value);
    virtual HRESULT STDMETHODCALLTYPE put_PeriodType(INT value);
    virtual HRESULT STDMETHODCALLTYPE get_PeriodValue(DOUBLE* value);
    virtual HRESULT STDMETHODCALLTYPE put_PeriodValue(DOUBLE value);
    virtual HRESULT STDMETHODCALLTYPE get_Alignment(INT* value);
    virtual HRESULT STDMETHODCALLTYPE put_Alignment(INT value);

public:

    DXCandleSymbol();

private:

    _bstr_t baseSymbol;
    CHAR exchangeCode;
    INT price;
    INT session;
    INT periodType;
    DOUBLE periodValue;
    INT alignment;
};

/* -------------------------------------------------------------------------- */

DXCandleSymbol::DXCandleSymbol()
    : DefIDispatchImpl(IID_IDXCandleSymbol)
    , baseSymbol()
    , exchangeCode(DXF_CANDLE_EXCHANGE_CODE_ATTRIBUTE_DEFAULT)
    , price(dxf_cpa_default)
    , session(dxf_csa_default)
    , periodType(dxf_cpa_default)
    , periodValue(DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT)
    , alignment(dxf_caa_default) {}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_BaseSymbol(BSTR* value) {
    CHECK_PTR(value);
    *value = baseSymbol.copy(true);
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_BaseSymbol(BSTR value) {
    HRESULT hr = S_OK;

    try {
        baseSymbol = _bstr_t(value);
    }
    catch (const _com_error& e) {
        hr = e.Error();
    }

    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_ExchangeCode(CHAR* value) {
    CHECK_PTR(value);
    *value = exchangeCode;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_ExchangeCode(CHAR value) {
    exchangeCode = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_Price(INT* value) {
    CHECK_PTR(value);
    *value = price;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_Price(INT value) {
    price = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_Session(INT* value) {
    CHECK_PTR(value);
    *value = session;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_Session(INT value) {
    session = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_PeriodType(INT* value) {
    CHECK_PTR(value);
    *value = periodType;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_PeriodType(INT value) {
    periodType = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_PeriodValue(DOUBLE* value) {
    CHECK_PTR(value);
    *value = periodValue;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_PeriodValue(DOUBLE value) {
    periodValue = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_Alignment(INT* value) {
    CHECK_PTR(value);
    *value = alignment;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_Alignment(INT value) {
    alignment = value;
    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
*	DefDXCandleSymbolFactory methods implementation
*/
/* -------------------------------------------------------------------------- */

IDXCandleSymbol* DefDXCandleSymbolFactory::CreateInstance() {
    IDXCandleSymbol* res = NULL;

    try {
        res = new DXCandleSymbol();

        res->AddRef();
    }
    catch (...) {
    }

    return res;
}
