// Definition of the various helper classes and functions

#pragma once

#include "DXFeed.h"

#include <vector>
#include <string>

#include <Windows.h>
#include <comutil.h>
#include <crtdbg.h>

#ifndef VERIFY
    #ifdef _DEBUG
        #define VERIFY(x) _ASSERT(x)
    #else
        #define VERIFY(x) (x)
    #endif // _DEBUG
#endif // VERIFY    

/* -------------------------------------------------------------------------- */
/*
 *	DispUtils namespace

 *  a collection of various utility functions for IDispatch objects
 */
/* -------------------------------------------------------------------------- */

namespace DispUtils {
    typedef std::vector<std::wstring> string_vector_t;
    typedef std::vector<dxf_const_string_t> string_array_t;
    
    HRESULT GetMethodId (IDispatch* obj, const _bstr_t& methodName, OUT DISPID& methodId);
    HRESULT SafeArrayToStringArray (SAFEARRAY* safeArray, string_vector_t& storage,
                                    OUT string_array_t& stringArray);
    HRESULT StringArrayToSafeArray (dxf_const_string_t* stringArray, int stringCount,
                                    OUT SAFEARRAY*& safeArray);
} // DispUtils

/* -------------------------------------------------------------------------- */
/*
 *	Synchronization classes
 */
/* -------------------------------------------------------------------------- */

struct CriticalSection {
    CriticalSection ();
    ~CriticalSection ();
    
    void Enter ();
    void Leave ();

private:

    CRITICAL_SECTION m_cs;
};

/* -------------------------------------------------------------------------- */

struct AutoCriticalSection {
    AutoCriticalSection (CriticalSection& cs);
    ~AutoCriticalSection ();
    
private:

    CriticalSection& m_cs;
};

/* -------------------------------------------------------------------------- */
/*
 *	Interface wrappers
 */
/* -------------------------------------------------------------------------- */

struct IUnknownWrapper {
    IUnknownWrapper (IUnknown* p, bool addRef = true);
    ~IUnknownWrapper ();

private:

    IUnknown* m_p;
};