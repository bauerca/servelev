#ifndef PARSE_REQUEST_H
#define PARSE_REQUEST_H

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

struct request_grid_bounds {
    char format[4];
    double values[4];
};

struct request_grid {
    struct request_grid_bounds bounds;
    int resolution[2];
};

struct request {
    struct request_grid grid;
};


#endif
