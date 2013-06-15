
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>

#include <xdo.h>

#include <stdlib.h>
#include <sys/time.h>

#define fail(x) do { printf("!! %s\n", x); exit(1); } while (0)
#define fail_if(x) if (x) fail(#x)

long
ticks()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

typedef struct {
    int x;
    int y;
} pos;

struct {
    int tap_threshold;
    int tap_delay;
    int rotated;
    float scale_x;
    float scale_y;

    int sock;
    xdo_t *xd;
    pos press;
    pos cur;

    int pressed;
    int dragging;
    int taps;
    long released;
} ctx;

void
handle(char *op, pos p)
{
    pos d;

    switch (op[0]) {
        case 'p':
            ctx.press = ctx.cur = p;
            ctx.pressed = 1;
            break;
        case 'm':
            d.x = (p.x - ctx.cur.x) * ctx.scale_x;
            d.y = (p.y - ctx.cur.y) * ctx.scale_y;
            xdo_mousemove_relative(ctx.xd, d.x, d.y);
            ctx.cur = p;
            break;
        case 'r':
            if (ctx.dragging) {
                xdo_mouseup(ctx.xd, CURRENTWINDOW, 1);
                ctx.dragging = 0;
            }

            ctx.pressed = 0;
            d.x = p.x - ctx.press.x;
            d.y = p.y - ctx.press.y;
            if ((d.x*d.x + d.y*d.y) <
                    (ctx.tap_threshold*ctx.tap_threshold)) {
                ctx.taps++;
                ctx.released = ticks();
            }
            break;
        default:
            fail("Unknown event");
            break;
    }
}

void
idle()
{
    if (ctx.taps == 0) {
        return;
    }

    long now = ticks();

    if ((now - ctx.released) > ctx.tap_delay) {
        if (ctx.pressed) {
            ctx.dragging = 1;
            xdo_mousedown(ctx.xd, CURRENTWINDOW, 1);
        } else {
            for (int i=0; i<ctx.taps; i++) {
                xdo_mousedown(ctx.xd, CURRENTWINDOW, 1);
                xdo_mouseup(ctx.xd, CURRENTWINDOW, 1);
            }
        }

        ctx.taps = 0;
    }
}

void
process()
{
    char buf[64];

    // input buffer from n9mouse.py:
    // "<action>,<x>,<y>" where:
    //  <action>: "press", "move" or "release"
    //  <x>, <y>: integer number (pixel coordinates)
    ssize_t size = recv(ctx.sock, buf, sizeof(buf), 0);
    fail_if(size < 0);
    buf[size] = '\0';
    char *s = strchr(buf, ',');
    fail_if(s == NULL);
    *s = '\0';

    pos p;
    p.x = atoi(s+1);
    s = strchr(s+1, ',');
    fail_if(s == NULL);
    p.y = atoi(s+1);

    if (ctx.rotated) {
        int tmp = p.x;
        p.x = -p.y;
        p.y = tmp;
    }

    handle(buf, p);
}

int
main(int argc, char *argv[])
{
    ctx.tap_threshold = 3; // pixel threshold between clicking and just moving
    ctx.tap_delay = 100; // delay before taps are processed (for tap'n'drag)
    ctx.rotated = 1; // 1 = n9 in portrait mode, 0 = n9 in landscape mode
    ctx.scale_x = 1.8; // scaling factor for x axis
    ctx.scale_y = 1.8; // scaling factor for y axis

    ctx.xd = xdo_new(NULL);
    fail_if(ctx.xd == NULL);
    ctx.sock = socket(AF_INET, SOCK_DGRAM, 0);
    fail_if(ctx.sock == 0);

    ctx.pressed = 0;
    ctx.dragging = 0;
    ctx.taps = 0;
    ctx.released = ticks();

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(9977);

    int res = bind(ctx.sock, (struct sockaddr *)&sin, sizeof(sin));
    fail_if(res != 0);

    struct pollfd pfd;
    pfd.fd = ctx.sock;
    pfd.events = POLLIN | POLLPRI;
    while (1) {
        pfd.revents = 0;
        if (poll(&pfd, 1, ctx.tap_delay) == 1) {
            process();
        } else {
            idle();
        }
    }

    close(ctx.sock);
    xdo_free(ctx.xd);

    return 0;
}

