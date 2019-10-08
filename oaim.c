#include <err.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <giblib/giblib.h>

#define FPS 125
#define RECT_SIZE 10

union pixel {
  DATA32 in;
  struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
  } color;
};

int quit;
void sighandler(int _) { quit = 1; }

int main() {
  Display *disp;
  Window root;
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
    XGetWindowAttributes(disp, DefaultRootWindow(disp), &ra);
    cross_x = ra.width / 2 - RECT_SIZE;
    cross_y = ra.height / 2 - RECT_SIZE;
    imlib_context_set_display(disp);
    imlib_context_set_visual(vis);
  }

  do {
    Imlib_Image im = gib_imlib_create_image_from_drawable(root, 0,
        cross_x, cross_y, RECT_SIZE, RECT_SIZE, 1);
    imlib_context_set_image(im);
    DATA32 *data = imlib_image_get_data();

    for (unsigned i = 0; i < RECT_SIZE * RECT_SIZE; ++i) {
      union pixel p = { .in = data[i] };
      if (p.color.g >= 130 && p.color.r < 70 && p.color.b < 90) {
        XTestFakeButtonEvent(disp, 1, True, CurrentTime);
        XTestFakeButtonEvent(disp, 1, False, CurrentTime);
        XFlush(disp);
        break;
      }
    }

    imlib_free_image();
    usleep(1000*1000 / FPS - 100);
  } while (!quit);

  XCloseDisplay(disp);
  return 0;
}

