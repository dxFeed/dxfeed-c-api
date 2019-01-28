// Implementation of a library locker object

#include "LibraryLocker.h"

#include <Windows.h>

long LibraryLocker::m_lockCount = 0;
HMODULE LibraryLocker::m_libraryHandle = NULL;

/* -------------------------------------------------------------------------- */
/*
 *	LibraryLocker methods
 */
/* -------------------------------------------------------------------------- */

void LibraryLocker::AddLock () {
	::InterlockedIncrement(&m_lockCount);
}

/* -------------------------------------------------------------------------- */

void LibraryLocker::ReleaseLock () {
	::InterlockedDecrement(&m_lockCount);
}

/* -------------------------------------------------------------------------- */

bool LibraryLocker::IsLocked () {
	return (m_lockCount != 0);
}

/* -------------------------------------------------------------------------- */
/*
 *	DllCanUnloadNow function

 *  One of the functions this library exports. Determines whether it's safe
 *  to unload the library.
 */
/* -------------------------------------------------------------------------- */

HRESULT PASCAL DllCanUnloadNow (void) {
	// It's only safe to unload the library if all the objects given out
	// are Release'd and no server lock is pending

	return (LibraryLocker::IsLocked() ? S_FALSE : S_OK);
}