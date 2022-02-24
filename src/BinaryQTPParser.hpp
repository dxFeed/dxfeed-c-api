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

#pragma once

namespace dx {

class Connection;

class BinaryQTPParser {
	Connection* connection_;
	void* bufferedInputConnectionContext_;
	int currentTimeMark_;  // =0 when unknown

public:

	explicit BinaryQTPParser(Connection* connection);

	void setContext(void* bufferedInputConnectionContext);

	void setCurrentTimeMark(int currentTimeMark);

	void parseHeartbeat();
};

}  // namespace dx
