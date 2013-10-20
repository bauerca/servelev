#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse_request.h"
#include "../libwebsockets/lib/libwebsockets.h"
#include "../gridfloat/src/gridfloat.h"
#include "../gridfloat/src/linear.h"
#include "../gridfloat/src/quadratic.h"
#include "../gridfloat/src/gfpng.h"


#define JSMN_TOKEN_COUNT 128

#define MAX_GRIDFLOAT_OPEN 10
#define MAX_VALUES (128*128)
#define SEND_HEADER_SIZE 16
#define PROTO_BUF 256

/* Change these to pull data from a different file. */
const char *hdr = "/home/bauerca/.usgs/n3f/n46w122/floatn46w122_13.hdr";
const char *flt = "/home/bauerca/.usgs/n3f/n46w122/floatn46w122_13.flt";
static gf_struct gf;


static int
gf_set_byte_zero(void **data_ptr) {
    char **dptr = (char **)data_ptr;
    (*dptr)[0] = 0;
    dptr[0]++;
    return 0;
}

typedef enum {
    SUCCESS = 0,
    ERR_BIG_REQUEST = 1
} err_t;

const char *err_msgs[] = {
    "",
    "ERR: BIG"
};


struct per_session_data__super_dumb {
    gf_grid grid;
    int err;
    int datatype;
    double n_sun[3];
};

static int
callback_super_dumb(struct libwebsocket_context *context,
    struct libwebsocket *wsi,
    enum libwebsocket_callback_reasons reason,
    void *user, void *in, size_t len)
{
    int n, m;
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING +
                          SEND_HEADER_SIZE +
                          sizeof(float) * MAX_VALUES +
						  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *hdrp = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	unsigned char *datp = hdrp + SEND_HEADER_SIZE;
    struct per_session_data__super_dumb *pss =
        (struct per_session_data__super_dumb *)user;
    gf_grid *grid = &pss->grid;
    size_t px_size;
    double nsun[3] = {0, 0, 1};

    //printf("\nrcved msg of length %d\n", len);
    //printf("reason: %d\n", reason);
    
    switch(reason) {
	case LWS_CALLBACK_ESTABLISHED:
		lwsl_info("callback_dumb_increment: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		break;
    case LWS_CALLBACK_RECEIVE:
        pss->err = 0;
        pss->err = parse_request((const char *)in, &pss->datatype, grid);

        printf("%d\n", MAX_VALUES);
        if (grid->nx * grid->ny > MAX_VALUES)
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

        switch (pss->datatype) {
        case SHADE:
            gf_biquadratic(&gf, grid, nsun, //(void *)pss->n_sun,
                &gf_relief_shade_kernel, &gf_set_byte_zero, (void *)datp);
            px_size = 1;
            break;
        case RAW:
            /* Write data */
            //memset((void *)datp, 0, sizeof(float) * grid->nx * grid->ny);
            gf_bilinear_interpolate(&gf, grid, (void *)datp);
            px_size = sizeof(float);
            break;
        default:
            return -1;
        }

        /* Bytes we should write. */
        n = SEND_HEADER_SIZE + px_size * grid->nx * grid->ny;


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
        SEND_HEADER_SIZE + sizeof(float) * MAX_VALUES,
        0
        //sizeof(float) * MAX_VALUES
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
