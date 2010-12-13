// Definition of a library locker object

#pragma once

#include <Windows.h>

struct LibraryLocker {
    static void AddLock ();
    static void ReleaseLock ();
    static bool IsLocked ();
    
    static HMODULE GetLibraryHandle () {
        return m_libraryHandle;
    }
    
    static void SetLibraryHandle (HMODULE libraryHandle) {
        m_libraryHandle = libraryHandle;
    }

private:

    static long m_lockCount;
    static HMODULE m_libraryHandle;
};