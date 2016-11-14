// Implementation of the DXFeed class

#include "Feed.h"
#include "Guids.h"
#include "LibraryLocker.h"
#include "TypeLibraryManager.h"
#include "DispatchImpl.h"
#include "Connection.h"
#include "Interfaces.h"
#include "CandleSymbol.h"

#include <comdef.h>

#include "DXFeed.h"

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed class
 
 *  the default implementation of IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct DXFeed : public IDXFeed, private DefIDispatchImpl {
    friend struct DefDXFeedFactory; 
    
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
                                              
    virtual HRESULT STDMETHODCALLTYPE CreateConnection (BSTR address, IDispatch** connection);
    virtual HRESULT STDMETHODCALLTYPE GetLastError (INT* code);
    virtual HRESULT STDMETHODCALLTYPE GetLastErrorDescr (BSTR* descr);
    virtual HRESULT STDMETHODCALLTYPE InitLogger (BSTR file, VARIANT_BOOL overwrite,
                                                  VARIANT_BOOL showTimezone, VARIANT_BOOL verbose);
    virtual HRESULT STDMETHODCALLTYPE CreateCandleSymbol(IDispatch** candleSymbol);
                                              
private:

    DXFeed ();
};

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed methods implementation
 */
/* -------------------------------------------------------------------------- */

DXFeed::DXFeed ()
: DefIDispatchImpl(IID_IDXFeed) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeed::CreateConnection (BSTR address, IDispatch** connection) {
    if (connection == NULL) {
        return E_POINTER;
    }
    
    *connection = NULL;
    
    HRESULT hr = E_FAIL;
    _bstr_t addrWrapper;
    
    try {
        addrWrapper = _bstr_t(address, false);
        
        if ((*connection = DefDXConnectionFactory::CreateInstance((const char*)addrWrapper)) != NULL) {
            hr = NOERROR;
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    } catch (...) {    
    }
    
    addrWrapper.Detach();
    
    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeed::GetLastError (INT* code) {
    if (code == NULL) {
        return E_POINTER;
    }
    
    if (dxf_get_last_error(code, NULL) == DXF_FAILURE) {
        return E_FAIL;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeed::GetLastErrorDescr (BSTR* descr) {
    if (descr == NULL) {
        return E_POINTER;
    }
    
    int dummy;
    dxf_const_string_t descrBuffer = NULL;
    
    if (dxf_get_last_error(&dummy, &descrBuffer) == DXF_FAILURE) {
        return E_FAIL;
    }
    
    try {
        _bstr_t descrWrapper(descrBuffer);
        
        *descr = descrWrapper.Detach();
    } catch (const _com_error& e) {
        return e.Error();
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeed::InitLogger (BSTR file, VARIANT_BOOL overwrite,
                                              VARIANT_BOOL showTimezone, VARIANT_BOOL verbose) {
    _bstr_t fileWrapper;
    HRESULT hr = S_OK;
    
    try {
        fileWrapper = _bstr_t(file, false);
        
        if (dxf_initialize_logger((const char*)fileWrapper, overwrite, showTimezone, verbose) == DXF_FAILURE) {
            hr = E_FAIL;
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    }
    
    fileWrapper.Detach();
    
    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeed::CreateCandleSymbol(IDispatch** candleSymbol) {
    if (candleSymbol == NULL) {
        return E_POINTER;
    }

    HRESULT hr = S_OK;

    try {

        *candleSymbol = DefDXCandleSymbolFactory::CreateInstance();
    }
    catch (const _com_error& e) {
        hr = e.Error();
    }
    catch (...) {
    }

    return hr;
}

/* -------------------------------------------------------------------------- */
/*
 *	DefDXFeedFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDXFeed* DefDXFeedFactory::CreateInstance () {
    IDXFeed* res = NULL;
    
    try {
        res = new DXFeed;
        
        res->AddRef();
    } catch (...) {        
    }
    
    return res;    
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeedStorage stuff
 */
/* -------------------------------------------------------------------------- */

IDXFeed* DXFeedStorage::m_feed = NULL;