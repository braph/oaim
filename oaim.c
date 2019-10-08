#include <err.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#define FPS 60

#define _ 1,
#define x 0,
/* Shape of the area that should be searched for the enemy.
 * 'x' represents a crosshair pixel (and will not be used in the search). */
char crosshair_area[10][10] = {
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
};
#undef x
#undef _

#define RECT_WIDTH  (sizeof(crosshair_area[0]))
#define RECT_HEIGHT (sizeof(crosshair_area)/sizeof(crosshair_area[0]))

int quit;
void sighandler(int _) { quit = 1; }

int main() {
  Display *disp;
  Window root;
  Colormap cm;
  int cross_x, cross_y;

  if (! (disp = XOpenDisplay(NULL)))
    errx(1, "Can't open X display");

  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);

  INIT: {
    XWindowAttributes ra;
    Screen *scr = ScreenOfDisplay(disp, DefaultScreen(disp));
    Visual *vis = DefaultVisual(disp, XScreenNumberOfScreen(scr));
    root = RootWindow(disp, XScreenNumberOfScreen(scr));
    cm = XDefaultColormap(disp, DefaultScreen(disp));
    XGetWindowAttributes(disp, root, &ra);
    cross_x = ra.width / 2 - RECT_WIDTH / 2;
    cross_y = ra.height / 2 - RECT_HEIGHT / 2;
  }

  do {
    XImage* image = XGetImage(disp, root, cross_x, cross_y, RECT_WIDTH, RECT_HEIGHT, AllPlanes, XYPixmap);
    XColor c;

    if (image) {
      for (unsigned x = 0; x < RECT_WIDTH; ++x)
        for (unsigned y = 0; y < RECT_HEIGHT; ++y) {
          if (crosshair_area[y][x]) {
            c.pixel = XGetPixel(image, x, y);
            XQueryColor(disp, cm, &c);
            if (c.green/256 >= 130 && c.red/256 < 70 && c.blue/256 < 90) {
              XTestFakeButtonEvent(disp, 1, True, CurrentTime);
              XTestFakeButtonEvent(disp, 1, False, CurrentTime);
              XFlush(disp);
              x = RECT_WIDTH;
              break;
            }
          }
        }
      XFree(image);
    }

    usleep(1000*1000 / FPS - 100);
  } while (!quit);

  return XCloseDisplay(disp);
}
