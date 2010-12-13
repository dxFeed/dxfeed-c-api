// Implementation of the COM object self-registration mechanism

#include "Guids.h"
#include "LibraryLocker.h"
#include "TypeLibraryManager.h"

#include <string>
#include <vector>
#include <algorithm>
#include <memory>

#include <tchar.h>
#include <comutil.h>
#include <OleCtl.h>

/* -------------------------------------------------------------------------- */
/*
 *	Common types
 */
/* -------------------------------------------------------------------------- */

typedef std::basic_string<TCHAR> tstring;

/* -------------------------------------------------------------------------- */
/*
 *	RegKeyWrapper class
 
 *  embeds the registry key processing functionality
 */
/* -------------------------------------------------------------------------- */

class RegKeyWrapper {
    
    typedef std::vector<RegKeyWrapper*> key_vector_t;
    
    struct KeyDeleter {
        void operator () (RegKeyWrapper* key) {
            delete key;
        }
    };
    
    struct RemoveFlagSetter {
        RemoveFlagSetter (bool value)
        : m_value(value) {        
        }
        
        void operator () (RegKeyWrapper* key) {
            key->setRemoveFlag(m_value);
        }
        
    private:
        
        bool m_value;
    };
    
public:
    
    RegKeyWrapper ()
    : m_key(NULL)
    , m_removeKey(true) {        
    }
    
    virtual ~RegKeyWrapper () {
        deleteSubkeys();
        
        if (m_key != NULL) {
            ::RegCloseKey(m_key);
        }
    }
    
    HKEY getKey () const {
        return m_key;
    }
    
    void setValue (LPCTSTR valueName, DWORD type, const BYTE* data, DWORD dataSize) {
        if (::RegSetValueEx(m_key, valueName, 0, type, data, dataSize) != ERROR_SUCCESS) {
            throw "Failed to set a value";
        }
    }
    
    void addSubkey (RegKeyWrapper* subkey) {
        m_subkeys.push_back(subkey);
    }
    
    void setRemoveFlag (bool value = true) {
        m_removeKey = value;
        
        std::for_each(m_subkeys.begin(), m_subkeys.end(), RemoveFlagSetter(value));
    }
    
protected:

    bool getRemoveFlag () const {
        return m_removeKey;
    }
    
    void deleteSubkeys () {
        std::for_each(m_subkeys.begin(), m_subkeys.end(), KeyDeleter());
        
        m_subkeys.clear();
    }
    
protected:

    HKEY m_key;

private:    
    
    bool m_removeKey;
    key_vector_t m_subkeys;
};

/* -------------------------------------------------------------------------- */
/*
 *	class RegKeyWrapperCreate
 
 *  implements the functionality of new registry key creation
 */
/* -------------------------------------------------------------------------- */

struct RegKeyWrapperCreate : public RegKeyWrapper {
    RegKeyWrapperCreate (HKEY parentKey, LPCTSTR subkey, DWORD options, REGSAM accessMask)
    : m_parentKey(parentKey)
    , m_subkey(subkey) {
        if (::RegCreateKeyEx(parentKey, subkey, 0, NULL, options, accessMask, NULL, &m_key, NULL) != ERROR_SUCCESS) {
            throw "Failed to create a key";
        }
    }
    
    RegKeyWrapperCreate (RegKeyWrapper* parent, LPCTSTR subkey, DWORD options, REGSAM accessMask)
    : m_parentKey(parent->getKey())
    , m_subkey(subkey) {
        if (::RegCreateKeyEx(m_parentKey, subkey, 0, NULL, options, accessMask, NULL, &m_key, NULL) != ERROR_SUCCESS) {
            throw "Failed to create a key";
        }
        
        parent->addSubkey(this);
    }
    
    ~RegKeyWrapperCreate () {
        deleteSubkeys();
        
        ::RegCloseKey(m_key);
        
        if (getRemoveFlag()) {
            ::RegDeleteKey(m_parentKey, m_subkey.c_str());
        }
    }

protected:

    HKEY m_parentKey;
    tstring m_subkey;
};

/* -------------------------------------------------------------------------- */
/*
 *	class RegKeyWrapperOpen
 
 *  implements the functionality of opening the existing key
 */
/* -------------------------------------------------------------------------- */

struct RegKeyWrapperOpen : public RegKeyWrapper {
    RegKeyWrapperOpen (HKEY parentKey, LPCTSTR subkey, REGSAM accessMask)
    : m_parentKey(parentKey)
    , m_subkey(subkey) {
        if (::RegOpenKeyEx(parentKey, subkey, 0, accessMask, &m_key) != ERROR_SUCCESS) {
            throw "Failed to open a key";
        }
    }
    
    RegKeyWrapperOpen (RegKeyWrapper* parent, LPCTSTR subkey, REGSAM accessMask)
    : m_parentKey(parent->getKey())
    , m_subkey(subkey) {
        if (::RegOpenKeyEx(m_parentKey, subkey, 0, accessMask, &m_key) != ERROR_SUCCESS) {
            throw "Failed to open a key";
        }
        
        parent->addSubkey(this);
    }
    
protected:

    HKEY m_parentKey;
    tstring m_subkey;
};

/* -------------------------------------------------------------------------- */
/*
 *	Common library data
 */
/* -------------------------------------------------------------------------- */

const TCHAR g_rootKeyPath[] = _T("Software\\Classes");
const TCHAR g_progID[] = _T("DXFeed.1.0");
const TCHAR g_clsidStr[] = _T("CLSID");
const TCHAR g_objectDescription[] = _T("DXFeed COM component");
const DWORD g_guidStringSize = 39 * sizeof(TCHAR); // 32 GUID symbols + 4 dashes + 2 braces + null terminator
const TCHAR g_typeLibFileName[] = _T("DXFeedCOM.tlb");

/* -------------------------------------------------------------------------- */
/*
 *	Utility functions
 */
/* -------------------------------------------------------------------------- */

const TCHAR* stringFromCLSID (REFIID riid) {
    static _bstr_t container;    
    LPOLESTR stringBuf = NULL;
    
    if (::StringFromCLSID(riid, &stringBuf) != S_OK) {
        return NULL;
    }
    
    container = _bstr_t(stringBuf, true);
    ::CoTaskMemFree(stringBuf);
    
    return (const TCHAR*)container;
}

/* -------------------------------------------------------------------------- */

RegKeyWrapper* getRegistryTree (bool haltOnError = true) {
    try {
        std::auto_ptr<RegKeyWrapper> rootKey(new RegKeyWrapperOpen(HKEY_LOCAL_MACHINE, g_rootKeyPath, KEY_WRITE));
        
        do {
            RegKeyWrapper* prodKey = NULL;
            
            try {
                prodKey = new RegKeyWrapperCreate(rootKey.get(), g_progID, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS);
                
                prodKey->setValue(NULL, REG_SZ, (const BYTE*)g_objectDescription, sizeof(g_objectDescription));
            } catch (...) {
                if (haltOnError) {
                    throw;
                } else {
                    break;
                }
            }

            try {
                RegKeyWrapper* clsid = new RegKeyWrapperCreate(prodKey, g_clsidStr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS);

                clsid->setValue(NULL, REG_SZ, (const BYTE*)stringFromCLSID(CLSID_DXFeed), g_guidStringSize);
            } catch (...) {
                if (haltOnError) {
                    throw;
                }
            }
        } while (false);

        do {
            RegKeyWrapper* clsidKey = NULL;
            
            try {
                clsidKey = new RegKeyWrapperOpen(rootKey.get(), g_clsidStr, KEY_ALL_ACCESS);
            } catch (...) {
                if (haltOnError) {
                    throw;
                } else {
                    break;
                }
            }

            do {
                RegKeyWrapper* objId = NULL;
                
                try {
                    objId = new RegKeyWrapperCreate(clsidKey, stringFromCLSID(CLSID_DXFeed), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS);
                    
                    objId->setValue(NULL, REG_SZ, (const BYTE*)g_objectDescription, sizeof(g_objectDescription));
                } catch (...) {
                    if (haltOnError) {
                        throw;
                    } else {
                        break;
                    }
                }

                try {
                    RegKeyWrapper* inProcSrvKey = new RegKeyWrapperCreate(clsidKey, _T("InprocServer32"), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS);
                    TCHAR libPath[MAX_PATH];
                    DWORD realPathLength = ::GetModuleFileName(LibraryLocker::GetLibraryHandle(), libPath, MAX_PATH);

                    if (realPathLength == 0) {
                        throw "Failed to retrieve the module path";
                    }

                    inProcSrvKey->setValue(NULL, REG_SZ, (const BYTE*)libPath, realPathLength * sizeof(TCHAR));
                    inProcSrvKey->setValue(_T("ThreadingModel"), REG_SZ, (const BYTE*)_T("both"), 5 * sizeof(TCHAR));
                } catch (...) {
                    if (haltOnError) {
                        throw;
                    }
                }

                try {
                    RegKeyWrapper* progIdKey = new RegKeyWrapperCreate(clsidKey, _T("ProgID"), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS);

                    progIdKey->setValue(NULL, REG_SZ, (const BYTE*)g_progID, sizeof(g_progID));
                } catch (...) {
                    if (haltOnError) {
                        throw;
                    }
                }
            } while (false);
        } while (false);
        
        return rootKey.release();
    } catch (...) {
        return NULL;
    }
}

/* -------------------------------------------------------------------------- */
/*
 *	DllRegisterServer function
 
 *  performs the creation of the registry entries for the library
 */
/* -------------------------------------------------------------------------- */

HRESULT PASCAL DllRegisterServer () {
    try {
        std::auto_ptr<RegKeyWrapper> libTree(getRegistryTree());
        
        if (libTree.get() == NULL) {
            return SELFREG_E_CLASS;
        }
        
        TCHAR libPath[MAX_PATH];
        DWORD realPathLength = ::GetModuleFileName(LibraryLocker::GetLibraryHandle(), libPath, MAX_PATH);    
        
        if (realPathLength == 0) {
            return E_UNEXPECTED;
        }
        
        TCHAR driveBuf[_MAX_DRIVE];
        TCHAR dirBuf[_MAX_DIR];
        
        _tsplitpath(libPath, driveBuf, dirBuf, NULL, NULL);
        
        tstring tlbPath;
        tlbPath = tlbPath + driveBuf + dirBuf + g_typeLibFileName;
        _bstr_t tlbPathBstr(tlbPath.c_str());
        
        ITypeLib* pTypeLib;
        
        if (::LoadTypeLib(tlbPathBstr.GetBSTR(), &pTypeLib) != S_OK ||
            ::RegisterTypeLib(pTypeLib, tlbPathBstr.GetBSTR(), NULL) != S_OK) {
            return SELFREG_E_TYPELIB;
        }
        
        libTree->setRemoveFlag(false);
    } catch (...) {
        return E_UNEXPECTED;
    }
    
    return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DllUnregisterServer
 */
/* -------------------------------------------------------------------------- */

HRESULT PASCAL DllUnregisterServer () {
    if (::UnRegisterTypeLib(TypeLibraryProperties::clsid(), TypeLibraryProperties::majorVer(),
                            TypeLibraryProperties::minorVer(), TypeLibraryProperties::lcid(), SYS_WIN32) != S_OK) {
        return SELFREG_E_TYPELIB;
    }
    
    try {
        std::auto_ptr<RegKeyWrapper> libTree(getRegistryTree(false));
        
        if (libTree.get() == NULL) {
            return SELFREG_E_CLASS;
        }
    } catch (...) {
        return E_UNEXPECTED;
    }
    
    return S_OK;
}