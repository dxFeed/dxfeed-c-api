// DXFeedCOMC++Sample.cpp : Defines the entry point for the console application.
//

#include "Guids.h"
#include "Interfaces.h"
#include "TypeLibraryManager.h"
#include "Sinks.h"

#include "EventData.h"

#include <memory>
#include <vector>
#include <iostream>

#include <ObjBase.h>
#include <OCIdl.h>
#include <comutil.h>
#include <comdef.h>
#include <tchar.h>

using namespace std;

/* -------------------------------------------------------------------------- */
/*
 *	Helper classes
 */
/* -------------------------------------------------------------------------- */

struct CoInitializer {
    CoInitializer (DWORD modes = COINIT_MULTITHREADED) {
        HRESULT res = ::CoInitializeEx(NULL, modes);
        
        if (!(res == S_OK || res == S_FALSE)) {
            throw "Failed to initialize COM";
        }
    }
    ~CoInitializer () {
        ::CoUninitialize();
    }
};

typedef std::auto_ptr<CoInitializer> CoInitializerPtr;

/* -------------------------------------------------------------------------- */

struct SafeArrayDestroyer {
    SafeArrayDestroyer (SAFEARRAY* sa)
    : m_sa(sa) {}
    ~SafeArrayDestroyer () {
        if (m_sa != NULL) {
            ::SafeArrayDestroy(m_sa);
        }
    }
    
private:

    SAFEARRAY* m_sa;
};

/* -------------------------------------------------------------------------- */

struct SymbolPack {
    const char** symbols;
    int symbolCount;
};

/* -------------------------------------------------------------------------- */

typedef std::vector<std::string> StringVector;

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

struct DXFeedError {};

void processError (HRESULT res, IDXFeed* feed = NULL) {
    if (res == S_OK) {
        cout << "\tSuccess!\n";
        
        return;
    }
    
    cout << "\tFailure! Operation error code = " << res;
    
    if (feed == NULL) {
        cout << "!\n";
        
        throw DXFeedError();
    }
    
    INT errorCode = 0;
    HRESULT errRes = feed->GetLastError(&errorCode);
    
    if (errRes != S_OK) {
        cout << ", DXFeed code retrieval failed!\n";
        
        throw DXFeedError();
    }
    
    cout << ", DXFeed error code = " << errorCode << "!\n";
    
    throw DXFeedError();
}

/* -------------------------------------------------------------------------- */

DWORD attachSink (IDispatch* obj, IDispatch* sink, REFGUID sinkId) {
    IConnectionPointContainer* connContainer = NULL;

    cout << "\tRetrieving an IConnectionPointContainer implementation from the object...\n";

    processError(obj->QueryInterface(IID_IConnectionPointContainer, (void**)&connContainer));

    ComReleaser contReleaser(connContainer);

    IConnectionPoint* connPt = NULL;

    cout << "\tRetrieving an IConnectionPoint implementation from the connection point container...\n";

    processError(connContainer->FindConnectionPoint(sinkId, &connPt));

    ComReleaser ptReleaser(connPt);

    DWORD cookie;

    cout << "\tAttaching a sink object...\n";

    processError(connPt->Advise(sink, &cookie));
    
    return cookie;
}

/* -------------------------------------------------------------------------- */

HRESULT SymbolPackToSafeArray (const SymbolPack& pack, OUT SAFEARRAY*& safeArray) {
    if ((safeArray = ::SafeArrayCreateVector(VT_BSTR, 0, pack.symbolCount)) == NULL) {
        return E_FAIL;
    }

    BSTR* data = NULL;
    HRESULT hr = ::SafeArrayAccessData(safeArray, (void**)&data);
    
    if (hr != S_OK) {
        ::SafeArrayDestroy(safeArray);
        
        return hr;
    }
    
    try {
        for (int i = 0; i < pack.symbolCount; ++i) {
            data[i] = _bstr_t(pack.symbols[i]).Detach();
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

/* -------------------------------------------------------------------------- */

HRESULT SafeArrayToStringVector (SAFEARRAY* safeArray, OUT StringVector& stringVector) {
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

        stringVector.reserve(elemCount);

        for (int i = 0; i < elemCount; ++i) {
            stringVector.push_back(std::string((const char*)_bstr_t(data[i], false)));
        }
    } catch (const _com_error& e) {
        hr = e.Error();
    } catch (...) {
        hr = E_FAIL;
    }

    ::SafeArrayUnaccessData(safeArray);

    return hr;
}
                              
/* -------------------------------------------------------------------------- */

void addSymbol (IDXFeed* feed, IDXSubscription* subscr, const std::string& symbol) {
    cout << "\tThe symbol is " << symbol.c_str() << std::endl;
    
    processError(subscr->AddSymbol(_bstr_t(symbol.c_str()).GetBSTR()), feed);
}

/* -------------------------------------------------------------------------- */

void addSymbols (IDXFeed* feed, IDXSubscription* subscr, const SymbolPack& symbols) {
    cout << "\tThe symbols are: ";
    
    for (int i = 0; i < symbols.symbolCount - 1; ++i) {
        cout << symbols.symbols[i] << ", ";
    }
    
    cout << symbols.symbols[symbols.symbolCount - 1] << std::endl << std::endl;
    
    cout << "\tConverting the symbols into a SAFEARRAY...\n";
    
    SAFEARRAY* safeArray;
    
    processError(SymbolPackToSafeArray(symbols, safeArray));
    
    SafeArrayDestroyer sad(safeArray);
    
    cout << "\tCalling the AddSymbols method...\n";
    
    processError(subscr->AddSymbols(safeArray), feed);
}

/* -------------------------------------------------------------------------- */

void getSymbols (IDXFeed* feed, IDXSubscription* subscr) {
    cout << "\tCalling the GetSymbols method...\n";
    
    SAFEARRAY* safeArray;
    
    processError(subscr->GetSymbols(&safeArray), feed);
    
    SafeArrayDestroyer sad(safeArray);
    
    cout << "\tTransforming the SAFEARRAY into a string vector...\n";
    
    StringVector strings;
    
    processError(SafeArrayToStringVector(safeArray, strings));
    
    cout << "\tThe symbols are: ";

    for (int i = 0; i < (int)strings.size() - 1; ++i) {
        cout << strings[i].c_str() << ", ";
    }

    cout << strings.rbegin()->c_str() << std::endl << std::endl;
}

/* -------------------------------------------------------------------------- */

void pauseThread (int timeout) {
    cout << "Master thread sleeping for " << timeout << " ms...\n";
    ::Sleep (timeout);
    cout << "Master thread woke up\n";
}

/* -------------------------------------------------------------------------- */

void removeSymbols (IDXFeed* feed, IDXSubscription* subscr, const SymbolPack& symbols) {
    cout << "\tThe symbols are: ";

    for (int i = 0; i < symbols.symbolCount - 1; ++i) {
        cout << symbols.symbols[i] << ", ";
    }

    cout << symbols.symbols[symbols.symbolCount - 1] << std::endl << std::endl;

    cout << "\tConverting the symbols into a SAFEARRAY...\n";

    SAFEARRAY* safeArray;

    processError(SymbolPackToSafeArray(symbols, safeArray));

    SafeArrayDestroyer sad(safeArray);

    cout << "\tCalling the RemoveSymbols method...\n";

    processError(subscr->RemoveSymbols(safeArray), feed);
}

/* -------------------------------------------------------------------------- */

void setSymbols (IDXFeed* feed, IDXSubscription* subscr, const SymbolPack& symbols) {
    cout << "\tThe symbols are: ";

    for (int i = 0; i < symbols.symbolCount - 1; ++i) {
        cout << symbols.symbols[i] << ", ";
    }

    cout << symbols.symbols[symbols.symbolCount - 1] << std::endl << std::endl;

    cout << "\tConverting the symbols into a SAFEARRAY...\n";

    SAFEARRAY* safeArray;

    processError(SymbolPackToSafeArray(symbols, safeArray));

    SafeArrayDestroyer sad(safeArray);

    cout << "\tCalling the SetSymbols method...\n";

    processError(subscr->SetSymbols(safeArray), feed);
}

/* -------------------------------------------------------------------------- */

void getSubscrEvents (IDXFeed* feed, IDXSubscription* subscr) {
    static const char* eventTypeNames[dx_eid_count] = {
        "Trade", "Quote", "Summary", "Profile", "Order", "Time&Sale"
    };
    
    INT eventTypes;
    
    cout << "\tCalling the GetEventTypes method...\n";
    processError(subscr->GetEventTypes(&eventTypes), feed);
    
    cout << "\tThe event types are: ";
    
    int eid = dx_eid_begin;

    for (; eid < dx_eid_count; ++eid) {
        if (eventTypes & DX_EVENT_BIT_MASK(eid)) {
            cout << eventTypeNames[eid] << " ";
        }
    }
    
    cout << std::endl;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helper data
 */
/* -------------------------------------------------------------------------- */

const char* symbol_pack_1[] = { "IBM" };
const char* symbol_pack_2[] = { "MSFT", "YHOO", "C" };
const char* symbol_pack_3[] = { "C" };
const char* symbol_pack_4[] = { "MSFT", "YHOO", "IBM" };
const char* symbol_pack_5[] = { "MSFT", "YHOO" };

const SymbolPack symbolPacks[] = {
    { symbol_pack_1, sizeof(symbol_pack_1) / sizeof(symbol_pack_1[0]) },
    { symbol_pack_2, sizeof(symbol_pack_2) / sizeof(symbol_pack_2[0]) },
    { symbol_pack_3, sizeof(symbol_pack_3) / sizeof(symbol_pack_3[0]) },
    { symbol_pack_4, sizeof(symbol_pack_4) / sizeof(symbol_pack_4[0]) },
    { symbol_pack_5, sizeof(symbol_pack_5) / sizeof(symbol_pack_5[0]) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Main sample function
 */
/* -------------------------------------------------------------------------- */

int _tmain(int argc, _TCHAR* argv[]) {
	try {
        TypeLibraryMgrStorage::SetContent(DefTypeLibMgrFactory::CreateInstance());

        DXConnectionTerminationNotifier ctn;
        DXEventListener el;
        
        cout << "\nDXFeed COM wrapper C++ sample started...\n";

        cout << "Initializing the COM subsystem...\n";

        CoInitializerPtr cip;

        try {
            cip.reset(new CoInitializer());
        } catch (...) {
            cout << "Failed to initialize the COM subsystem!\n";

            return -1;
        }

        cout << "COM subsystem initialized successfully!\n";
        cout << "Creating the main DXFeed COM object - the IDXFeed implementation...\n";

        IDXFeed* feed = NULL;

        processError(::CoCreateInstance(CLSID_DXFeed, NULL, CLSCTX_INPROC_SERVER, IID_IDXFeed, (void**)&feed));
        
        ComReleaser feedReleaser(feed);

        {
            std::string loggerPath("log.log");
            bool overwriteLog = true;
            bool showTimezone = true;
            bool verboseMode = true;

            cout << "Initializing the DXFeed library logging mechanism: path is '" << loggerPath.c_str() << "', overwrite mode is " << overwriteLog <<
                ", timezone display is " << showTimezone << ", verbose mode is " << verboseMode << "...\n";

            _bstr_t loggerPathBstr = loggerPath.c_str();
            
            processError(feed->InitLogger(loggerPathBstr.GetBSTR(), overwriteLog, showTimezone, verboseMode), feed);
        }
        
        IDXConnection* connection = NULL;
        
        {
            std::string address("demo.dxfeed.com:7300");
                        
            cout << "Creating a connection: address is '" << address.c_str() << "...\n";
            
            _bstr_t addressBstr = address.c_str();
            
            processError(feed->CreateConnection(addressBstr.GetBSTR(), (IDispatch**)&connection), feed);
        }
        
        ComReleaser connReleaser(connection);        
        
        cout << "Attaching a termination notification sink to the connection object...\n";            
        attachSink(connection, (IDXConnectionTerminationNotifier*)&ctn, DIID_IDXConnectionTerminationNotifier);
        
        IDXSubscription* subscr = NULL;
        
        {
            int eventTypes = DXF_ET_TRADE;
            
            cout << "Creating a subscription based on the created connection: event type bitmask = " << eventTypes << "...\n";
            
            processError(connection->CreateSubscription(eventTypes, (IDispatch**)&subscr), feed);
        }
        
        ComReleaser subscrReleaser(subscr);
        
        cout << "Adding a single symbol to the subscription...\n";        
        addSymbol(feed, subscr, std::string(symbolPacks[0].symbols[0]));
        
        cout << "Attaching an event data listener to the subscription object...\n";        
        attachSink(subscr, (IDXEventListener*)&el, DIID_IDXEventListener);
        
        cout << "Adding multiple symbols to the subscription...\n";
        addSymbols(feed, subscr, symbolPacks[1]);
        pauseThread(5000);
        
        cout << "Clearing the subscription symbols...\n";
        processError(subscr->ClearSymbols(), feed);
        pauseThread(5000);
        
        cout << "Removing multiple symbols from the subscription...\n";
        removeSymbols(feed, subscr, symbolPacks[3]);
        pauseThread(5000);
        
        cout << "Setting the subscription symbols...\n";
        setSymbols(feed, subscr, symbolPacks[4]);
        pauseThread(5000);
        
        cout << "Retrieving the subscription event types...\n";
        getSubscrEvents(feed, subscr);
        pauseThread(5000);
        
        cout << "Adding new symbols to the subscription...\n";
        addSymbols(feed, subscr, symbolPacks[2]);
        pauseThread(5000);
        
        cout << "Getting the subscription symbols...\n";
        getSymbols(feed, subscr);
        pauseThread(5000);
        
        pauseThread(100000);
        
	
	} catch (const DXFeedError&) {
	    return -1;
	} catch (...) {
	    cout << "UNEXPECTED FATAL ERROR!!!\n";
	    
	    return -1;
	}
	
	return 0;
}

