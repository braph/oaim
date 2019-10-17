#define _GNU_SOURCE /* strcasestr */
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

#define RECT_WIDTH  10
#define RECT_HEIGHT 10
#define NUM_CROSSHAIRS 4
/* Shape of the area that should be searched for the enemy.
 * 'x' represents a crosshair pixel (and will not be used in the search). */
const uint8_t crosshair_areas[NUM_CROSSHAIRS][RECT_HEIGHT][RECT_WIDTH] = {
#define _ 1,
#define x 0,
  {
    { x x x x x x x x x x },
    { x x x x _ _ x x x x },
    { x x x _ x x _ x x x },
    { x x _ x x x x _ x x },
    { x _ x x x x x x _ x },
    { x _ x x x x x x _ x },
    { x x _ x x x x _ x x },
    { x x x _ x x _ x x x },
    { x x x x _ _ x x x x },
    { x x x x x x x x x x },
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

static const struct color_range {
  uint16_t red[2], green[2], blue[2];
}
RANGE_WHITE = {
  .red   = { 235, 255 },
  .green = { 235, 255 },
  .blue  = { 235, 255 },
},
RANGE_GREEN = {
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
    printf("%c%c", ch[xy] ? '_' : 'x', ((1+xy) % RECT_WIDTH) ? ' ' : '\n');
}

static char *get_window_title(Display*, Window);
static Window *get_window_list(Display*, unsigned long*);

static unsigned int die;
static void sighandler(int _) { die = 1; }

int main(int argc, char *argv[]) {
  unsigned int usecs;
  struct color_range *color_ranges = NULL;
  unsigned int color_ranges_count = 0;
  union {
    uint8_t d2[RECT_HEIGHT] [RECT_WIDTH];
    uint8_t d1[RECT_HEIGHT * RECT_WIDTH];
  } crosshair_area;

/*COMMAND_LINE_OPTIONS*/
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
          if (6 != sscanf(optarg, "%hu-%hu,%hu-%hu,%hu-%hu", &cr.red[0], &cr.red[1],
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
      }

    usecs = 1000*1000 / fps - 100;
    memcpy(crosshair_area.d1, crosshair_areas[chnum], sizeof(crosshair_area.d1));
    printf("- Using %d FPS\n", fps);
    printf("- Using crosshair %d:\n", chnum+1);
    printCrosshair(crosshair_area.d1);
  }

  Display *disp;
  Window root, game;
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
    root = game = RootWindow(disp, XScreenNumberOfScreen(scr));
    cm = XDefaultColormap(disp, DefaultScreen(disp));

    unsigned long nchildren;
    Window *children = get_window_list(disp, &nchildren);
    while (nchildren-- >= 1) {
      char *title = get_window_title(disp, *children);
      if (strcasestr(title, "openarena")) {
        game = *children;
        break;
      }
      ++children;
    }

    if (game == root)
      warn("No game window found, falling back on root window");

    XWindowAttributes ra;
    XGetWindowAttributes(disp, game, &ra);
    cross_x = ra.width / 2 - RECT_WIDTH / 2;
    cross_y = ra.height / 2 - RECT_HEIGHT / 2;
  }

/*MAINLOOP*/
  XImage *image;
  union {
    XColor d2[RECT_HEIGHT] [RECT_WIDTH];
    XColor d1[RECT_HEIGHT * RECT_WIDTH];
  } c;

  while (!die) {
    usleep(usecs);
    image = XGetImage(disp, game, cross_x, cross_y, RECT_WIDTH, RECT_HEIGHT, AllPlanes, XYPixmap);

    if (image) {
      for (unsigned x = 0; x < RECT_WIDTH; ++x)
        for (unsigned y = 0; y < RECT_HEIGHT; ++y)
          c.d2[y][x].pixel = XGetPixel(image, x, y);
      XQueryColors(disp, cm, c.d1, RECT_WIDTH*RECT_HEIGHT);

      for (unsigned i = 0; i < color_ranges_count; ++i)
        for (unsigned xy = 0; xy < RECT_WIDTH*RECT_HEIGHT; ++xy)
          if (crosshair_area.d1[xy])
            if (XColor_In_Range(c.d1[xy], color_ranges[i])) {
              XTestFakeButtonEvent(disp, 1, True, CurrentTime);
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

static Window *get_window_list(Display *disp, unsigned long *len) {
  Atom type, prop = XInternAtom(disp, "_NET_CLIENT_LIST", False);
  int form;
  unsigned long remain;
  unsigned char *list;
  XGetWindowProperty(disp, XDefaultRootWindow(disp), prop, 0, 1024, False,
      XA_WINDOW, &type, &form, len, &remain, &list);
  return (Window*)list;
}

static char *get_window_title(Display *disp, Window window) {
  Atom type, prop = XInternAtom(disp, "WM_CLASS", False);
  int form;
  unsigned long remain, len;
  unsigned char *list;
  XGetWindowProperty(disp, window, prop, 0, 1024, False, AnyPropertyType,
      &type, &form, &len, &remain, &list);
  return (char*)list;
}

