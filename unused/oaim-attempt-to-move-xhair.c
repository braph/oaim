#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>
#define case break;case

#define USAGE \
  "Usage: [-f FPS] [-WG] [-X crosshair] [-C color]\n" \
  " -f <FPS>\tSet FPS\n" \
  " -G\t\tShoot on green\n" \
  " -W\t\tShoot on white\n" \
  " -C <COLOR>\tShoot on custom color\n"\
  "\t\t(Format: RedMin-RedMax,GreenMin-GreenMax,BlueMin-BlueMax e.g. 0-70,130-256,0-90)\n"\
  " -X <NUM>\tSelect crosshair\n"

#define L 2,
#define R 4,
#define U 8,
#define D 16,

#define RECT_WIDTH  10
#define RECT_HEIGHT 10
#define NUM_CROSSHAIRS 4
/* Shape of the area that should be searched for the enemy.
 * 'x' represents a crosshair pixel (and will not be used in the search). */
const uint8_t crosshair_areas[NUM_CROSSHAIRS][RECT_HEIGHT][RECT_WIDTH] = {
#define _ 1,
#define x 0,
  {
    { L L U U U U U U R R },
    { L L x x _ _ x x R R },
    { L L L _ x x _ R R R },
    { L L _ x x x x _ R R },
    { L L x x x x x x _ R },
    { L L x x x x x x _ R },
    { L L _ x x x x _ R R },
    { L L R _ x x _ R R R },
    { L L x x _ _ x x R R },
    { L L D D D D D D R R },
  }, {
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ _ x x x x _ _ _ },
    { _ _ x x x x x x _ _ },
    { _ _ x x x x x x _ _ },
    { _ _ _ x x x x _ _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
  }, {
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ x x x x x x _ _ },
    { _ _ x x x x x x _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ _ _ x x _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
  }, {
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
    { _ _ _ _ _ _ _ _ _ _ },
  }
#undef x
#undef _
};

#undef L
#undef R
#undef U
#undef D

#define L 2
#define R 4
#define U 8
#define D 16

struct color_range {
  uint16_t red[2], green[2], blue[2];
};
static const struct color_range RANGE_WHITE = {
  .red   = { 235, 255 },
  .green = { 235, 255 },
  .blue  = { 235, 255 },
};
static const struct color_range RANGE_GREEN = {
  .red   = { 0,   70  },
  .green = { 130, 255 },
  .blue  = { 0,   90  },
};

#define XColor_In_Range(X_COLOR , COLOR_RANGE) (\
    (X_COLOR.red   >= COLOR_RANGE.red[0])   && \
    (X_COLOR.red   <= COLOR_RANGE.red[1])   && \
    (X_COLOR.green >= COLOR_RANGE.green[0]) && \
    (X_COLOR.green <= COLOR_RANGE.green[1]) && \
    (X_COLOR.blue  >= COLOR_RANGE.blue[0])  && \
    (X_COLOR.blue  <= COLOR_RANGE.blue[1])     )

static inline void printCrosshair(char *ch) {
  for (unsigned xy = 0; xy < RECT_HEIGHT * RECT_WIDTH; ++xy)
    printf("%c%c", ch[xy] ? '_' : 'x', (1+xy) % RECT_WIDTH ? ' ' : '\n');
}

char *windowtitle(Display *disp, Window window) {
  Atom prop = XInternAtom(disp, "WM_CLASS", False);
  Atom type;
  int form;
  unsigned long remain, len;
  unsigned char *list;
  XGetWindowProperty(disp, window, prop, 0, 1024, False, AnyPropertyType,
      &type, &form, &len, &remain, &list);
  return (char*)list;
}

Window *list(Display *disp, unsigned long *len) {
  Atom prop = XInternAtom(disp, "_NET_CLIENT_LIST", False);
  Atom type;
  int form;
  unsigned long remain;
  unsigned char *list;
  XGetWindowProperty(disp, XDefaultRootWindow(disp), prop, 0, 1024, False,
      XA_WINDOW, &type, &form, len, &remain, &list);
  return (Window*)list;
}

unsigned die;
void sighandler(int _) { die = 1; }

int main(int argc, char *argv[]) {
  unsigned int usecs;
  struct color_range *color_ranges = NULL;
  unsigned int color_ranges_count = 0;
  union {
    uint8_t d2[RECT_HEIGHT] [RECT_WIDTH];
    uint8_t d1[RECT_HEIGHT * RECT_WIDTH];
  } crosshair_area;

COMMAND_LINE_OPTIONS:
  {
    int o, fps = 60, chnum = 0;
    struct color_range cr;

    while ((o = getopt(argc, argv, "hf:X:C:WG")) != -1)
      switch (o) {
        default:
          errx(1, USAGE);
        case 'f':
          if (! (fps = atoi(optarg)))
            errx(1, "Invalid value for FPS: %s", optarg);
        case 'X':
          if (! (chnum = atoi(optarg)) || --chnum >= NUM_CROSSHAIRS)
            errx(1, "Invalid value for crosshair: %s", optarg);
        case 'W':
          cr = RANGE_WHITE;
          printf("- Shooting on white color\n");
          goto ADD_COLOR_RANGE;
        case 'G':
          cr = RANGE_GREEN;
          printf("- Shooting on green color\n");
          goto ADD_COLOR_RANGE;
        case 'C':
          if (6 != sscanf(optarg, "%u-%u,%u-%u,%u-%u", &cr.red[0], &cr.red[1],
                &cr.green[0], &cr.green[1], &cr.blue[0], &cr.blue[1]))
            errx(1, "Invalid custom color format: %s", optarg);
          printf("- Shooting on custom color: %s\n", optarg);
ADD_COLOR_RANGE:
          for (unsigned i = 0; i <= 1; ++i) {
            if (cr.red[i] > 255) errx(1, "Invalid value for red: %u", cr.red[i]);
            if (cr.blue[i] > 255) errx(1, "Invalid value for blue: %u", cr.blue[i]);
            if (cr.green[i] > 255) errx(1, "Invalid value for green: %u", cr.green[i]);
            cr.red[i] *= 256;
            cr.blue[i] *= 256;
            cr.green[i] *= 256;
          }
          color_ranges = realloc(color_ranges, ++color_ranges_count);
          color_ranges[color_ranges_count-1] = cr;
          break;
      }

    usecs = 1000*1000 / fps - 100;
    memcpy(crosshair_area.d1, crosshair_areas[chnum], sizeof(crosshair_area.d1));
    printf("- Using %u FPS\n", fps);
    printf("- Using crosshair %d:\n", chnum+1);
    printCrosshair(crosshair_area.d1);
  }

  Display *disp;
  Window root;
  Window game;
  Colormap cm;
  int cross_x, cross_y;

INIT:
  {
    if (! (disp = XOpenDisplay(NULL)))
      errx(1, "Can't open X display");

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    Screen *scr = ScreenOfDisplay(disp, DefaultScreen(disp));
    Visual *vis = DefaultVisual(disp, XScreenNumberOfScreen(scr));
    root = RootWindow(disp, XScreenNumberOfScreen(scr));
    cm = XDefaultColormap(disp, DefaultScreen(disp));
    XWindowAttributes ra;
    XGetWindowAttributes(disp, root, &ra);
    cross_x = ra.width / 2 - RECT_WIDTH / 2;
    cross_y = ra.height / 2 - RECT_HEIGHT / 2;

    Window root_return;
    Window *children;
    unsigned long nchildren;
    children = list(disp, &nchildren);
    for (unsigned int i = 0; i < nchildren; ++i) {
    //while (nchildren >= 1) {
      //if (*children == 10485777)
      //  ++*children;
      char *title = windowtitle(disp, children[i]);
      if (strcasestr(title, "openarena")) {
        game = children[i];
        printf("FOUND\n");
      }
      printf("%u %s lol\n", children[i], windowtitle(disp, children[i]));
      //++children;
      //--nchildren;
    }
  }

  XImage *image;
  union {
    XColor d2[RECT_HEIGHT] [RECT_WIDTH];
    XColor d1[RECT_HEIGHT * RECT_WIDTH];
  } c;

  while (!die) {
    usleep(usecs);
    image = XGetImage(disp, root, cross_x, cross_y, RECT_WIDTH, RECT_HEIGHT, AllPlanes, XYPixmap);

    if (1 /* image */) {
      for (unsigned x = 0; x < RECT_WIDTH; ++x)
        for (unsigned y = 0; y < RECT_HEIGHT; ++y)
          c.d2[y][x].pixel = XGetPixel(image, x, y);
      XQueryColors(disp, cm, c.d1, RECT_WIDTH*RECT_HEIGHT);

      for (unsigned i = 0; i < color_ranges_count; ++i)
        for (unsigned xy = 0; xy < RECT_WIDTH*RECT_HEIGHT; ++xy)
          if (crosshair_area.d1[xy])
            if (XColor_In_Range(c.d1[xy], color_ranges[i])) {
              if (crosshair_area.d1[xy] == 1) {
                //XTestFakeButtonEvent(disp, 1, True, CurrentTime);
                //XTestFakeButtonEvent(disp, 1, False, CurrentTime);
                XFlush(disp);
              }
              else {
                Window root_return;
                Window child_return;
                int rx, ry, wx, wy;
                unsigned int mask;
                XQueryPointer(disp, game, &root_return, &child_return,
                    &rx, &ry, &wx, &wy, &mask);
                printf("%d %d %d %d\n", rx, ry, wx,wy);

                XWarpPointer(disp, game, game, rx,ry,1600,900,//cross_x, cross_y, 1600, 900,
                    cross_x + (crosshair_area.d1[xy] & L ? -150 :
                    (crosshair_area.d1[xy] & R ? 150 :
                    0)),
                    cross_y + (crosshair_area.d1[xy] & D ? -150 :
                    (crosshair_area.d1[xy] & U ? 150 :
                    0)));
                XFlush(disp);
              }
              goto BREAK;
            }

BREAK:
      XFree(image);
    }
  }

  return XCloseDisplay(disp);
}
