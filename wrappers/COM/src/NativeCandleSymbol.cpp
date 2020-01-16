#include <ComDef.h>
#include <stdexcept>
#include "DXFeed.h"
#include "Interfaces.h"
#include "NativeCandleSymbol.h"


NativeCandleSymbol::NativeCandleSymbol()
	: mCandleAttributes(NULL)
{
}

NativeCandleSymbol::NativeCandleSymbol(IDXCandleSymbol* symbol)
{
	if (symbol == NULL)
		throw std::invalid_argument("The candle symbol object is null");

	_bstr_t baseSymbolWrapper;
	WCHAR exchangeCode;
	INT price;
	INT session;
	INT periodType;
	DOUBLE periodValue;
	INT alignment;
	DOUBLE priceLevel;

	if (FAILED(symbol->get_BaseSymbol(baseSymbolWrapper.GetAddress())) ||
		FAILED(symbol->get_ExchangeCode(&exchangeCode)) ||
		FAILED(symbol->get_Price(&price)) ||
		FAILED(symbol->get_Session(&session)) ||
		FAILED(symbol->get_PeriodType(&periodType)) ||
		FAILED(symbol->get_PeriodValue(&periodValue)) ||
		FAILED(symbol->get_Alignment(&alignment)) ||
		FAILED(symbol->get_PriceLevel(&priceLevel)))
	{
		throw std::exception("Can't get symbol parameters.");
	}

	ERRORCODE errCode = dxf_create_candle_symbol_attributes((const wchar_t*)baseSymbolWrapper,
	                                                        exchangeCode, periodValue,
	                                                        (dxf_candle_type_period_attribute_t)periodType,
	                                                        (dxf_candle_price_attribute_t)price,
	                                                        (dxf_candle_session_attribute_t)session,
	                                                        (dxf_candle_alignment_attribute_t)alignment,
	                                                        (dxf_double_t)priceLevel, &mCandleAttributes);

	if (errCode == DXF_FAILURE)
		throw std::exception("Can't create Candle symbol attribute object.");
}

NativeCandleSymbol::~NativeCandleSymbol()
{
	if (mCandleAttributes != NULL)
	{
		dxf_delete_candle_symbol_attributes(mCandleAttributes);
	}
}

dxf_candle_attributes_t& NativeCandleSymbol::operator*()
{
	return mCandleAttributes;
}
