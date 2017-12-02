#pragma once

#include "DXTypes.h"

class NativeCandleSymbol {

public:
	NativeCandleSymbol(IDXCandleSymbol*);
	virtual ~NativeCandleSymbol();
	dxf_candle_attributes_t& operator*();

private:
	NativeCandleSymbol();
	dxf_candle_attributes_t mCandleAttributes;
};
