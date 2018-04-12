// Implementation of the type library manager object

#include "TypeLibraryManager.h"
#include "Guids.h"

#include <map>
#include <functional>
#include <memory.h>

/* -------------------------------------------------------------------------- */
/*
 *	Interfaces with type info
 */
/* -------------------------------------------------------------------------- */

typedef const GUID* GUIDPTR;

static GUIDPTR const g_interfaceGuids[] = {
	&IID_IDXFeed,
	&IID_IDXConnection,
	&IID_IDXSubscription,
	&IID_IDXEventDataCollection,
	&IID_IDXTrade,
	&IID_IDXQuote,
	&IID_IDXSummary,
	&IID_IDXProfile,
	&IID_IDXOrder,
	&IID_IDXTimeAndSale,
	&IID_IDXCandle,
	&IID_IDXGreeks,
	&IID_IDXTheoPrice,
	&IID_IDXUnderlying,
	&IID_IDXSeries,
	&IID_IDXConfiguration,

	&IID_IDXCandleSymbol,
	&DIID_IDXConnectionTerminationNotifier,
	&DIID_IDXEventListener,
	&DIID_IDXIncrementalEventListener
};

static const int g_interfaceCount = sizeof(g_interfaceGuids) / sizeof(g_interfaceGuids[0]);

/* -------------------------------------------------------------------------- */
/*
 *	Type library properties
 */
/* -------------------------------------------------------------------------- */

const WORD g_typeLibMajorVersion = 1;
const WORD g_typeLibMinorVersion = 0;
const LCID g_typeLibLcid = LOCALE_NEUTRAL; /* language neutral */

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryProperties methods implementation
 */
/* -------------------------------------------------------------------------- */

REFGUID TypeLibraryProperties::clsid () { return CLSID_DXFeedTypeLib; }
WORD TypeLibraryProperties::majorVer () { return g_typeLibMajorVersion; }
WORD TypeLibraryProperties::minorVer () { return g_typeLibMinorVersion; }
LCID TypeLibraryProperties::lcid () { return g_typeLibLcid; }

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary types and data structures
 */
/* -------------------------------------------------------------------------- */

struct GuidPtrLess : public std::binary_function <GUIDPTR, GUIDPTR, bool> {
	bool operator() (const GUIDPTR& left, const GUIDPTR& right) const {
	return (memcmp(left, right, sizeof(GUID)) < 0);
	}
};

typedef std::pair<ITypeInfo*, HRESULT> TypeInfoEntry;
typedef std::map<GUIDPTR, TypeInfoEntry, GuidPtrLess> TypeInfoMap;

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryManagerImpl class

 *  a default implementation of TypeLibraryManager interface
 */
/* -------------------------------------------------------------------------- */

class TypeLibraryManagerImpl : public TypeLibraryManager {
public:

	TypeLibraryManagerImpl ();
	virtual ~TypeLibraryManagerImpl ();

	virtual HRESULT GetTypeInfo (REFGUID iid, OUT ITypeInfo*& typeInfo);

private:

	bool m_isInitialized;
	HRESULT m_typeLibResult;
	TypeInfoMap m_typeInfoMap;
};

/* -------------------------------------------------------------------------- */

TypeLibraryManagerImpl::TypeLibraryManagerImpl ()
: m_isInitialized(false)
, m_typeLibResult(S_OK) {
	ITypeLib* typeLib = NULL;

	if ((m_typeLibResult = ::LoadRegTypeLib(CLSID_DXFeedTypeLib,
	g_typeLibMajorVersion, g_typeLibMinorVersion,
	g_typeLibLcid, &typeLib)) != S_OK) {
	// Type library failed to load, storing the error code and halting the initialization
	return;
	}

	m_isInitialized = true;

	int i = 0;

	for (; i < g_interfaceCount; ++i) {
	TypeInfoEntry tie;

	tie.first = NULL;
	tie.second = typeLib->GetTypeInfoOfGuid(*g_interfaceGuids[i], &tie.first);

	m_typeInfoMap[g_interfaceGuids[i]] = tie;

	if (tie.second == S_OK) {
	tie.first->AddRef();
	}
	}

	typeLib->Release();
}

/* -------------------------------------------------------------------------- */

TypeLibraryManagerImpl::~TypeLibraryManagerImpl () {
	TypeInfoMap::iterator it = m_typeInfoMap.begin();
	TypeInfoMap::iterator itEnd = m_typeInfoMap.end();

	for (; it != itEnd; ++it) {
	if (it->second.second == S_OK) {
	it->second.first->Release();
	}
	}
}

/* -------------------------------------------------------------------------- */

HRESULT TypeLibraryManagerImpl::GetTypeInfo (REFGUID iid, OUT ITypeInfo*& typeInfo) {
	if (!m_isInitialized) {
	typeInfo = NULL;

	return m_typeLibResult;
	}

	TypeInfoMap::iterator it = m_typeInfoMap.find(&iid);

	if (it == m_typeInfoMap.end()) {
	typeInfo = NULL;

	return E_FAIL;
	}

	typeInfo = it->second.first;

	return it->second.second;
}

/* -------------------------------------------------------------------------- */
/*
 *	DefTypeLibMgrFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

TypeLibraryManager* DefTypeLibMgrFactory::CreateInstance () {
	try {
	return new TypeLibraryManagerImpl();
	}
	catch (...) {
	return NULL;
	}
}

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryMgrStorage stuff
 */
/* -------------------------------------------------------------------------- */

TypeLibraryManager* TypeLibraryMgrStorage::m_tlm = NULL;
