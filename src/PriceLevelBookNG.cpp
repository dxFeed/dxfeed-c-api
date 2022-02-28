/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Devexperts LLC.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

extern "C" {

#include "PriceLevelBookNG.h"
}

#include "PriceLevelBookNG.hpp"

extern "C" {

dxf_price_level_book_v2_t dx_create_price_level_book_v2(dxf_connection_t connection, dxf_const_string_t symbol,
														const char* source, int levels_number) {
	auto plb = dx::PriceLevelBook::create(connection, dx::StringConverter::wStringToUtf8(symbol), std::string(source),
										  levels_number < 0 ? 0 : static_cast<std::size_t>(levels_number));

	return static_cast<dxf_price_level_book_v2_t>(plb);
}

int dx_close_price_level_book_v2(dxf_price_level_book_v2_t book) {
	if (book == nullptr) {
		return false;
	}

	delete static_cast<dx::PriceLevelBook*>(book);

	return true;
}

int dx_set_price_level_book_listeners_v2(dxf_price_level_book_v2_t book,
										 dxf_price_level_book_listener_t onNewBookHandler,
										 dxf_price_level_book_listener_t onBookUpdateHandler,
										 dxf_price_level_book_inc_listener_t onIncrementalChangeHandler,
										 void* userData) {
	if (book == nullptr) return false;

	auto* plb = static_cast<dx::PriceLevelBook*>(book);

	if (userData) {
		plb->setUserData(userData);
	}

	if (onNewBookHandler != nullptr) {
		plb->setOnNewBook([onNewBookHandler](const dx::PriceLevelChanges& changes, void* userData) {
			dx::PriceLevelBookDataBundle bundle{changes};

			onNewBookHandler(&bundle.priceLevelBookData, userData);
		});
	}

	if (onBookUpdateHandler != nullptr) {
		plb->setOnBookUpdate([onBookUpdateHandler](const dx::PriceLevelChanges& changes, void* userData) {
			dx::PriceLevelBookDataBundle bundle{changes};

			onBookUpdateHandler(&bundle.priceLevelBookData, userData);
		});
	}

	if (onIncrementalChangeHandler != nullptr) {
		plb->setOnIncrementalChange(
			[onIncrementalChangeHandler](const dx::PriceLevelChangesSet& changesSet, void* userData) {
				dx::PriceLevelBookDataBundle removalsBundle{changesSet.removals};
				dx::PriceLevelBookDataBundle additionsBundle{changesSet.additions};
				dx::PriceLevelBookDataBundle updatesBundle{changesSet.updates};

				onIncrementalChangeHandler(&removalsBundle.priceLevelBookData, &additionsBundle.priceLevelBookData,
										   &updatesBundle.priceLevelBookData, userData);
			});
	}

	return true;
}
}