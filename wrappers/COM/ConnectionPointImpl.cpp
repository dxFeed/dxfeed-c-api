// Implementation of DefIConnectionPointImpl class

#include "ConnectionPointImpl.h"

/* -------------------------------------------------------------------------- */
/*
 *	DefIConnectionPointImpl methods implementation
 */
/* -------------------------------------------------------------------------- */

DefIConnectionPointImpl::DefIConnectionPointImpl (REFIID riid)
: m_riid(riid) {
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIConnectionPointImpl::EnumConnectionPoints (IEnumConnectionPoints **ppEnum) {
    // No need to implement this method so far

    if (ppEnum == NULL) {
        return E_POINTER;
    }

    *ppEnum = NULL;

    return E_NOTIMPL;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIConnectionPointImpl::FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) {
    if (ppCP == NULL) {
        return E_POINTER;
    }

    *ppCP = NULL;

    if (!IsEqualIID(riid, m_riid)) {
        return E_NOINTERFACE;
    }

    *ppCP = this;

    static_cast<IConnectionPointContainer*>(this)->AddRef();

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIConnectionPointImpl::GetConnectionInterface (IID *pIID) {
    if (pIID == NULL) {
        return E_POINTER;
    }

    memcpy(pIID, &m_riid, sizeof(GUID));

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIConnectionPointImpl::GetConnectionPointContainer (IConnectionPointContainer **ppCPC) {
    if (ppCPC == NULL) {
        return E_POINTER;
    }

    *ppCPC = this;

    static_cast<IConnectionPointContainer*>(this)->AddRef();

    return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DefIConnectionPointImpl::EnumConnections (IEnumConnections **ppEnum) {
    if (ppEnum == NULL) {
        return E_POINTER;
    }

    *ppEnum = NULL;

    return E_NOTIMPL;
}
