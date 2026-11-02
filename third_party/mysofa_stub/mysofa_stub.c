/* Stand-ins for libmysofa, which Steam Audio's iOS static library references
 * only for loading custom SOFA HRTF files. Viksen uses Steam Audio's built-in
 * default HRTF, never SOFA files, so these return "not supported". If custom
 * SOFA HRTFs are ever offered, replace this file with libmysofa (BSD-3-Clause). */
#include <stddef.h>

struct MYSOFA_EASY;
struct MYSOFA_LOOKUP;
struct MYSOFA_NEIGHBORHOOD;

enum { ATMOS_MYSOFA_UNSUPPORTED = 10003 }; /* libmysofa's MYSOFA_INVALID_FORMAT range */

struct MYSOFA_EASY *mysofa_open_no_norm(const char *filename, float samplerate, int *filterlength, int *err) {
    (void)filename; (void)samplerate;
    if (filterlength) *filterlength = 0;
    if (err) *err = ATMOS_MYSOFA_UNSUPPORTED;
    return NULL;
}

struct MYSOFA_EASY *mysofa_open_data_no_norm(const char *data, long size, float samplerate, int *filterlength, int *err) {
    (void)data; (void)size;
    return mysofa_open_no_norm(NULL, samplerate, filterlength, err);
}

void mysofa_close(struct MYSOFA_EASY *easy) { (void)easy; }

int mysofa_lookup(struct MYSOFA_LOOKUP *lookup, float *coordinate) { (void)lookup; (void)coordinate; return -1; }

int *mysofa_neighborhood(struct MYSOFA_NEIGHBORHOOD *neighborhood, int pos) { (void)neighborhood; (void)pos; return NULL; }

void mysofa_c2s(float values[3]) { (void)values; }
void mysofa_s2c(float values[3]) { (void)values; }
