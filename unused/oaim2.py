#!/usr/bin/python3
C = r"""#line 3
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

struct color_range {
  uint16_t red[2], green[2], blue[2];
};

#if $SHOOT_ON_WHITE
static const struct color_range RANGE_WHITE = $RANGE_WHITE_INIT;
#endif

#if $SHOOT_ON_GREEN
static const struct color_range RANGE_GREEN = $RANGE_GREEN_INIT;
#endif
foo;
#define XColor_In_Range(X_COLOR , COLOR_RANGE) (\
    (X_COLOR.red   >= COLOR_RANGE.red[0])   && \
    (X_COLOR.red   <= COLOR_RANGE.red[1])   && \
    (X_COLOR.green >= COLOR_RANGE.green[0]) && \
    (X_COLOR.green <= COLOR_RANGE.green[1]) && \
    (X_COLOR.blue  >= COLOR_RANGE.blue[0])  && \
    (X_COLOR.blue  <= COLOR_RANGE.blue[1])     )

static unsigned int die;
static void sighandler(int _) { die = 1; }

int main(int argc, char *argv[]) {
  union {
    uint8_t d2[$CH_HEIGHT] [$CH_WIDTH];
    uint8_t d1[$CH_HEIGHT * $CH_WIDTH];
  } crosshair_area = { .d2 = $CROSSHAIR };

  Display *disp;
  Window game;
  Colormap cm;
  int cross_x, cross_y;

/*INIT*/
  {
    if (! (disp = XOpenDisplay(NULL)))
      errx(1, "Can't open X display");

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    Screen *scr = ScreenOfDisplay(disp, DefaultScreen(disp));
    Visual *vis = DefaultVisual(disp, XScreenNumberOfScreen(scr));
    cm = XDefaultColormap(disp, DefaultScreen(disp));

#if $GAME_WINDOW_ID
    game = $GAME_WINDOW_ID;
#else
    game = RootWindow(disp, XScreenNumberOfScreen(scr));
#endif

    XWindowAttributes ra;
    XGetWindowAttributes(disp, game, &ra);
    cross_x = ra.width / 2 - $CH_WIDTH / 2;
    cross_y = ra.height / 2 - $CH_HEIGHT / 2;
  }

/*MAINLOOP*/
  XImage *image;
  union {
    XColor d2[$CH_HEIGHT] [$CH_WIDTH];
    XColor d1[$CH_HEIGHT * $CH_WIDTH];
  } c;

  while (!die) {
    usleep(1000*1000 / $FPS - 100);
    image = XGetImage(disp, game, cross_x, cross_y, $CH_WIDTH, $CH_HEIGHT, AllPlanes, XYPixmap);

    if (image) {
      for (unsigned x = 0; x < $CH_WIDTH; ++x)
        for (unsigned y = 0; y < $CH_HEIGHT; ++y)
          c.d2[y][x].pixel = XGetPixel(image, x, y);
      XQueryColors(disp, cm, c.d1, $CH_WIDTH*$CH_HEIGHT);

      for (unsigned xy = 0; xy < $CH_WIDTH*$CH_HEIGHT; ++xy)
        if (crosshair_area.d1[xy])
          if (0
#if $SHOOT_ON_WHITE
              || XColor_In_Range(c.d1[xy], RANGE_WHITE)
#endif
#if $SHOOT_ON_GREEN
              || XColor_In_Range(c.d1[xy], RANGE_GREEN)
#endif
             ) {
#if $SHOOT_DELAY_MIN
			usleep($SHOOT_DELAY_MIN * 1000);
#endif
#if $SHOOT_DELAY_MAX
            usleep((rand() * 1000) % $SHOOT_DELAY_MAX - $SHOOT_DELAY_MIN);
#endif
            XTestFakeButtonEvent(disp, 1, True, CurrentTime);
#if $SHOOT_TIME
            XFlush(disp);
            usleep(SHOOT_TIME * 1000);
#endif
            XTestFakeButtonEvent(disp, 1, False, CurrentTime);
            XFlush(disp);
            goto BREAK;
          }
BREAK:
      XFree(image);
    }
  }

  return XCloseDisplay(disp);
}

"""
#if 0 /* For viewing with :set filetype=C */

# =============================================================================
# P Y T H O N  C O D E
# =============================================================================

import sys, os, re
from argparse import ArgumentParser
from subprocess import Popen, PIPE
from string import Template

class ColorRange:
    def __init__(self, red, green, blue):
        for i in (0,1):
            if red[i]   > 255: raise Exception("Invalid color for red")
            if green[i] > 255: raise Exception("Invalid color for green")
            if blue[i]  > 255: raise Exception("Invalid color for blue")
        self.red, self.green, self.blue = red, green, blue

    def __str__(self):
        return '{ .red = { %d, %d }, .green = { %d, %d }, .blue  = { %d, %d } }' % (
            self.red[0]   * 256, self.red[1]   * 256,
            self.green[0] * 256, self.green[1] * 256,
            self.blue[0]  * 256, self.blue[1]  * 256
        )

class CrossHair:
    def __init__(self, ch):
        self.ch = ch
        self.compressed = ch.strip()
        self.height = self.compressed.count('\n') + 1
        self.width = self.compressed.count("x") + self.compressed.count('_')
        self.width = int(self.width / self.height)
    def __str__(self):
        s = "{"
        for l in self.compressed.split('\n'):
            l = l.replace('x', '1,').replace('_', "0,")
            s += "{"+l+"},"
        return s + "}"

# Shape of the area that should be searched for the enemy.
# 'x' represents a crosshair pixel (and will not be used in the search). */
CROSSHAIRS = [
"""
     x x x _ _ x x x 
     x _ _ x x _ _ x 
     x _ x x x x _ x 
     _ x x x x x x _ 
     _ x x x x x x _ 
     x _ x x x x _ x 
     x _ _ x x _ _ x 
     x x x _ _ x x x 
""", """
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ x x _ _ _ _ 
    _ _ _ x x x x _ _ _ 
    _ _ x x x x x x _ _ 
    _ _ x x x x x x _ _ 
    _ _ _ x x x x _ _ _ 
    _ _ _ _ x x _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
""", """
    _ _ _ _ _ _ _ _ _ _
    _ _ _ _ _ _ _ _ _ _
    _ _ _ _ x x _ _ _ _
    _ _ _ _ x x _ _ _ _
    _ _ x x x x x x _ _
    _ _ x x x x x x _ _
    _ _ _ _ x x _ _ _ _
    _ _ _ _ x x _ _ _ _
    _ _ _ _ _ _ _ _ _ _
    _ _ _ _ _ _ _ _ _ _
""","""
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
    _ _ _ _ _ _ _ _ _ _ 
"""
]

color_ranges = {
    'white': ColorRange(red=(235,255), green=(235,255), blue=(235,255)),
    'green': ColorRange(red=(0,70), green=(130,255), blue=(0,90))
}

argp = ArgumentParser()
argp.add_argument("-f", "--fps", type=int, dest="FPS", help="Set FPS")
argp.add_argument("-s", "--shoot-time", type=int, dest="SHOOT_TIME", help="Set shoot duration in milli seconds")
argp.add_argument("-X", "--crosshair", choices=(1,2,3), type=int, dest="CROSSHAIR_NUM", help="Select crosshair")
argp.add_argument("-G", "--shoot-on-green", default=0, action="store_const", const=1, dest="SHOOT_ON_GREEN", help="Shoot on green color")
argp.add_argument("-W", "--shoot-on-white", default=0, action="store_const", const=1, dest="SHOOT_ON_WHITE", help="Shoot on white color")
argp.set_defaults(
    RANGE_WHITE_INIT=str(color_ranges["white"]),
    RANGE_GREEN_INIT=str(color_ranges["green"]),
    CROSSHAIR_NUM=1,
    CROSSHAIR='',
    FPS=60,
    SHOOT_TIME=0,
    GAME_WINDOW_ID=0,
    SHOOT_DELAY_MIN=0,
    SHOOT_DELAY_MAX=0,
    CH_HEIGHT=0,
    CH_WIDTH=0)
args = argp.parse_args()
ch = CrossHair(CROSSHAIRS[args.CROSSHAIR_NUM-1])
args.CROSSHAIR = str(ch)
args.CH_HEIGHT = ch.height
args.CH_WIDTH = ch.width

p = Popen(["xwininfo", "-root", "-all"], stdout=PIPE, encoding='UTF-8')
for line in p.stdout:
    line_lower = line.lower()
    if "openarena" in line_lower or "yuoa." in line_lower:
        print(line_lower)
        args.GAME_WINDOW_ID = re.search("0x[0-9a-fA-F]+", line_lower)[0]
        break
if args.GAME_WINDOW_ID == 0:
    print("Warning: Falling back on root window")

C = Template(C).substitute(args.__dict__)

p = Popen(['gcc', '-lX11', '-lXtst', '-Ofast', '-x', 'c', '-', '-o', 'oaim'],
        stdin=PIPE, encoding='UTF-8')
p.communicate(C)

#if not ret:
#    os.execv('oaim', ['oaim'])

#endif /* For viewing with :set filetype=C */
