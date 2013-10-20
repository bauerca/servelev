#include "parse_request.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../jsnn/jsnn.h"

#define JSNN_TOKEN_COUNT 32

static jsnntok_t tokens[JSNN_TOKEN_COUNT];
static jsnn_parser parser;

int parse_request(const char *msg, int *datatype, gf_grid *grid) {
    jsnnerr_t err;
    jsnntok_t *bounds, *fmt, *vals, *res;
    jsnntok_t *dtype_tok;
    char index_str[10], dtype_str[10];
    int pos;
    /* Grid variables */
    double val, lat, lng, width, height;
    int nlat, nlng;

    printf("message: %s\n\n", msg);

    jsnn_init(&parser);
    err = jsnn_parse(&parser, msg, tokens, JSNN_TOKEN_COUNT);
    if (err) {
        printf("error in json parse\n");
        return -1;
    }

    /* Process data before sending? */
    dtype_tok = jsnn_get(tokens, "type", msg, tokens);
    if (jsnn_cmp(dtype_tok, msg, "raw") == 0)
        *datatype = RAW;
    else if (jsnn_cmp(dtype_tok, msg, "shade") == 0)
        *datatype = SHADE;
    else {
        printf("Wrong datatype request:\n");
        return -1;
    }

    /* Get bounds object */
    bounds = jsnn_get(tokens, "bounds", msg, tokens);
    fmt = jsnn_get(bounds, "format", msg, tokens);
    if (fmt->end - fmt->start != 4) {
        printf("Wrong number of characters in bounds format string: %d", fmt->end - fmt->start);
        return -1;
    }
    vals = jsnn_get(bounds, "values", msg, tokens);
    if (vals->size != 4) {
        printf("Need 4 bounds values. Got %d", vals->size);
        return -1;
    }

    for (pos = fmt->start; pos < fmt->end; pos++) {
        sprintf(index_str, "[%d]", pos - fmt->start);
        val = atof(msg + jsnn_get(vals, index_str, msg, tokens)->start);

        switch (msg[pos]) {
        case 'N':
            lat = val;
            break;
        case 'W':
            lng = -val;
            break;
        case 'w':
            width = val;
            break;
        case 'h':
            height = val;
            break;
        }
    }

    /* Get grid resolution */
    res = jsnn_get(tokens, "resolution", msg, tokens);
    switch (res->type) {
    case JSNN_ARRAY:
        nlng = atoi(msg + jsnn_get(res, "[0]", msg, tokens)->start);
        nlat = atoi(msg + jsnn_get(res, "[1]", msg, tokens)->start);
        break;
    case JSNN_PRIMITIVE:
        nlng = nlat = atoi(msg + res->start);
        break;
    default:
        printf("Got weird value for grid resolution.\n");
        return -1;
    }

    printf("lat = %f\n"
        "lng = %f\n"
        "width = %f\n"
        "height = %f\n"
        "nlat = %d\n"
        "nlng = %d\n",
        lat, lng, width ,height, nlat, nlng);
    gf_init_grid_point(grid, lat, lng, width, height, nlat, nlng);
    return 0;
}

