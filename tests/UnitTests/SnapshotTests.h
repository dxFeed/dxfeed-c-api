/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
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

#ifndef SNAPSHOT_TESTS_H_INCLUDED
#define SNAPSHOT_TESTS_H_INCLUDED

#include "PrimitiveTypes.h"

int snapshot_initialization_test(void);
int snapshot_duplicates_test(void);
int snapshot_subscription_test(void);
int snapshot_multiply_subscription_test(void);
int snapshot_subscription_and_events_test(void);
int snapshot_symbols_test(void);

int snapshot_all_test(void);

int snapshot_all_unit_test(void);

#endif /* SNAPSHOT_TESTS_H_INCLUDED */