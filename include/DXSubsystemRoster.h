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

/*
 *	Contains a roster of subsystem codes used for error reporting and handling
 */

#ifndef SUBSYSTEM_ROSTER_H_INCLUDED
#define SUBSYSTEM_ROSTER_H_INCLUDED

enum dx_subsystem_code_t {
    sc_memory,
    sc_sockets,
    sc_threads,
    sc_network,
    
    /*  add new subsystem codes above this line 
        also don't forget to modify DXErrorHandling.c to manually
        aggregate the new subsystem's error codes into the error
        handling system */
    
    sc_subsystem_count,
    sc_invalid_subsystem = -1
};

#endif /* SUBSYSTEM_ROSTER_H_INCLUDED */