#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libwebsockets/lib/libwebsockets.h"
#include "../jsmn/jsmn.h"
#include "../gridfloat/src/gridfloat.h"


#define JSMN_TOKEN_COUNT 128

#define MAX_GRIDFLOAT_OPEN 10

const char *bounds_formats[] = {
    "lrbt",
    "xyab",
    "nwab"
};

struct msg_attr {
    const char *name;
    jsmntype_t type;
    int parent;
};





enum {
    MSG_ATTR_GRID,
    MSG_ATTR_BOUNDS
};

const struct msg_attr msg_schema[] = {
    { "grid", JSMN_OBJECT, 0, parse_grid,  },
    { "bounds", JSMN_OBJECT, MSG_ATTR_GRID },
    { "format", JSMN_STRING, MSG_ATTR_BOUNDS },
    { "values", JSMN_ARRAY, MSG_ATTR_BOUNDS },
    { "resolution", JSMN_ARRAY, MSG_ATTR_GRID },
    { NULL, -1, -1 }
};

//static int
//match_msg_attr(jsmntok_t *token) {
//    struct msg_attr *attr;
//    for (attr = msg_schema; attr->name != NULL; attr++) {
//        
//
//    }
//
//}



static char parse_err_msg[256];





struct per_session_data__super_dumb {
    int number;
};

static int
callback_super_dumb(struct libwebsocket_context *context,
    struct libwebsocket *wsi,
    enum libwebsocket_callback_reasons reason,
    void *user, void *in, size_t len)
{
    int n, m;
    gf_grid grid;
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 512 +
						  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
    struct per_session_data__super_dumb *pss =
        (struct per_session_data__super_dumb *)user;

    printf("rcved msg of length %d\n", len);
    
    switch(reason) {
	case LWS_CALLBACK_ESTABLISHED:
		lwsl_info("callback_dumb_increment: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		break;
    case LWS_CALLBACK_RECEIVE:
        msg_to_grid((const char *)in, &grid);
        libwebsocket_callback_on_writable(context, wsi);
        break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		n = sprintf((char *)p, "%d", pss->number++);
		m = libwebsocket_write(wsi, p, n, LWS_WRITE_TEXT);
		if (m < n) {
			lwsl_err("ERROR %d writing to di socket\n", n);
			return -1;
		}
		break;
    default:
        break;
    }
    return 0;
}

static struct libwebsocket_protocols protocols[] = {
    {
        "super-dumb-proto",
        callback_super_dumb,
        sizeof(struct per_session_data__super_dumb),
        128
    },
    {NULL, NULL, 0, 0}
};

int main(int argc, char **argv) {
    struct libwebsocket_context *context;
    struct lws_context_creation_info info;
    int n;

    memset(&info, 0, sizeof info);
    info.port = 7631;
    info.iface = NULL;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

    context = libwebsocket_create_context(&info);
    if (context == NULL) {
        lwsl_err("libwebsocket init failed.\n");
        exit(1);
    }

    while (n >= 0) {
        n = libwebsocket_service(context, 50);
    }

    libwebsocket_context_destroy(context);
    exit(0);
}
