// Definition of the sink classes

#pragma once

#include "Interfaces.h"

/* -------------------------------------------------------------------------- */
/*
 *	GenericSinkImpl class
 */
/* -------------------------------------------------------------------------- */

struct GenericSinkImpl {
protected:

	GenericSinkImpl (REFGUID riid);

	HRESULT QueryInterfaceImpl (IDispatch* objPtr, REFIID riid, void **ppvObject);
	LONG AddRefImpl ();
	LONG ReleaseImpl ();

	HRESULT GetTypeInfoCountImpl (UINT *pctinfo);
	HRESULT GetTypeInfoImpl (UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo);
	HRESULT GetIDsOfNamesImpl (REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId);
	HRESULT InvokeImpl (IDispatch* objPtr,
	DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
	DISPPARAMS *pDispParams, VARIANT *pVarResult,
	EXCEPINFO *pExcepInfo, UINT *puArgErr);
private:

	REFGUID m_riid;
	ITypeInfo* m_typeInfo;
	HRESULT m_typeInfoRes;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXConnectionTerminationNotifier class

 *  IDXConnectionTerminationNotifier sink interface implementation
 */
/* -------------------------------------------------------------------------- */

class DXConnectionTerminationNotifier : public IDXConnectionTerminationNotifier, private GenericSinkImpl {
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject) {
	return QueryInterfaceImpl(this, riid, ppvObject);
	}
	virtual ULONG STDMETHODCALLTYPE AddRef () { return AddRefImpl(); }
	virtual ULONG STDMETHODCALLTYPE Release () { return ReleaseImpl(); }

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

	virtual HRESULT STDMETHODCALLTYPE OnConnectionTerminated (IDispatch* connection);

public:

	DXConnectionTerminationNotifier ();
};

/* -------------------------------------------------------------------------- */
/*
 *	DXEventListener class

 *  IDXEventListener sink interface implementation
 */
/* -------------------------------------------------------------------------- */

class DXEventListener : public IDXEventListener, private GenericSinkImpl {
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject) {
	return QueryInterfaceImpl(this, riid, ppvObject);
	}
	virtual ULONG STDMETHODCALLTYPE AddRef () { return AddRefImpl(); }
	virtual ULONG STDMETHODCALLTYPE Release () { return ReleaseImpl(); }

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

	virtual HRESULT STDMETHODCALLTYPE OnNewData (IDispatch* subscription, INT eventType, BSTR symbol,
	IDispatch* dataCollection);

public:

	DXEventListener ();
};

/* -------------------------------------------------------------------------- */
/*
 *	DXIncrementalEventListener class

 *  IDXIncrementalEventListener sink interface implementation
 */
/* -------------------------------------------------------------------------- */

class DXIncrementalEventListener : public IDXIncrementalEventListener, private GenericSinkImpl {
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject) {
	return QueryInterfaceImpl(this, riid, ppvObject);
	}
	virtual ULONG STDMETHODCALLTYPE AddRef () { return AddRefImpl(); }
	virtual ULONG STDMETHODCALLTYPE Release () { return ReleaseImpl(); }

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

	virtual HRESULT STDMETHODCALLTYPE OnNewSnapshot (IDispatch* subscription, INT eventType, BSTR symbol,
	IDispatch* dataCollection);
	virtual HRESULT STDMETHODCALLTYPE OnUpdate (IDispatch* subscription, INT eventType, BSTR symbol,
	IDispatch* dataCollection);

public:

	DXIncrementalEventListener ();
};

/* -------------------------------------------------------------------------- */
/*
 *	Helper stuff
 */
/* -------------------------------------------------------------------------- */

struct ComReleaser {
	ComReleaser (IUnknown* obj)
	: m_obj(obj) {}
	~ComReleaser () {
	if (m_obj != NULL) {
	m_obj->Release();
	}
	}

private:

	IUnknown* m_obj;
};