#include "parse_request.h"

#include <string.h>
#include <stdlib.h>

#include "../jsmn/jsmn.h"

#define MAX_OBJ_DEPTH 32

struct json_attr {
    const char *path[MAX_OBJ_DEPTH];
    jsmntype_t type;
};

struct json_attr_arr {
    const char *path[MAX_OBJ_DEPTH];
    jsmntype_t type;
    size_t len;
    int (*put)(size_t index, const char *value, void *user);
};

struct json_attr_str {
    const char *path[MAX_OBJ_DEPTH];
    jsmntype_t type;
    size_t len;
    char *buf;
};


/*
 * Typedef for a function that is used to parse a token
 * found by the jsmn parser.
 */
typedef int (attr_parser)(const char *json, jsmntok_t *token,
    void *user);


struct attr_parser_info {
    const char **path;
    jsmntype_t type;
    attr_parser *parser;
};



/*
 * grid.bounds.format
 * ------------------
 * "lrbt", "xywh", or "nwwh"
 */
static const char *grid_bounds_format_path[4] = {
    "grid",
    "bounds",
    "format",
    NULL
};

static int
attr_parser__grid_bounds_format(const char *json, jsmntok_t *token,
    void *user)
{
    struct request *r = (struct request *)user;
    strncpy(r->grid.bounds.format, json + token->start,
        token->end - token->start);
}

static struct attr_parser_info grid_bounds_format =
{
    grid_bounds_format_path,
    JSMN_STRING,
    attr_parser__grid_bounds_format
};



/*
 * grid.bounds.values
 * ------------------
 * A length-4 array of doubles representing the bounding box.
 * Interpretation of array of values depends on grid.bounds.format.
 */
static int
put_grid_bounds_value(size_t index, const char *value, void *user)
{
    struct request *r = (struct request *)user;
    r->grid.bounds.values[index] = atof(value);
    return 0;
}

static struct attr_array grid_bounds_values = {
    { "values", "bounds", "grid", NULL },
    JSMN_ARRAY,
    4,
    put_grid_bounds_value
};




static jsmn_parser parser;
static jsmntok_t tokens[JSMN_TOKEN_COUNT];


static int
token_strcmp(const char *json, jsmntok_t *token, const char *s) {
    return strncmp(json + token->start, s, token->end - token->start);
}


static int
parse_value(const jsmntok_t *ntok, const jsmntok_t *vtok,
    const struct json_attr *attr, void *data)
{
    const struct json_attr_arr *attr_arr;
    const struct json_attr_str *attr_str;
    size_t i, len;
    char null_swap;


    switch (vtok->type) {
    case JSMN_ARRAY:
        attr_arr = (const struct json_attr_arr *)attr;
        for (i = 0; i < attr_arr->len; i++) {
            null_swap = json[vtok->end];
            json[vtok->end] = '\0';
            (*attr_arr->put)(i, json + vtok->start, data);
            json[vtok->end] = null_swap;
        }
        break;
    case JSMN_STRING:
        attr_str = (const struct json_attr_str *)attr;
        len = vtok->end - vtok->start;
        if ((len + 1) > attr_str->len) {
            /* Token string is too long for buffer. */
            return -1;
        }
        strncpy(attr_str->buf, json + vtok->start, len);
        attr_str->buf[len] = '\0';
        break;
    }

}

static int
parse_request(const char *msg, const struct json_attr *attrs, void *data) {
    jsmnerr_t err;
    jsmntok_t *ntok, *vtok;
    const char *name, * const *path_part;
    const struct json_attr *attr;

    printf("message: %s\n\n", msg);

    jsmn_init(&parser);
    err = jsmn_parse(&parser, msg, tokens, JSMN_TOKEN_COUNT);
    if (err) {
        printf("error in json parse\n");
        return -1;
    }

    for (ntok = tokens;; ntok++) {
        if (ntok->start == ntok->end)
            break;

        if (ntok->pair_type == JSMN_NAME) {
            for (attr = attrs; attr->path != NULL; attr++) {
                for (path_part = attr->path; *path_part != NULL; path_part++) {
                    if (token_strcmp(msg, ntok, *path_part) != 0)
                        break;
                }
                if (*path_part == NULL) {
                    /* Path match */
                    vtok = ntok + 1;
                    if (vtok->type == attr->type)
                        parse_value(ntok, vtok, attr, data);
                }
            }
        }

        printf("Token\n  type: %d\n  Position: %d\n  End: %d\n  "
            "Parent: %d\n  Size: %d\n  Value: %.*s\n",
            token->type,
            token->start,
            token->end,
            token->parent,
            token->size,
            token->end - token->start,
            msg + token->start);
    }

    return 0;
}



/*
 * Parses bounds info from json message and stores in gridfloat
 * grid struct.
 *
 * @param token Must point to the token in the jsmn array of
 *     tokens that 
 *
 * @returns A pointer to the next json token that should be
 *     parsed. NULL if parsing failed. If NULL is returned,
 *     an error message has been set in parse_err_msg.
 */
static jsmntok_t *
parse_attr(const char *json, jsmntok_t *token, gf_grid *grid) {
    

    int parent, attrs_parsed;


    if (token_strcmp(json, token, "bounds") != 0)
        return NULL;

    parent = token->parent;
    
    /* First token after "bounds" attr name token is bounds object.
       Confirm this. Also confirm that object's parent is same
       as bounds attr name. */
    token++;
    if (token->type != JSMN_OBJECT || token->parent != parent)
        return -1;

    /* Next token should be "fmt" or "values" attr name. */
    attrs_parsed = 0;
    while (attrs_parsed < 2) {
        token++;

        if (token_strcmp(json, token, "fmt") == 0)
            token = parse_bounds_fmt(json, token);
        else if (token_strcmp(json, token, "fmt") == 0)
            token = parse_bounds_fmt(json, token);


        attrs_parsed++;

    }



}



/*
 * Put it all together
 */
struct attr_parser_info parsers[] = {
    {
        (const char **)grid_bounds_format_path,
        JSMN_STRING,
        attr_parser__grid_bounds_format
    },
    { grid_bounds_values_path, JSMN_ARRAY, parse_grid_bounds_values },
    { grid_resolution_path, JSMN_ARRAY, parse_grid_resolution }
};


static int
parse_request(const char *json, struct request *req) {
    struct msg m;

    char grid_bounds_values_path[3][128] = { "grid", "bounds", "values" };
    char grid_resolution_path[2][128] = { "grid", "resolution" };


    parse_json(msg, parsers, &m);
