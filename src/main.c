#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse_request.h"
#include "../libwebsockets/lib/libwebsockets.h"
#include "../gridfloat/src/gridfloat.h"
#include "../gridfloat/src/linear.h"


#define JSMN_TOKEN_COUNT 128

#define MAX_GRIDFLOAT_OPEN 10
#define MAX_CHUNK_SIZE (128*128)
#define SEND_HEADER_SIZE 16

/* Change these to pull data from a different file. */
const char *hdr = "/home/bauerca/.usgs/n3f/n46w122/floatn46w122_13.hdr";
const char *flt = "/home/bauerca/.usgs/n3f/n46w122/floatn46w122_13.flt";
static gf_struct gf;

struct per_session_data__super_dumb {
    gf_grid grid;
    int err;
};

typedef enum {
    SUCCESS = 0,
    ERR_BIG_REQUEST = 1
} err_t;

const char *err_msgs[] = {
    "",
    "ERR: BIG"
};

static int
write_header(struct libwebsocket *wsi, char *buf, const char *hdrmsg) {
    int n, m;

    n = sprintf(buf, "%*s", SEND_HEADER_SIZE, hdrmsg);
    m = libwebsocket_write(wsi, buf, n, LWS_WRITE_TEXT);

    printf("header: write %d bytes. wrote %d bytes.\n", n, m);
    
    if (m < n) {
    	lwsl_err("ERROR %d writing header to di socket\n", n);
    	return -1;
    }

    return 0;
}


static int
callback_super_dumb(struct libwebsocket_context *context,
    struct libwebsocket *wsi,
    enum libwebsocket_callback_reasons reason,
    void *user, void *in, size_t len)
{
    int n, m;
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING +
                          SEND_HEADER_SIZE +
                          sizeof(float) * MAX_CHUNK_SIZE +
						  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *hdrp = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	unsigned char *datp = hdrp + SEND_HEADER_SIZE;
    struct per_session_data__super_dumb *pss =
        (struct per_session_data__super_dumb *)user;
    gf_grid *grid = &pss->grid;

    printf("\nrcved msg of length %d\n", len);
    printf("reason: %d\n", reason);
    
    switch(reason) {
	case LWS_CALLBACK_ESTABLISHED:
		lwsl_info("callback_dumb_increment: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		break;
    case LWS_CALLBACK_RECEIVE:
        pss->err = 0;

        parse_request((const char *)in, grid);

        printf("%d\n", MAX_CHUNK_SIZE);
        if (grid->nx * grid->ny > MAX_CHUNK_SIZE)
            pss->err = -1;

        libwebsocket_callback_on_writable(context, wsi);
        break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
        if (pss->err != 0) {
            n = sprintf((char *)hdrp, "%*s", SEND_HEADER_SIZE, "ERR BIG");
		    m = libwebsocket_write(wsi, hdrp, n, LWS_WRITE_TEXT);
		    if (m < n) {
		    	lwsl_err("ERROR %d writing data to di socket\n", n);
		    	return -1;
		    }
            break;
        }
            
        /* Write success header */
        sprintf((char *)hdrp, "%*s", SEND_HEADER_SIZE, "SUCCESS");

        /* Write data */
        memset((void *)datp, 0, sizeof(float) * grid->nx * grid->ny);
        gf_bilinear_interpolate(&gf, grid, (void *)datp);

        /* Bytes we should write. */
        n = SEND_HEADER_SIZE + sizeof(float) * grid->nx * grid->ny;
		m = libwebsocket_write(wsi, hdrp, n, LWS_WRITE_BINARY);
        printf("data: write %d bytes. wrote %d bytes.\n", n, m);
		if (m < n) {
			lwsl_err("ERROR %d writing data to di socket\n", n);
			return -1;
		}
        usleep(1);
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

    gf_open(hdr, flt, &gf);

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
    gf_close(&gf);
    exit(0);
}
