#include <err.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#define FPS 60
#define RECT_SIZE 8

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
    XGetWindowAttributes(disp, DefaultRootWindow(disp), &ra);
    cross_x = ra.width / 2 - RECT_SIZE;
    cross_y = ra.height / 2 - RECT_SIZE;
  }

  do {
    XImage* image = XGetImage(disp, root, cross_x, cross_y, RECT_SIZE, RECT_SIZE, AllPlanes, XYPixmap);
    XColor c;

    if (image) {
      for (unsigned x = 0; x < RECT_SIZE; ++x)
        for (unsigned y = 0; y < RECT_SIZE; ++y) {
          c.pixel = XGetPixel(image, x, y);
          XQueryColor(disp, cm, &c);
          if (c.green/256 >= 130 && c.red/256 < 70 && c.blue/256 < 90) {
            XTestFakeButtonEvent(disp, 1, True, CurrentTime);
            XTestFakeButtonEvent(disp, 1, False, CurrentTime);
            XFlush(disp);
            break;
          }
        }
      XFree(image);
    }

    usleep(1000*1000 / FPS - 100);
  } while (!quit);

  return XCloseDisplay(disp);
}
