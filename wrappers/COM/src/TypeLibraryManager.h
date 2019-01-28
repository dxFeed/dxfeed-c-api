// Definition of a type library manager interface

#pragma once

#include <objbase.h>

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryProperties class
 */
/* -------------------------------------------------------------------------- */

struct TypeLibraryProperties {
	static REFGUID clsid ();
	static WORD majorVer ();
	static WORD minorVer ();
	static LCID lcid ();
};

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryManager interface
 */
/* -------------------------------------------------------------------------- */

struct TypeLibraryManager {
	virtual ~TypeLibraryManager () {}

	virtual HRESULT GetTypeInfo (REFGUID iid, OUT ITypeInfo*& typeInfo) = 0;
};

/* -------------------------------------------------------------------------- */
/*
 *	DefTypeLibMgrFactory class

 *  creates the default implementations of TypeLibraryManager interface
 */
/* -------------------------------------------------------------------------- */

struct DefTypeLibMgrFactory {
	static TypeLibraryManager* CreateInstance ();
};

/* -------------------------------------------------------------------------- */
/*
 *	TypeLibraryMgrStorage class

 *  stores a single instance of TypeLibraryManager
 */
/* -------------------------------------------------------------------------- */

struct TypeLibraryMgrStorage {
	static TypeLibraryManager* GetContent () {
	return m_tlm;
	}

	static void SetContent (TypeLibraryManager* value) {
	m_tlm = value;
	}

private:

	static TypeLibraryManager* m_tlm;
};