#ifndef PARSE_REQUEST_H
#define PARSE_REQUEST_H

#include "../gridfloat/src/gridfloat.h"

/*
 * {
 *   "type": "shade | raw",
 *   "grid": {
 *     "bounds": {
 *       "format": "NWwh",
 *       "values": "array<double>(4)"
 *     },
 *     "resolution": "array<int>(2)"
 *   }
 * }
 *
 */

enum {
    RAW,
    SHADE
};

int parse_request(const char *msg, int *datatype, gf_grid *grid);


#endif
