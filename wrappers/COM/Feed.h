// Definition of the class implementing the main library interface

#pragma once

#include <objbase.h>

/* -------------------------------------------------------------------------- */
/*
 *	IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct IDXFeed : public IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CreateConnection (BSTR address, IDispatch** connection) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastError (INT* code) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLastErrorDescr (BSTR* descr) = 0;
    virtual HRESULT STDMETHODCALLTYPE InitLogger (BSTR file, VARIANT_BOOL overwrite,
                                                  VARIANT_BOOL showTimezone, VARIANT_BOOL verbose) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	DefDXFeedFactory class
 
 *  creates the default implementation of IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXFeedFactory {
    static IDXFeed* CreateInstance ();
};

/* -------------------------------------------------------------------------- */
/*
 *	DXFeedStorage class
 
 *  stores a single instance of IDXFeed
 */
/* -------------------------------------------------------------------------- */

struct DXFeedStorage {
    static IDXFeed* GetContent () {
        return m_feed;
    }
    
    static void SetContent (IDXFeed* value) {
        m_feed = value;
    }
    
private:

    static IDXFeed* m_feed;
};