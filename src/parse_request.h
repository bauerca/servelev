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

#define MAX_OBJ_DEPTH 32

typedef enum {
    JSNN_PATH_NAME,
    JSNN_PATH_INDEX
} jsnnpath_t;

/*
Example:
    Given,

        {
            "dogs": [
                {
                    "name": "spot",
                    "breed": "terrier"
                },
                {
                    "name": "gracie",
                    "breed": "golden retriever"
                }
            ]
        }

    one would want to specify the following paths
    to aid in capturing data.

    // Define your data structures.
    struct dog dogs[128];

    struct dog {
        char name[32];
        char breed[128];
    };

    // Define your data extractors.
    int set_dog_name(jsnn_path *path, const char *name, void *data) {...}
    int set_dog_breed(jsnn_path *path, const char *breed, void *data) {...}

    // Define your schema.
    struct jsnn_path dogs_path = {
        JSNN_PATH_NAME,
        "dogs",
        NULL,
        NULL
    };

    struct jsnn_path dog_path = {
        JSNN_PATH_INDEX,
        NULL,
        &dogs,
        NULL
    };

    struct jsnn_path dog_name_path = {
        JSNN_PATH_NAME,
        "name",
        &dog,
        set_dog_name
    };

    struct jsnn_path dog_breed_path = {
        JSNN_PATH_NAME,
        "breed",
        &dog,
        set_dog_breed
    };

    // Put the schema together for jsnn om noming.
    struct jsnn_path schema[] = {
        &dog_breed_path,
        &dog_name_path,
        NULL
    };

    jsnn_parse(json, schema);

*/
struct jsnn_path {
    jsnnpath_t type;
    const char *name;
    jsnn_path *parent;
};


typedef int 

/*

Base struct for defining an attribute to be extracted
from the json string.

*/
struct jsnn_attr {
    jsnn_path *path;
    jsnntype_t type;
};

/*

Substruct of jsnn_attr for defining an array attribute
to be extracted from json string.

TODO: Aw crap. What happens when the array type is
not a string or primitive? Surely the callback style
declared below is not useful, since the value argument
in this case will be the entire object/array string.

Fix?: Make path a more complicated object. e.g.

    path = { "dogs", JSMN_ARRAY_INDEX, "name" }

Then in callback, give user the matched path

    { "dogs", 0, "name" }

*/
struct jsnn_attr_arr {
    const char *path[MAX_OBJ_DEPTH];
    jsnntype_t type;
    size_t len;
    int (*put)(size_t index, const char *value, void *user);
};

struct jsnn_attr_str {
    const char *path[MAX_OBJ_DEPTH];
    jsnntype_t type;
    size_t len;
    char *buf;
};



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
