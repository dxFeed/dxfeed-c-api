// ErrorReport.h
// Contains functions for manipulating the information about errors

#ifndef ERROR_REPORT_H_INCLUDED
#define ERROR_REPORT_H_INCLUDED

enum dx_errcode_t {
    ECSUCCESS = 0,
    ECBADSUBSYSID = -1,
    ECBADERRCODE = -2,
    ECBADERRCODECOUNT = -3,
    ECNULLERRCODES = -4,
    ECNOERROR = -5
};

enum dx_subsystem_id {
    SS_DataSerializer = 1,
};
/* -------------------------------------------------------------------------- */
/*
 *	The error manipulation functions
 */
/* -------------------------------------------------------------------------- */

/*  registers the subsystem's error codes.
    that's used to make a complete roster of errors the subsystem can report.
    
    Input:
        subsystem_id - the subsystem ID that the subsystem chooses by itself;
                       should be unique across the project
        errcode_count - the number of error codes the subsystem wants
                        to register; must not be zero
        errcodes - a pointer to an array of integer error codes the subsystem
                   registers; must not be NULL
        errcodedescr - a pointer to an array of error code text descriptions
                       in ANSI; is optional 
                       
    Return value is one of the following:
        ECSUCCESS - OK
        ECBADSUBSYSID - a subsystem ID to register is already in use
        ECBADERRCODECOUNT - an error code count is non-positive
        ECNULLERRCODES - a pointer to the error code array is NULL */
                            
enum dx_errcode_t dx_reg_errcodes (int subsystem_id, int errcode_count,
                              const int* errcodes,
                              const char* const* errcodedescr);

/* reports an error. */

enum dx_errcode_t dx_set_last_error (int subsystem_id, int errcode) { return ECSUCCESS; };

/* retrieves an information about a previously reported error. */

enum dx_errcode_t dx_get_last_error (int* subsystem_id, int* errcode, const char** errcodedescr) { return ECSUCCESS; };








#endif // ERROR_REPORT_H_INCLUDED