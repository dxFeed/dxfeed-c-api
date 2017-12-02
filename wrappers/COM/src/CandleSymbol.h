#pragma once

struct IDXCandleSymbol;

/* -------------------------------------------------------------------------- */
/*
 *	DefDXCandleSymbolFactory class

 *  creates the instances implementing the IDXCandleSymbol interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXCandleSymbolFactory {
	static IDXCandleSymbol* CreateInstance();
};