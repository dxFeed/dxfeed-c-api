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
 
#include "DXAlgorithms.h"

/* -------------------------------------------------------------------------- */
/*
 *	Array functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (size_t new_size, size_t* capacity) {
    if (new_size > *capacity) {
        *capacity = (size_t)((double)*capacity * 1.5) + 1;

        return true;
    }

    if (new_size < *capacity && ((double)new_size / *capacity) < 1.5) {
        *capacity = new_size;

        return true;
    }

    return false;
}