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

struct DefIDispatchImpl {
protected:

    DefIDispatchImpl (REFGUID riid, IUnknown* parent = NULL);
    
    void SetBehaviorCustomizer (IDispBehaviorCustomizer* customizer);    
    
    HRESULT QueryInterfaceImpl (IDispatch* objPtr, REFIID riid, void **ppvObject);
    LONG AddRefImpl ();
    LONG ReleaseImpl ();

    HRESULT GetTypeInfoCountImpl (UINT *pctinfo);
    HRESULT GetTypeInfoImpl (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo);
    HRESULT GetIDsOfNamesImpl (REFIID riid, LPOLESTR *rgszNames,
                               UINT cNames, LCID lcid, DISPID *rgDispId);
    HRESULT InvokeImpl (IDispatch* objPtr,
                        DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
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