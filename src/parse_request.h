#ifndef PARSE_REQUEST_H
#define PARSE_REQUEST_H

#include "../gridfloat/src/gridfloat.h"

/*
 * {
 *   "grid": {
 *     "bounds": {
 *       "format": "string",
 *       "values": "array<double>(4)"
 *     },
 *     "resolution": "array<int>(2)"
 *   }
 * }
 *
 */


int parse_request(const char *msg, gf_grid *grid);


#endif
