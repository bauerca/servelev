#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse_request.h"
#include "../libwebsockets/lib/libwebsockets.h"
#include "../gridfloat/src/gridfloat.h"
#include "../gridfloat/src/linear.h"
#include "../gridfloat/src/quadratic.h"
#include "../gridfloat/src/gfpng.h"
#include "../jsnn/jsnn.h"


#define JSMN_TOKEN_COUNT 128

#define MAX_GRIDFLOAT_OPEN 10
#define MAX_VALUES (128*128)
#define SEND_STATUS_SIZE 16
#define SEND_BOUNDS_SIZE (4 * sizeof(double))
#define SEND_DIMS_SIZE (2 * sizeof(int))
#define SEND_HEADER_SIZE ( \
    SEND_STATUS_SIZE + \
    SEND_BOUNDS_SIZE + \
    SEND_DIMS_SIZE \
)
#define SEND_GRID_SIZE sizeof(gf_grid)
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

#define JSNN_TOKEN_COUNT 32

static jsnntok_t tokens[JSNN_TOKEN_COUNT];
static jsnn_parser parser;

#define CHUNK_DIM 32
#define CHUNK_SIZE (sizeof(float) * CHUNK_DIM * CHUNK_DIM)
/* Overestimate map resolution? 4k x 4k? = 16m pixels.
   If minimum chunk size is ~ 16x16, max chunks to ship
   for a given view 62500. */
#define QUEUE_SIZE 1024

struct per_session_data__super_dumb {
    gf_grid queue[QUEUE_SIZE];
    int count; /* Valid grids in queue. */
    int send;  /* Grid to send next. */
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
                          CHUNK_SIZE +
						  LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *hdrp = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	unsigned char *datp = hdrp + SEND_HEADER_SIZE;
    unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
    struct per_session_data__super_dumb *pss =
        (struct per_session_data__super_dumb *)user;
    size_t px_size;
    double nsun[3] = {0, 0, 1};

    //printf("%d\n", SEND_HEADER_SIZE + CHUNK_SIZE);

    jsnnerr_t err;
    jsnntok_t *bounds, *fmt, *vals, *res;
    jsnntok_t *dtype_tok;
    char index_str[10], dtype_str[10];
    int pos;
    /* Grid variables */
    double val, lat, lng, width, height;
    int nlat, nlng;
    const char *msg;
    double l, r, t, b, dx, dy, chw, chh;
    gf_grid *queue, *grid;
    int nx, ny, px, py, i, j, ii;

    //printf("\nrcved msg of length %d\n", len);
    //printf("reason: %d\n", reason);
    
    switch(reason) {
	case LWS_CALLBACK_ESTABLISHED:
		lwsl_info("callback_dumb_increment: "
						 "LWS_CALLBACK_ESTABLISHED\n");
		break;
    case LWS_CALLBACK_RECEIVE:
        pss->err = 0;
        //pss->err = parse_request((const char *)in, &pss->datatype, grid);
    
        msg = (const char *)in;
        printf("message: %s\n\n", msg);
    
        jsnn_init(&parser);
        err = jsnn_parse(&parser, msg, tokens, JSNN_TOKEN_COUNT);
        if (err) {
            printf("error in json parse\n");
            return -1;
        }

        queue = pss->queue;

        l = atof(msg + jsnn_get(tokens, "l", msg, tokens)->start);
        r = atof(msg + jsnn_get(tokens, "r", msg, tokens)->start);
        t = atof(msg + jsnn_get(tokens, "t", msg, tokens)->start);
        b = atof(msg + jsnn_get(tokens, "b", msg, tokens)->start);
        nx = atoi(msg + jsnn_get(tokens, "nx", msg, tokens)->start);
        ny = atoi(msg + jsnn_get(tokens, "ny", msg, tokens)->start);

        px = nx / CHUNK_DIM + 1;
        py = ny / CHUNK_DIM + 1;

        dx = (r - l) / (double)(nx - 1);
        dy = (t - b) / (double)(ny - 1);

        chw = dx * (CHUNK_DIM - 1);
        chh = dy * (CHUNK_DIM - 1);

        if (px * py > QUEUE_SIZE)
            pss->err = -1;
        else {
            ii = 0;
            for (i = 0; i < px; i++)
                for (j = 0; j < py; j++)
                    gf_init_grid_bounds(&queue[ii++],
                        l + i * chw,
                        l + (i + 1) * chw,
                        t - (j + 1) * chh,
                        t - j * chh, CHUNK_DIM, CHUNK_DIM);
            printf("requesting %d %dx%d chunks.\n", ii, CHUNK_DIM, CHUNK_DIM);
            pss->count = ii;
            pss->datatype = SHADE;
            pss->send = 0;
        }
    
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

        //p = hdrp;

        /* Write success header */
        sprintf((char *)p, "%*s", SEND_STATUS_SIZE, "SUCCESS");
        p += SEND_STATUS_SIZE;

        /* Get grid to send. */
        grid = &pss->queue[pss->send++];
        memcpy(p, (void *)&grid->left, SEND_BOUNDS_SIZE);
        p += SEND_BOUNDS_SIZE;

        memcpy(p, (void *)&grid->nx, SEND_DIMS_SIZE);
        p += SEND_DIMS_SIZE;


        switch (pss->datatype) {
        case SHADE:
            gf_biquadratic(&gf, grid, nsun, //(void *)pss->n_sun,
                &gf_relief_shade_kernel, &gf_set_byte_zero, (void *)p);
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

        /* Rewind p to beginning of our header. */
        p -= SEND_HEADER_SIZE;

        /* Bytes we should write. */
        n = SEND_HEADER_SIZE + px_size * grid->nx * grid->ny;

		m = libwebsocket_write(wsi, p, n, LWS_WRITE_BINARY);
        printf("data: write %d bytes. wrote %d bytes.\n", n, m);
		if (m < n) {
			lwsl_err("ERROR %d writing data to di socket\n", n);
			return -1;
		}

        if (pss->send < pss->count)
            libwebsocket_callback_on_writable(context, wsi);

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
        SEND_HEADER_SIZE + CHUNK_SIZE,
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
