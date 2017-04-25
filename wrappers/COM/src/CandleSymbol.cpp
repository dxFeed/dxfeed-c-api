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
    virtual HRESULT STDMETHODCALLTYPE get_ExchangeCode(WCHAR* value);
    virtual HRESULT STDMETHODCALLTYPE put_ExchangeCode(WCHAR value);
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
    virtual HRESULT STDMETHODCALLTYPE ToString(BSTR* value);

public:

    DXCandleSymbol();

private:

    _bstr_t baseSymbol;
    WCHAR exchangeCode;
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

HRESULT STDMETHODCALLTYPE DXCandleSymbol::get_ExchangeCode(WCHAR* value) {
    CHECK_PTR(value);
    *value = exchangeCode;
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXCandleSymbol::put_ExchangeCode(WCHAR value) {
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

typedef struct {
    dxf_string_t string;
    dxf_long_t period_interval_millis;
} dx_candle_type;

static const dx_candle_type g_candle_type_period[dxf_ctpa_count] = {
    { L"t", 0LL },
    { L"s", 1000LL },
    { L"m", 60LL * 1000LL },
    { L"h", 60LL * 60LL * 1000LL },
    { L"d", 24LL * 60LL * 60LL * 1000LL },
    { L"w", 7LL * 24LL * 60LL * 60LL * 1000LL },
    { L"mo", 30LL * 24LL * 60LL * 60LL * 1000LL },
    { L"o", 30LL * 24LL * 60LL * 60LL * 1000LL },
    { L"y", 365LL * 24LL * 60LL * 60LL * 1000LL },
    { L"v", 0LL },
    { L"p", 0LL },
    { L"pm", 0LL },
    { L"pr", 0LL }
};

static const dxf_string_t g_candle_price[dxf_cpa_count] = {
    L"last",
    L"bid",
    L"ask",
    L"mark",
    L"s"
};

static const dxf_string_t g_candle_session[dxf_csa_count] = {
    L"false", /*ANY*/
    L"true"   /*REGULAR*/
};

static const dxf_string_t g_candle_alignment[dxf_caa_count] = {
    L"m", /*MIDNIGHT*/
    L"s"  /*SESSION*/
};

HRESULT STDMETHODCALLTYPE DXCandleSymbol::ToString(BSTR* value) {
    CHECK_PTR(value);
    std::wstring buf;
    bool put_comma = false;

    buf.append((const wchar_t*)baseSymbol);
    if (iswalpha(exchangeCode)) {
        buf.append(1, L'&').append(1, exchangeCode);
    }

    if (periodValue == DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT &&
        periodType == dxf_ctpa_default &&
        alignment == dxf_caa_default &&
        price == dxf_cpa_default &&
        session == dxf_csa_default) {

        *value = SysAllocStringLen(buf.c_str(), buf.size());
        if (*value == NULL)
            return E_OUTOFMEMORY;
        return S_OK;
    }

    buf.append(L"{");

    /*attribute (period) has no name and is written the first, and the rest should be sorted alphabetically.*/
    if (periodType != dxf_ctpa_default ||
        periodValue != DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT) {

        buf.append(L"=");
        if (periodValue != DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT) {
            buf.append(std::to_wstring(periodValue));
        }
        buf.append(g_candle_type_period[periodType].string);
        put_comma = true;
    }

    if (alignment != dxf_caa_default) {
        if (put_comma) {
            buf.append(L",");
        }
        buf.append(L"a=").append(g_candle_alignment[alignment]);
        put_comma = true;
    }

    if (price != dxf_cpa_default) {
        if (put_comma) {
            buf.append(L",");
        }
        buf.append(L"price=").append(g_candle_price[price]);

        put_comma = true;
    }

    if (session != dxf_csa_default) {
        if (put_comma) {
            buf.append(L",");
        }
        buf.append(L"tho=").append(g_candle_session[session]);

        put_comma = true;
    }

    buf.append(L"}");

    *value = SysAllocStringLen(buf.c_str(), buf.size());
    if (*value == NULL)
        return E_OUTOFMEMORY;
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
