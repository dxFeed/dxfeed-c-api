// Definition of the default IDispatch implementation

#pragma once

#include "AuxAlgo.h"

#include <objbase.h>

/* -------------------------------------------------------------------------- */
/*
 *	DefIDispatchImpl class
 */
/* -------------------------------------------------------------------------- */

struct IDispBehaviorCustomizer;

struct DefIDispatchImpl : public IDispatch {
    DefIDispatchImpl (REFGUID riid, IUnknown* parent = NULL);
    
    void SetBehaviorCustomizer (IDispBehaviorCustomizer* customizer);    
    
    virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef ();
    virtual ULONG STDMETHODCALLTYPE Release ();

    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount (UINT *pctinfo);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames (REFIID riid, LPOLESTR *rgszNames,
                                                     UINT cNames, LCID lcid, DISPID *rgDispId);
    virtual HRESULT STDMETHODCALLTYPE Invoke (DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                                              DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                              EXCEPINFO *pExcepInfo, UINT *puArgErr);    
private:

    LONG m_refCount;
    REFGUID m_riid;
    IUnknownWrapper m_parent;
    IDispBehaviorCustomizer* m_customizer;
    ITypeInfo* m_typeInfo;
    HRESULT m_typeInfoRes;
};

/* -------------------------------------------------------------------------- */
/*
 *	IDispBehaviorCustomizer interface
 
 *  allows the DefIDispatchImpl users to customize its behavior
 */
/* -------------------------------------------------------------------------- */

struct IDispBehaviorCustomizer {
    virtual void OnBeforeDelete () = 0;
};