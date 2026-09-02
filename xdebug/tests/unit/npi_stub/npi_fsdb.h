#pragma once

// Minimal test-only declarations for the public NPI primitives used by
// xdebug/src/core/npi/compat.h.  These are intentionally opaque and do not
// mirror or include a vendor header.
typedef unsigned long long npiFsdbTime;

typedef enum npiFsdbValType {
    npiFsdbBinStrVal = 1,
    npiFsdbHexStrVal = 2,
    npiFsdbDecStrVal = 3,
    npiFsdbNonStringVal = 4
} npiFsdbValType;

struct xdebug_test_npi_fsdb_file;
struct xdebug_test_npi_fsdb_sig;
struct xdebug_test_npi_fsdb_vct;

typedef struct xdebug_test_npi_fsdb_file* npiFsdbFileHandle;
typedef struct xdebug_test_npi_fsdb_sig* npiFsdbSigHandle;
typedef struct xdebug_test_npi_fsdb_vct* npiFsdbVctHandle;

typedef struct npiFsdbValue {
    npiFsdbValType format;
    union {
        const char* str;
    } value;
} npiFsdbValue;

#ifdef __cplusplus
extern "C" {
#endif

npiFsdbVctHandle npi_fsdb_create_vct(npiFsdbSigHandle signal);
int npi_fsdb_goto_time(npiFsdbVctHandle vct, npiFsdbTime time);
int npi_fsdb_vct_value(npiFsdbVctHandle vct, npiFsdbValue* value);
void npi_fsdb_release_vct(npiFsdbVctHandle vct);
npiFsdbSigHandle npi_fsdb_sig_by_name(npiFsdbFileHandle file,
                                      const char* path,
                                      void* scope);

#ifdef __cplusplus
}
#endif
