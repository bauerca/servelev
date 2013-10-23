'use strict';

function extend(obj, attrs) {
    var attr, res = {};
    for (attr in obj)
        res[attr] = obj[attr];
    for (attr in attrs)
        res[attr] = attrs[attr];
    return res;
}

/**
 * On scroll:
 *   1. Shift pixels that remain visible.
 *   2. Request data for new pixels.
 *   3. Register draw callback for when requested pixels are ready.
 *
 * Bounds
 * ------
 * Bounds always refer to the area covered by a grid of points.
 * Image data deals with a grid of pixels. So what do we mean when
 * we attribute lat/lng bounds to an image? A pixel shows a color
 * based on the data at its midpoint. Thus, the real-data bounds
 * run from 
 * So, when we deal with image data (i.e. pixels), the bounds of
 * the data shown by the image has its top-right
 */
function MapView() {
    this.init.apply(this, arguments);
}

MapView.prototype = {
    constructor: MapView,

    init: function(canvas, scale) {
        console.log('MapView constructor called.');

        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');

        /* Multiply canvas w/h in pixels by this to
           get canvas w/h in degrees lng/lat, respectively. */
        this.scale = scale || 0.001; 
        this.mouseDown = false;
        this.bounds = {};

        var attr, m, f;
        for (attr in this)
            if (typeof(f = this[attr]) === 'function' && (m = attr.match(/^ev(.+)$/)))
                this.on(m[1].toLowerCase(), f);

        var ws = this.ws = new WebSocket('ws://localhost:7631', 'super-dumb-proto');
        ws.binaryType = 'arraybuffer';
        ws.onmessage = this.onMessage.bind(this);
    },

    on: function(name, handler) {
        var me = this;
        $(this.canvas).on(name, function(ev) {
            return handler.call(me, ev);
        });
    },

    /**
     * Receive a data chunk from the server via the
     * websocket. If we've received data, we know we don't
     * already have it cached, so just draw it if it
     * intersects our view bounds.
     
     Check if bounds of received data overlap
     * any dirty regions on canvas. If so, draw chunk
     * into canvas.
     */
    onMessage: function(msg) {
        var dBnds,
            b,
            iBnds,
            data = msg.data,
            vBnds = this.bounds,
            ptr = 0,
            hdr, hdrSz = 16, /* 16 chars */
            bnds, bndsSz = 8 * 4, /* 4 doubles */
            dims, nx, ny, dimsSz = 4 * 2; /* 2 ints */

        /* Get status message */
        hdr = String.fromCharCode.apply(null, new Uint8Array(data, ptr, hdrSz)).trim();
        //console.log(hdr);
        ptr += hdrSz;

        if (hdr === 'SUCCESS') {
            bnds = new Float64Array(data, ptr, 4);
            bnds = {l: bnds[0], r: bnds[1], b: bnds[2], t: bnds[3]};
            //console.log(bnds);
            ptr += bndsSz;

            dims = new Int32Array(data, ptr, 2);
            //console.log(dims);
            nx = dims[0];
            ny = dims[1];
            ptr += dimsSz;

            data = new Uint8Array(data, ptr, nx * ny);
            //console.log(data.length);

            this.draw(data, nx, ny, bnds, function(imgData, i, val) {
                var v = Math.round(val),
                    d = imgData.data;
                d[i] = v;
                d[i + 1] = v;
                d[i + 2] = v;
                d[i + 3] = 255;
            });
        }
        /* If cache full, remove the data chunk that is
           farthest from current view */
    },

    intersect: function(b1, b2) {
        if (
            (b1.l < b2.r && b1.r > b2.l) &&
            (b1.b < b2.t && b1.t > b2.b)
        )
            return {
                l: b1.l > b2.l ? b1.l : b2.l,
                r: b1.r < b2.r ? b1.r : b2.r,
                b: b1.b > b2.b ? b1.b : b2.b,
                t: b1.t < b2.t ? b1.t : b2.t
            }
        return false;
    },

    evMouseDown: function(ev) {
        //console.log('mousedown');
        this.mouseDown = true;
        this.mouseX = ev.pageX;
        this.mouseY = ev.pageY;
    },

    evMouseMove: function(ev) {
        //console.log('mousemove');
        if (this.mouseDown) {
            this.doPan(ev.pageX - this.mouseX, ev.pageY - this.mouseY);
            this.mouseX = ev.pageX;
            this.mouseY = ev.pageY;
        }
    },

    evMouseUp: function(ev) {
        //console.log('mouseup');
        this.mouseDown = false;
    },

    /**
     * Communicate to server our new view state.
     */
    broadcast: function() {
        var c = this.canvas,
            s = this.scale,
            b = this.bounds;
        b.l = this.x;
        b.r = this.x + (c.width - 1) * s;
        b.b = this.y - (c.height - 1) * s;
        b.t = this.y;
        this.ws.send(JSON.stringify(extend(b, {nx: c.width, ny: c.height})));
    },

    /**
     * Shifts.
     * @param shiftX (pixels) Distance to move current image in x-direction
     * @param shiftY (pixels) Distance to move current image in y-direction
     */
    doPan: function(shiftX, shiftY) {
        var magX = Math.abs(shiftX),
            magY = Math.abs(shiftY),
            w = this.canvas.width,
            h = this.canvas.height,
            imgData;


        /* Get pixels that will remain visible */
        if (false && magX < w && magY < h) {
            imgData = this.ctx.getImageData(
                shiftX > 0 ? 0 : -shiftX,
                shiftY > 0 ? 0 : -shiftY,
                w - magX,
                h - magY);

            this.ctx.clearRect(0, 0, w, h);

            this.ctx.putImageData(imgData, 
                shiftX > 0 ? shiftX : 0,
                shiftY > 0 ? shiftY : 0);
        } else {
            /* Entire canvas is dirty. */
            
        }

        this.x -= this.scale * shiftX;
        this.y += this.scale * shiftY;
        this.broadcast();
    },

    /**
     * Draw the given data with dimensions nwxnh onto the canvas
     * in the given bnds.
     */
    draw: function(data, nxData, nyData, bndsData, setter) {
        var imgData,
            ixStartImg,
            iyStartImg,
            ixEndImg,
            iyEndImg,
            xImg,
            yImg,
            ixImg,
            iyImg,
            dxImg = this.scale,
            dyImg = dxImg,
            nxImg, nyImg,
            wy,
            wx,
            c = this.canvas,
            ixData, iyData,
            v,
            dxData = (bndsData.r - bndsData.l) / (nxData - 1),
            dyData = (bndsData.t - bndsData.b) / (nyData - 1);
        
        /* Find intersection rect */

        /* Get pixels to fill from intersection rect */
        ixStartImg = Math.ceil((bndsData.l - this.x) / dxImg);
        if (ixStartImg < 0) ixStartImg = 0;
        ixEndImg = Math.ceil((bndsData.r - this.x) / dxImg);
        if (ixEndImg > c.width) ixEndImg = c.width;

        if (ixStartImg === ixEndImg)
            /* Nothing to draw. */
            return false;
        
        /* Get pixels to fill from intersection rect */
        iyStartImg = Math.ceil((this.y - bndsData.t) / dyImg);
        if (iyStartImg < 0) iyStartImg = 0;
        iyEndImg = Math.ceil((this.y - bndsData.b) / dyImg);
        if (iyEndImg > c.height) iyEndImg = c.height;

        if (iyStartImg === iyEndImg)
            /* Nothing to draw. */
            return false;

        nxImg = ixEndImg - ixStartImg;
        nyImg = iyEndImg - iyStartImg;

        imgData = this.ctx.getImageData(ixStartImg, iyStartImg, nxImg, nyImg);

        /* Fill each pixel, interpolating from given data */
        yImg = this.y - iyStartImg * dyImg;
        for (iyImg = 0; iyImg < nyImg; iyImg++) {
            /* src top. */
            iyData = Math.floor((bndsData.t - yImg) / dyData);
            wy = (bndsData.t - yImg) / dyData - iyData;

            xImg = this.x + ixStartImg * dxImg;
            for (ixImg = 0; ixImg < nxImg; ixImg++) {
                /* src left. */
                ixData = Math.floor((xImg - bndsData.l) / dxData);
                wx = (xImg - bndsData.l) / dxData - ixData;

                v = (1 - wx) * (1 - wy) * data[ iyData      * nxData +  ixData     ] +
                    (1 - wx) *      wy  * data[(iyData + 1) * nxData +  ixData     ] +
                         wx  * (1 - wy) * data[ iyData      * nxData + (ixData + 1)] +
                         wx  *      wy  * data[(iyData + 1) * nxData + (ixData + 1)];

                setter(imgData, 4 * (iyImg * c.width + ixImg), v);
                //imgData.data[iyImg * c.width + ixImg] = v;
                xImg += dxImg;
            }
            yImg -= dyImg;
        }

        this.ctx.clearRect(ixStartImg, iyStartImg, nxImg, nyImg);
        this.ctx.putImageData(imgData, ixStartImg, iyStartImg);
        return true;
    },

    setCenter: function(n, w) {
        this.x = -w - 0.5 * this.canvas.width * this.scale;
        this.y = n + 0.5 * this.canvas.height * this.scale;
        this.broadcast();
    },

    setScale: function(s) {
        this.scale = s;
        this.broadcast();
    }

};



$(document).ready(function() {
    var map = new MapView($('canvas').get(0), 0.001);

    $('form').on('submit', function(ev) {
        var form = $(this).serializeArray(),
            n, w;
        $.each(form, function(index, input) {
            switch (input.name) {
            case 'north':
                n = parseFloat(input.value);
                break;
            case 'west':
                w = parseFloat(input.value);
                break;
            }
        });

        ev.preventDefault();
        map.setCenter(n || -map.midX(), w || map.midY());
    });

});
