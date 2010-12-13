// Implementation of DefIDispatchImpl

#include "DispatchImpl.h"
#include "TypeLibraryManager.h"
#include "LibraryLocker.h"

/* -------------------------------------------------------------------------- */
/*
 *	DefIDispatchImpl methods implementation
 */
/* -------------------------------------------------------------------------- */

DefIDispatchImpl::DefIDispatchImpl (REFGUID riid, IUnknown* parent)
: m_refCount(0)
, m_riid(riid)
, m_parent(parent, true)
, m_customizer(NULL)
, m_typeInfo(NULL)
, m_typeInfoRes(E_FAIL) {
    TypeLibraryManager* tlm = TypeLibraryMgrStorage::GetContent();

    if (tlm == NULL) {
        return;
    }

    m_typeInfoRes = tlm->GetTypeInfo(m_riid, &m_typeInfo);
}

/* -------------------------------------------------------------------------- */

void DefIDispatchImpl::SetBehaviorCustomizer (IDispBehaviorCustomizer* customizer) {
    m_customizer = customizer;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIDispatchImpl::QueryInterface (REFIID riid, void **ppvObject) {
    if (ppvObject == NULL) {
        return E_POINTER;
    }
    
    *ppvObject = NULL;
    
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IDispatch) || IsEqualIID(riid, m_riid)) {
        *ppvObject = this;

        AddRef();

        return NOERROR;
    }

    return E_NOINTERFACE;
}

/* -------------------------------------------------------------------------- */

ULONG STDMETHODCALLTYPE DefIDispatchImpl::AddRef () {
    LibraryLocker::AddLock();

    return InterlockedIncrement(&m_refCount);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIDispatchImpl::GetTypeInfoCount (UINT *pctinfo) {
    *pctinfo = 1;

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIDispatchImpl::GetTypeInfo (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) {
    if (ppTInfo == NULL) {
        return E_POINTER;
    }

    *ppTInfo = NULL;

    if (iTInfo != 0) {
        return DISP_E_BADINDEX;
    }

    *ppTInfo = m_typeInfo;

    if (m_typeInfo != NULL) {
        m_typeInfo->AddRef();
    }

    return m_typeInfoRes;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIDispatchImpl::GetIDsOfNames (REFIID riid, LPOLESTR *rgszNames,
                                                           UINT cNames, LCID lcid, DISPID *rgDispId) {
    if (m_typeInfoRes != S_OK) {
        return m_typeInfoRes;
    }

    return ::DispGetIDsOfNames(m_typeInfo, rgszNames, cNames, rgDispId);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIDispatchImpl::Invoke (DISPID dispIdMember, REFIID riid, LCID lcid,
                                                    WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
                                                    EXCEPINFO *pExcepInfo, UINT *puArgErr) {
    if (!IsEqualIID(riid, IID_NULL)) {
        return DISP_E_UNKNOWNINTERFACE;
    }

    if (m_typeInfoRes != S_OK) {
        return m_typeInfoRes;
    }

    return ::DispInvoke(this, m_typeInfo, dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
}

/* -------------------------------------------------------------------------- */

LONG DefIDispatchImpl::ReleaseImpl () {
    LibraryLocker::ReleaseLock();

    LONG res = InterlockedDecrement(&m_refCount);

    if (res == 0) {
        if (m_customizer != NULL) {
            m_customizer->OnBeforeDelete();
        }
    }

    return res;
}