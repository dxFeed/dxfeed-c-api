// Here we define our IClassFactory implementation

#include "LibraryLocker.h"
#include "Guids.h"
#include "Feed.h"
#include "Interfaces.h"

#include <Windows.h>
#include <ObjBase.h>

/* -------------------------------------------------------------------------- */
/*
 *	DXFeedClassFactory struct
 */
/* -------------------------------------------------------------------------- */

struct DXFeedClassFactory : public IClassFactory {
	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void **ppvObject);
	virtual ULONG STDMETHODCALLTYPE AddRef ();
	virtual ULONG STDMETHODCALLTYPE Release ();

	virtual HRESULT STDMETHODCALLTYPE CreateInstance (IUnknown *pUnkOuter, REFIID riid, void **ppvObject);
	virtual HRESULT STDMETHODCALLTYPE LockServer (BOOL fLock);
};

static DXFeedClassFactory classFactorySingleton;

/* -------------------------------------------------------------------------- */
/*
 *	DXFeedClassFactory methods implementation
 */
/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeedClassFactory::QueryInterface (REFIID riid, void **ppvObject) {
	if (ppvObject == NULL) {
	return E_POINTER;
	}

	*ppvObject = NULL;

	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
	AddRef();

	*ppvObject = this;

	return NOERROR;
	}

	// We don't know about any other GUIDs
	return E_NOINTERFACE;
}

/* -------------------------------------------------------------------------- */

ULONG STDMETHODCALLTYPE DXFeedClassFactory::AddRef () {
	// Someone is obtaining our IClassFactory, so that must lock the library
	// (at least) until the object is released
	LibraryLocker::AddLock();

	// Since we never actually allocate/free the IClassFactory object (i.e., we
	// use just a static one), we don't need to maintain a separate
	// reference count for our IClassFactory. We'll just tell the caller
	// that there's at least one of our IClassFactory objects existing
	return 1;
}

/* -------------------------------------------------------------------------- */

ULONG STDMETHODCALLTYPE DXFeedClassFactory::Release () {
	// Releasing the library lock previously obtained during AddRef()
	LibraryLocker::ReleaseLock();

	// No deallocation of IClassLibrary really takes place. So the result is the same
	return 1;
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeedClassFactory::CreateInstance (IUnknown *pUnkOuter, REFIID riid, void **ppvObject) {
	if (ppvObject == NULL) {
	return E_POINTER;
	}

	// Assume an error by clearing caller's handle
	*ppvObject = NULL;

	if (pUnkOuter != NULL) {
	// Aggregation is not supported

	return CLASS_E_NOAGGREGATION;
	}

	IDXFeed* objToCreate = NULL;

	if ((objToCreate = DXFeedStorage::GetContent()) == NULL) {
	return E_OUTOFMEMORY;
	}

	// Let's allow the DXFeed's QueryInterface fill in the client's object handle.
	// It will also check the GUID and increase the object's reference count on success
	return objToCreate->QueryInterface(riid, ppvObject);
}

/* -------------------------------------------------------------------------- */

HRESULT STDMETHODCALLTYPE DXFeedClassFactory::LockServer (BOOL fLock) {
	// Using the same locking mechanism as for AddRef()/Release()

	if (fLock) {
	LibraryLocker::AddLock();
	} else {
	LibraryLocker::ReleaseLock();
	}

	return S_OK;
}

/* -------------------------------------------------------------------------- */
/*
 *	DllGetClassObject function

 *  One of the functions our COM DLL exports. It's used to acquire an
 *  IClassFactory object to later create the top-level objects with its help
 */
/* -------------------------------------------------------------------------- */

HRESULT PASCAL DllGetClassObject (REFCLSID objGuid, REFIID factoryGuid, void **factoryHandle) {
	// Our class factory only supports the DXFeed class, so we must check whether it's
	// the one the client expects us to support

	if (!IsEqualCLSID(objGuid, CLSID_DXFeed)) {
	// The GUID passed was not recognized as one supported by this library

	*factoryHandle = NULL;

	return CLASS_E_CLASSNOTAVAILABLE;
	}

	// We need to fill in the client's factory handle. That's the job for our
	// factory's QueryInterface. It will also check the passed GUID's validity
	return classFactorySingleton.QueryInterface(factoryGuid, factoryHandle);
}