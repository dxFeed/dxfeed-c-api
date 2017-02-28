// Implementation of the helper classes and functions

#include "AuxAlgo.h"
#include "TypeLibraryManager.h"

#include <ComDef.h>

/* -------------------------------------------------------------------------- */
/*
 *	DispUtils methods implementation
 */
/* -------------------------------------------------------------------------- */

namespace DispUtils {

HRESULT GetMethodId (REFIID riid, const _bstr_t& methodName, OUT DISPID& methodId) {
    TypeLibraryManager* tlm = TypeLibraryMgrStorage::GetContent ();
    
    if (tlm == NULL) {
        return E_FAIL;
    }
    
    ITypeInfo* ti = NULL;
    HRESULT res = tlm->GetTypeInfo(riid, ti);
    
    if (res != S_OK) {
        return res;
    }
    
    _ASSERT(ti != NULL);

    BSTR methodNameStr = const_cast<_bstr_t&>(methodName).GetBSTR();

    return ::DispGetIDsOfNames(ti, &methodNameStr, 1, &methodId);
}

/* -------------------------------------------------------------------------- */

HRESULT SafeArrayToStringArray (SAFEARRAY* safeArray, string_vector_t& storage,
                                OUT string_array_t& stringArray) {
    {
        VARTYPE vt;
        
        if (::SafeArrayGetVartype(safeArray, &vt) != S_OK ||
            vt != VT_BSTR ||
            ::SafeArrayGetDim(safeArray) != 1) {
            
            return E_INVALIDARG;
        }
    }
    
    BSTR* data = NULL;
    HRESULT hr = S_OK;
    
    {
        hr = ::SafeArrayAccessData(safeArray, (void**)&data);
        
        if (hr != S_OK) {
            return hr;
        }
    }
    
    try {
        int elemCount = safeArray->rgsabound->cElements;
        
        storage.reserve(elemCount);
        stringArray.reserve(elemCount);

        for (int i = 0; i < elemCount; ++i) {
            storage.push_back(std::wstring((wchar_t*)data[i]));
        }
        
        for (int i = 0; i < elemCount; ++i) {
            stringArray.push_back(storage[i].c_str());
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    } catch (...) {
        hr = E_FAIL;
    }
    
    VERIFY(::SafeArrayUnaccessData(safeArray) == S_OK);
    
    return hr;
}

/* -------------------------------------------------------------------------- */

HRESULT StringArrayToSafeArray (dxf_const_string_t* stringArray, int stringCount,
                                OUT SAFEARRAY*& safeArray) {
    if (stringArray == NULL ||
        (safeArray = ::SafeArrayCreateVector(VT_BSTR, 0, stringCount)) == NULL) {
        return E_FAIL;
    }
    
    BSTR* data = NULL;
    HRESULT hr = ::SafeArrayAccessData(safeArray, (void**)&data);
    
    if (hr != S_OK) {
        ::SafeArrayDestroy(safeArray);
        
        return hr;
    }
        
    try {
        for (int i = 0; i < stringCount; ++i) {
            data[i] = _bstr_t(stringArray[i]).Detach();
        }
        
        ::SafeArrayUnaccessData(safeArray);
        
        return S_OK;
    } catch (const _com_error& e) {
        hr = e.Error();
    } catch (...) {
        hr = E_FAIL;
    }
    
    ::SafeArrayUnaccessData(safeArray);
    ::SafeArrayDestroy(safeArray);
    
    return hr;
}

} // DispUtils

/* -------------------------------------------------------------------------- */
/*
 *	Synchronization classes
 */
/* -------------------------------------------------------------------------- */
/*
 *	CriticalSection methods implementation
 */
/* ---------------------------------- */

CriticalSection::CriticalSection () {
    if (!::InitializeCriticalSectionAndSpinCount(&m_cs, 0)) {
        throw "Failed to initialize a critical section";
    }
}

/* -------------------------------------------------------------------------- */

CriticalSection::~CriticalSection () {
    ::DeleteCriticalSection(&m_cs);
}

/* -------------------------------------------------------------------------- */

void CriticalSection::Enter () {
    ::EnterCriticalSection(&m_cs);
}

/* -------------------------------------------------------------------------- */

void CriticalSection::Leave () {
    ::LeaveCriticalSection(&m_cs);
}

/* -------------------------------------------------------------------------- */
/*
 *	AutoCriticalSection methods implementation
 */
/* -------------------------------------------------------------------------- */

AutoCriticalSection::AutoCriticalSection (CriticalSection& cs)
: m_cs(cs) {
    m_cs.Enter();
}

/* -------------------------------------------------------------------------- */

AutoCriticalSection::~AutoCriticalSection () {
    m_cs.Leave();
}

/* -------------------------------------------------------------------------- */
/*
 *	Interface wrappers implementation
 */
/* -------------------------------------------------------------------------- */

IUnknownWrapper::IUnknownWrapper (IUnknown* p, bool addRef)
: m_p(p) {
    if (m_p != NULL && addRef) {
        m_p->AddRef();
    }
}

/* -------------------------------------------------------------------------- */

IUnknownWrapper::~IUnknownWrapper () {
    if (m_p != NULL) {
        m_p->Release();
    }
}