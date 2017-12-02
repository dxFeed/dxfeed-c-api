// Definition of a generic IConnectionPoint/IConnectionPointContainer implementation

#pragma once

#include <ObjBase.h>
#include <OCIdl.h>

/* -------------------------------------------------------------------------- */
/*
 *	DefIConnectionPointImpl class
 */
/* -------------------------------------------------------------------------- */

struct DefIConnectionPointImpl : public IConnectionPointContainer, public IConnectionPoint {
	DefIConnectionPointImpl (REFIID riid);

	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum);
	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP);

	virtual HRESULT STDMETHODCALLTYPE GetConnectionInterface (IID *pIID);
	virtual HRESULT STDMETHODCALLTYPE GetConnectionPointContainer (IConnectionPointContainer **ppCPC);
	virtual HRESULT STDMETHODCALLTYPE EnumConnections (IEnumConnections **ppEnum);

private:

	REFIID m_riid;
};