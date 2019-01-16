// Implementation of the IDXConnection interface and the related stuff

#include "Connection.h"
#include "DispatchImpl.h"
#include "ConnectionPointImpl.h"
#include "Subscription.h"
#include "EventFactory.h"
#include "AuxAlgo.h"
#include "Guids.h"
#include "Interfaces.h"
#include "Snapshot.h"

#include "DXFeed.h"

#include <ComDef.h>
#include <crtdbg.h>
#include <atlsafe.h>

#include <string>

#define LOW_DWORD_FROM_PTR(ptr) static_cast<DWORD>((DWORD_PTR)ptr & 0xFFFFFFFF)

/* -------------------------------------------------------------------------- */
/*
 *	DXConnection class

 *  the default implementation of IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

class DXConnection : private IDXConnection, private DefIDispatchImpl,
	private DefIConnectionPointImpl, private IDispBehaviorCustomizer {
	friend struct DefDXConnectionFactory;

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject);
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

	virtual HRESULT STDMETHODCALLTYPE CreateSubscription (INT eventTypes, IDispatch** subscription);
	virtual HRESULT STDMETHODCALLTYPE GetLastEvent (INT eventType, BSTR symbol, IDispatch** eventData);
	virtual HRESULT STDMETHODCALLTYPE CreateSubscriptionTimed(INT eventTypes, LONGLONG time, IDispatch** subscription);
	virtual HRESULT STDMETHODCALLTYPE CreateSnapshot(INT eventType, BSTR symbol, BSTR source, LONGLONG time, BOOL incremental, IDispatch** snapshot);
	virtual HRESULT STDMETHODCALLTYPE CreateCandleSnapshot(IDXCandleSymbol* symbol, LONGLONG time, BOOL incremental, IDispatch** snapshot);
	virtual HRESULT STDMETHODCALLTYPE GetProperties(SAFEARRAY** ppKeys, SAFEARRAY** ppValues);
	virtual HRESULT STDMETHODCALLTYPE GetConnectedAddress(BSTR* pAddress);

private:

	enum class AuthType {
	none = 0,
	basic = 1,
	bearer = 2,
	custom = 3
	};

	DXConnection(const char* address);
	DXConnection(const char* address, AuthType type, const char* param1, const char* param2);

	static void OnConnectionTerminated (dxf_connection_t connection, void* user_data);
	static int OnSocketThreadCreated (dxf_connection_t connection, void* user_data);
	static void OnSocketThreadDestroyed (dxf_connection_t connection, void* user_data);
	void ClearNotifier ();

private:

	// IConnectionPoint interface implementation

	virtual HRESULT STDMETHODCALLTYPE Advise (IUnknown *pUnkSink, DWORD *pdwCookie);
	virtual HRESULT STDMETHODCALLTYPE Unadvise (DWORD dwCookie);

private:

	// IDispBehaviorCustomizer interface implementation

	virtual void OnBeforeDelete ();

private:

	dxf_connection_t m_connHandle;

	CriticalSection m_notifierGuard;
	IDispatch* m_termNotifier;
	DISPID m_notifierMethodId;
	VARIANTARG m_thisWrapper;
	DISPPARAMS m_notifierMethodParams;
};

/* -------------------------------------------------------------------------- */
/*
 *	DXConnection methods implementation
 */
/* -------------------------------------------------------------------------- */

DXConnection::DXConnection(const char* address) : DXConnection(address, AuthType::none, NULL, NULL) {}

DXConnection::DXConnection (const char* address, AuthType type, const char* param1, const char* param2)
: DefIDispatchImpl(IID_IDXConnection)
, DefIConnectionPointImpl(DIID_IDXConnectionTerminationNotifier)
, m_connHandle(NULL)
, m_termNotifier(NULL)
, m_notifierMethodId(0) {
	ERRORCODE result;
	switch (type) {
	case AuthType::none:
	result = dxf_create_connection(address, OnConnectionTerminated, NULL, OnSocketThreadCreated,
	OnSocketThreadDestroyed, reinterpret_cast<void*>(this), &m_connHandle);
	break;
	case AuthType::basic:
	result = dxf_create_connection_auth_basic(address, param1, param2, OnConnectionTerminated, NULL,
	OnSocketThreadCreated, OnSocketThreadDestroyed, reinterpret_cast<void*>(this), &m_connHandle);
	break;
	case AuthType::bearer:
	result = dxf_create_connection_auth_bearer(address, param1, OnConnectionTerminated, NULL,
	OnSocketThreadCreated, OnSocketThreadDestroyed, reinterpret_cast<void*>(this), &m_connHandle);
	break;
	case AuthType::custom:
	result = dxf_create_connection_auth_custom(address, param1, param2, OnConnectionTerminated, NULL,
	OnSocketThreadCreated, OnSocketThreadDestroyed, reinterpret_cast<void*>(this), &m_connHandle);
	break;
	}
	if (result == DXF_FAILURE) {
	throw "Failed to establish connection";
	}

	SetBehaviorCustomizer(this);

	if (DispUtils::GetMethodId(DIID_IDXConnectionTerminationNotifier, _bstr_t("OnConnectionTerminated"),
	OUT m_notifierMethodId) != S_OK) {
	m_notifierMethodId = 0;
	}
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::QueryInterface (REFIID riid, void **ppvObject) {
	if (ppvObject == NULL) {
	return E_POINTER;
	}

	*ppvObject = NULL;

	if (IsEqualIID(riid, IID_IConnectionPointContainer)) {
	*ppvObject = static_cast<IConnectionPointContainer*>(this);

	AddRef();

	return NOERROR;
	}

	return QueryInterfaceImpl(this, riid, ppvObject);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::CreateSubscription (INT eventTypes, IDispatch** subscription) {
	if (subscription == NULL) {
	return E_POINTER;
	}

	IUnknown* parent = static_cast<IDXConnection*>(this);

	if ((*subscription = DefDXSubscriptionFactory::CreateInstance(m_connHandle, eventTypes, parent)) == NULL) {
	return E_FAIL;
	}

	return NOERROR;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::GetLastEvent (INT eventType, BSTR symbol, IDispatch** eventData) {
	if (eventData == NULL) {
	return E_POINTER;
	}

	*eventData = NULL;

	_bstr_t symbolWrapper;
	HRESULT hr = E_FAIL;

	try {
	dxf_event_data_t eventDataHandle = NULL;

	symbolWrapper = _bstr_t(symbol, false);

	if (dxf_get_last_event(m_connHandle, eventType, (const wchar_t*)symbolWrapper, &eventDataHandle) == DXF_FAILURE) {
	throw "Failed to retrieve the last event data";
	}

	IUnknown* parent = static_cast<IDXConnection*>(this);

	if ((*eventData = EventDataFactory::CreateInstance(eventType, eventDataHandle, parent)) != NULL) {
	hr = NOERROR;
	}
	} catch (const _com_error& e) {
	hr = e.Error();
	} catch (...) {
	}

	symbolWrapper.Detach();

	return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::CreateSubscriptionTimed(INT eventTypes, LONGLONG time, IDispatch** subscription) {
	if (subscription == NULL) {
	return E_POINTER;
	}

	IUnknown* parent = static_cast<IDXConnection*>(this);

	if ((*subscription = DefDXSubscriptionFactory::CreateInstance(m_connHandle, eventTypes, time, parent)) == NULL) {
	return E_FAIL;
	}

	return NOERROR;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::CreateSnapshot(INT eventType, BSTR symbol, BSTR source, LONGLONG time, BOOL incremental, IDispatch** snapshot) {
	if (snapshot == NULL) {
	return E_POINTER;
	}

	IUnknown* parent = static_cast<IDXConnection*>(this);

	if ((*snapshot = DefDXSnapshotFactory::CreateSnapshot(m_connHandle, eventType, symbol, source, time, incremental, parent)) == NULL) {
	return E_FAIL;
	}

	return NOERROR;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::CreateCandleSnapshot(IDXCandleSymbol* symbol, LONGLONG time, BOOL incremental, IDispatch** snapshot) {
	if (snapshot == NULL) {
	return E_POINTER;
	}

	IUnknown* parent = static_cast<IDXConnection*>(this);

	if ((*snapshot = DefDXSnapshotFactory::CreateSnapshot(m_connHandle, symbol, time, incremental, parent)) == NULL) {
	return E_FAIL;
	}

	return NOERROR;
}

HRESULT STDMETHODCALLTYPE DXConnection::GetProperties(SAFEARRAY** ppKeys, SAFEARRAY** ppValues) {
	if (ppKeys == NULL || ppValues == NULL) {
		return E_POINTER;
	}

	*ppKeys = NULL;
	*ppValues = NULL;

	class PropertiesHolder {
	private:
		dxf_property_item_t* m_pProperties;
		int m_count;

	public:
		PropertiesHolder(dxf_connection_t connection) {
			if (dxf_get_connection_properties_snapshot(connection, &m_pProperties, &m_count) == DXF_FAILURE) {
				throw CAtlException();
			}
		}

		std::pair<LPSAFEARRAY, LPSAFEARRAY> toSafeArrays() const {
			CComSafeArray<BSTR> keys;
			CComSafeArray<BSTR> values;
			for (int i = 0; i < m_count; ++i) {
				HRESULT hr = keys.Add(CComBSTR(m_pProperties[i].key).Detach(), FALSE);
				if (FAILED(hr)) {
					throw CAtlException(hr);
				}
				hr = values.Add(CComBSTR(m_pProperties[i].value).Detach(), FALSE);
				if (FAILED(hr)) {
					throw CAtlException(hr);
				}
			}
			return std::pair<LPSAFEARRAY, LPSAFEARRAY>(keys.Detach(), values.Detach());
		}

		virtual ~PropertiesHolder() {
			dxf_free_connection_properties_snapshot(m_pProperties, m_count);
		}
	} holder(m_connHandle);

	try {
		const std::pair<LPSAFEARRAY, LPSAFEARRAY> safe_arrays = holder.toSafeArrays();
		*ppKeys = safe_arrays.first;
		*ppValues = safe_arrays.second;
		return S_OK;
	}
	catch (const CAtlException& ex) {
		return ex;
	}
	catch (...) {
		return E_FAIL;
	}
}

HRESULT STDMETHODCALLTYPE DXConnection::GetConnectedAddress(BSTR* pAddress) {
	if (pAddress == NULL) {
		return E_POINTER;
	}
	*pAddress = NULL;
	char* pAddressBuf = NULL;
	HRESULT hr = S_OK;
	try {
		if (dxf_get_current_connected_address(m_connHandle, &pAddressBuf) == DXF_FAILURE) {
			return E_FAIL;
		}
		*pAddress = CComBSTR(pAddressBuf).Detach();
	}
	catch (const CAtlException& ex) {
		hr = ex;
	}
	catch (...) {
		hr = E_FAIL;
	}
	dxf_free(pAddressBuf);
	return hr;
}

/* -------------------------------------------------------------------------- */

void DXConnection::OnConnectionTerminated (dxf_connection_t connection, void* user_data) {
	if (user_data == NULL) {
	return;
	}

	DXConnection* thisPtr = reinterpret_cast<DXConnection*>(user_data);

	_ASSERT(thisPtr->m_connHandle == connection);

	AutoCriticalSection acs(thisPtr->m_notifierGuard);

	if (thisPtr->m_termNotifier == NULL) {
	return;
	}

	thisPtr->m_termNotifier->Invoke(thisPtr->m_notifierMethodId, IID_NULL, 0, DISPATCH_METHOD,
	&(thisPtr->m_notifierMethodParams), NULL, NULL, NULL);
}

/* -------------------------------------------------------------------------- */

int DXConnection::OnSocketThreadCreated (dxf_connection_t connection, void* user_data) {
	HRESULT res = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (res == S_OK || res == S_FALSE) {
	return 1;
	} else {
	return 0;
	}
}

/* -------------------------------------------------------------------------- */

void DXConnection::OnSocketThreadDestroyed (dxf_connection_t connection, void* user_data) {
	::CoUninitialize();
}

/* -------------------------------------------------------------------------- */

void DXConnection::ClearNotifier () {
	AutoCriticalSection acs(m_notifierGuard);

	if (m_termNotifier != NULL) {
	m_termNotifier->Release();
	m_termNotifier = NULL;
	}
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::Advise (IUnknown *pUnkSink, DWORD *pdwCookie) {
	if (pUnkSink == NULL || pdwCookie == NULL) {
	return E_POINTER;
	}

	*pdwCookie = 0;

	AutoCriticalSection acs(m_notifierGuard);

	if (m_termNotifier != NULL) {
	return CONNECT_E_ADVISELIMIT;
	}

	if (pUnkSink->QueryInterface(DIID_IDXConnectionTerminationNotifier, reinterpret_cast<void**>(&m_termNotifier)) != S_OK) {
	return CONNECT_E_CANNOTCONNECT;
	}

	if (m_notifierMethodId == 0) {
	ClearNotifier();

	return DISP_E_UNKNOWNNAME;
	}

	VariantInit(&m_thisWrapper);

	m_thisWrapper.vt = VT_DISPATCH;
	m_thisWrapper.pdispVal = this;

	m_notifierMethodParams.rgvarg = &m_thisWrapper;
	m_notifierMethodParams.rgdispidNamedArgs = NULL;
	m_notifierMethodParams.cArgs = 1;
	m_notifierMethodParams.cNamedArgs = 0;

	//Note: use low 4 bytes of address as cookie for crossplatform build.
	//      Don't use this cookie to converting into real pointer.
	*pdwCookie = LOW_DWORD_FROM_PTR(m_termNotifier);

	return S_OK;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXConnection::Unadvise (DWORD dwCookie) {
	AutoCriticalSection acs(m_notifierGuard);

	if (m_termNotifier == NULL) {
	return CONNECT_E_NOCONNECTION;
	}

	//Note: use low 4 bytes of address as cookie for crossplatform build.
	//      Don't use this cookie to converting into real pointer.
	if (dwCookie != LOW_DWORD_FROM_PTR(m_termNotifier)) {
	return E_POINTER;
	}

	ClearNotifier();

	return S_OK;
}

/* -------------------------------------------------------------------------- */

void DXConnection::OnBeforeDelete () {
	if (m_connHandle != NULL) {
	dxf_close_connection(m_connHandle);

	m_connHandle = NULL;
	}

	ClearNotifier();
}

/* -------------------------------------------------------------------------- */
/*
 *	DefDXConnectionFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

IDXConnection* DefDXConnectionFactory::CreateInstance (const char* address) {
	IDXConnection* connection = NULL;

	try {
	connection = new DXConnection(address);

	connection->AddRef();
	} catch (...) {
	}

	return connection;
}

/* -------------------------------------------------------------------------- */

IDXConnection* DefDXConnectionFactory::CreateAuthBasicInstance(const char* address, const char* user, const char* password) {
	IDXConnection* connection = NULL;

	try {
	connection = new DXConnection(address, DXConnection::AuthType::basic, user, password);

	connection->AddRef();
	} catch (...) {
	}

	return connection;
}

/* -------------------------------------------------------------------------- */

IDXConnection* DefDXConnectionFactory::CreateAuthBearerInstance(const char* address, const char* token) {
	IDXConnection* connection = NULL;

	try {
	connection = new DXConnection(address, DXConnection::AuthType::bearer, token, NULL);

	connection->AddRef();
	} catch (...) {
	}

	return connection;
}

/* -------------------------------------------------------------------------- */

IDXConnection* DefDXConnectionFactory::CreateAuthCustomInstance(const char* address, const char* authsheme, const char* authdata) {
	IDXConnection* connection = NULL;

	try {
	connection = new DXConnection(address, DXConnection::AuthType::custom, authsheme, authdata);

	connection->AddRef();
	} catch (...) {
	}

	return connection;
}