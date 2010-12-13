// DXFeedCOM.cpp : Defines the entry point for the DLL application.
//

#include "TypeLibraryManager.h"
#include "Feed.h"
#include "LibraryLocker.h"

#include <Windows.h>

static TypeLibraryManager* g_typeLibraryMgr = NULL;

BOOL APIENTRY DllMain (HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        TypeLibraryMgrStorage::SetContent(DefTypeLibMgrFactory::CreateInstance());
        DXFeedStorage::SetContent(DefDXFeedFactory::CreateInstance());
        LibraryLocker::SetLibraryHandle((HMODULE)hModule);
        
        ::DisableThreadLibraryCalls((HMODULE)hModule);
        
        break;
    case DLL_PROCESS_DETACH:
        {
            TypeLibraryManager* tlm = TypeLibraryMgrStorage::GetContent();
        
            if (tlm != NULL) {
                delete tlm;
            }
            
            TypeLibraryMgrStorage::SetContent(NULL);
        }
        
        {
            IDXFeed* feed = DXFeedStorage::GetContent();
            
            if (feed != NULL) {
                feed->Release();
            }
            
            DXFeedStorage::SetContent(NULL);
        }
    }
    
    return TRUE;
}

