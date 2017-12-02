// Definition of the connection interface

#pragma once

struct IDXConnection;

/* -------------------------------------------------------------------------- */
/*
 *	DefDXConnectionFactory class

 *  creates the instances implementing the IDXConnection interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXConnectionFactory {
	static IDXConnection* CreateInstance (const char* address);
	static IDXConnection* CreateAuthBasicInstance(const char* address, const char* user, const char* password);
	static IDXConnection* CreateAuthBearerInstance(const char* address, const char* token);
	static IDXConnection* CreateAuthCustomInstance(const char* address, const char* authsheme, const char* authdata);
};