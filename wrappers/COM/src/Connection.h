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
};