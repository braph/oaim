#define _GNU_SOURCE // strcasestr
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
#if USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
#define case break;case
#define likely(expr)      (__builtin_expect((!!(expr)), 1))
#define unlikely(expr)    (__builtin_expect((!!(expr)), 0))
#define array_size(ARRAY) (sizeof(ARRAY)/sizeof(*ARRAY))

Display *display;
static volatile int quit;

#define USAGE \
  "Usage: [-f FPS] [-s shoot duration] [-d shoot delay] [-WG] [-X crosshair] [-C color]\n" \
  " -f <FPS>\tSet FPS\n" \
  " -s <SHOOT TIME>  Set shoot duration in milli seconds\n" \
  " -d <SHOOT DELAY> Set shoot delay in milli seconds (Format: ShootDelayMin-ShootDelayMax)\n" \
  " -G\t\tShoot on green\n" \
  " -W\t\tShoot on white\n" \
  " -C <COLOR>\tShoot on custom color\n"\
  "\t\t(Format: RedMin-RedMax,GreenMin-GreenMax,BlueMin-BlueMax e.g. 0-70,130-256,0-90)\n" \
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
    { x x _ _ x x _ _ x x },
    { x x _ x x x x _ x x },
    { x _ x x x x x x _ x },
    { x _ x x x x x x _ x },
    { x x _ x x x x _ x x },
    { x x _ _ x x _ _ x x },
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
  uint8_t red[2], green[2], blue[2];
}
RANGE_WHITE = {
  .red   = { 235, 255 },
  .green = { 235, 255 },
  .blue  = { 235, 255 },
},
RANGE_GREEN = {
  .red   = { 0,   70  },
//.red   = { 0,   161 },
  .green = { 130, 255 },
  .blue  = { 0,   90  },
};

typedef union {
  unsigned int in;
  struct { uint8_t blue, green, red, alpha; } out;
} BGRA;

static const struct game_title {
  const char *game, *win_title;
} game_titles[] = {
  { .game = "OpenArena",            .win_title = "ioquake"   },
  { .game = "OpenArena",            .win_title = "openarena" },
  { .game = "OpenArena (YuEngine)", .win_title = "yuoa."     }
};

union crosshair_area {
  uint8_t d2[RECT_HEIGHT] [RECT_WIDTH];
  uint8_t d1[RECT_HEIGHT * RECT_WIDTH];
};

struct options {
  unsigned int fps;             // Frames per seconds
  unsigned int shoot_time;      // Milli seconds
  unsigned int shoot_delay_min; // Milli seconds
  unsigned int shoot_delay_max; // Milli seconds
  unsigned int threshold;       // Number of matched pixels to shoot
  unsigned int crosshair_number;
  struct color_range *color_ranges;
  unsigned int color_ranges_size;
} options = {
  .fps               = 1,
  .shoot_time        = 10,
  .shoot_delay_min   = 0,
  .shoot_delay_max   = 0,
  .threshold         = 100,
  .crosshair_number  = 1,
  .color_ranges      = NULL,
  .color_ranges_size = 0
};

#define Pixel_In_Range(X_COLOR, COLOR_RANGE) (\
  (X_COLOR.red   >= COLOR_RANGE.red[0])   && \
  (X_COLOR.red   <= COLOR_RANGE.red[1])   && \
  (X_COLOR.green >= COLOR_RANGE.green[0]) && \
  (X_COLOR.green <= COLOR_RANGE.green[1]) && \
  (X_COLOR.blue  >= COLOR_RANGE.blue[0])  && \
  (X_COLOR.blue  <= COLOR_RANGE.blue[1])     )

static inline void printCrosshair(union crosshair_area *chrosshair) {
  for (int y = 0; y < RECT_WIDTH; ++y) {
    for (int x = 0; x < RECT_HEIGHT; ++x)
      printf("%c ", chrosshair->d2[x][y] ? '_' : 'x');
    printf("\n");
  }
}

static int parse_options(int argc, char **argv) {
  int o;
  struct color_range cr;

  while ((o = getopt(argc, argv, "hd:f:s:T:C:WGX:")) != -1)
    switch (o) {
      default:
        errx(1, USAGE);
      case 'f':
        if (! (options.fps = atoi(optarg)))
          errx(1, "Invalid value for FPS: %s", optarg);
      case 'd':
        if (2 != sscanf(optarg, "%u-%u", &options.shoot_delay_min, &options.shoot_delay_max))
          errx(1, "Invalid value for shoot delay: %s", optarg);
        if (options.shoot_delay_min > options.shoot_delay_max)
          errx(1, "ShootDelayMin > ShootDelayMax: %s", optarg);
      case 's':
        if (! (options.shoot_time = atoi(optarg)))
          errx(1, "Invalid value for shoot time: %s", optarg);
      case 'T':
        if (! (options.threshold = atoi(optarg)))
          errx(1, "Invalid value for threshold: %s", optarg);
      case 'X':
        if (! (options.crosshair_number = atoi(optarg)) || options.crosshair_number > NUM_CROSSHAIRS)
          errx(1, "Invalid value for crosshair: %s", optarg);
      case 'W':
        cr = RANGE_WHITE;
        puts("- Shooting on white color");
        goto ADD_COLOR_RANGE;
      case 'G':
        cr = RANGE_GREEN;
        puts("- Shooting on green color");
        goto ADD_COLOR_RANGE;
      case 'C':
        if (6 != sscanf(optarg, "%hhu-%hhu,%hhu-%hhu,%hhu-%hhu",
              &cr.red[0],   &cr.red[1],
              &cr.green[0], &cr.green[1],
              &cr.blue[0],  &cr.blue[1]))
          errx(1, "Invalid custom color format: %s", optarg);
        printf("- Shooting on custom color: %s\n", optarg);
ADD_COLOR_RANGE:
        for (unsigned i = 0; i <= 1; ++i)
          /**/ if (cr.red[i]   > 255) errx(1, "Invalid value for red: %u",   cr.red[i]);
          else if (cr.blue[i]  > 255) errx(1, "Invalid value for blue: %u",  cr.blue[i]);
          else if (cr.green[i] > 255) errx(1, "Invalid value for green: %u", cr.green[i]);

        options.color_ranges = realloc(options.color_ranges, ++options.color_ranges_size);
        options.color_ranges[options.color_ranges_size-1] = cr;
    }

  return 1;
}

static inline
void shoot(
  unsigned int delay_min,
  unsigned int delay_max,
  unsigned int shoot_time)
{
  unsigned int delay = delay_min;
  if (unlikely(delay_max))
    delay += ((rand() * 1000) % (delay_max - delay_min));
  if (unlikely(delay))
    usleep(delay);
  XTestFakeButtonEvent(display, 1, True, CurrentTime);
  XFlush(display);
  if (shoot_time)
    usleep(shoot_time);
  XTestFakeButtonEvent(display, 1, False, CurrentTime);
  XFlush(display);
}

static inline
int scan_image(
    XImage *image,
    union crosshair_area *crosshair,
    struct color_range *color_ranges,
    unsigned int color_ranges_size,
    unsigned int threshold)
{
  BGRA pixel;
  const unsigned int data_size = RECT_HEIGHT * RECT_WIDTH;

  for (unsigned int i = 0; i < color_ranges_size; ++i) {
    unsigned int matched_pixels = 0;
    unsigned int *p = (unsigned int*) image->data;
    uint8_t      *c = crosshair->d1;

    for (unsigned d = 0; d < data_size; ++d) {
      if (*c) {
        pixel.in = *p;
        if (Pixel_In_Range(pixel.out, color_ranges[i]))
          if (++matched_pixels >= threshold)
            return matched_pixels;
      }

      ++c;
      ++p;
    }
  }

  return 0;
}

/// Scan crosshair area and shoot if needed
static inline
void scan_and_shoot(
    XImage* image,
    union crosshair_area *crosshair)
{
  if (scan_image(image, crosshair, options.color_ranges, options.color_ranges_size, options.threshold))
    shoot(options.shoot_delay_min, options.shoot_delay_max, options.shoot_time);
}

#if USE_SHM
/// Implementation using shared memory
static
int hitbot_with_shm(
    Visual *vis,
    Window game,
    union crosshair_area *crosshair,
    const int cross_x,
    const int cross_y)
{
  XShmSegmentInfo shminfo = {
    .shmid = 0,
    .shmaddr = 0,
    .readOnly = False
  };

  if (! XShmQueryExtension(display))
    return puts("XShmQueryExtension(): Shared memory extension is not available"), 0;

  XImage *image = XShmCreateImage(display, vis, DefaultDepth(display, DefaultScreen(display)),
                                  ZPixmap, NULL, &shminfo, RECT_WIDTH, RECT_HEIGHT);
  if (! image)
    return puts("XShmCreateImage(): failed"), 0;

  shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * RECT_HEIGHT, IPC_CREAT|0777);
  if (shminfo.shmid < 0)
    return puts("shmget(): failed"), 0;
  printf("Shared Memory ID is: %d\n", shminfo.shmid);

  shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
  if (! shminfo.shmaddr)
    return puts("shmat(): failed"), 0;

  if (! XShmAttach(display, &shminfo))
    return puts("XShmAttach(): failed"), 0;

  for (; !quit; usleep(options.fps))
    if (XShmGetImage(display, game, image, cross_x, cross_y, AllPlanes))
      scan_and_shoot(image, crosshair);

  if (! XShmDetach(display, &shminfo))
    puts("XShmDetach(): failed");

  if (shmdt(shminfo.shmaddr))
    puts("shmdt(): failed");

  if (shmctl(shminfo.shmid, IPC_RMID, NULL))
    puts("shmctl(): failed");

  return 1;
}
#endif

/// Implementation without shared memory
static int hitbot_no_shm(
    Window game,
    union crosshair_area *crosshair,
    int cross_x,
    int cross_y)
{
  XImage *image;

  for (; !quit; usleep(options.fps))
    if ((image = XGetImage(display, game, cross_x, cross_y, RECT_WIDTH, RECT_HEIGHT, AllPlanes, ZPixmap))) {
      scan_and_shoot(image, crosshair);
      XFree(image);
    }

  return 1;
}

static void sighandler(int sig) {quit = sig;}
static Window get_game_window();

int main(int argc, char **argv) {
  if (! parse_options(argc, argv))
    return 1;

  union crosshair_area crosshair;
  memcpy(crosshair.d1, crosshair_areas[options.crosshair_number-1], sizeof(crosshair.d1));
  printf("- Using %d FPS\n"
         "- Using shoot duration: %ums\n"
         "- Using shoot delay: %ums - %ums\n"
         "- Using crosshair %d:\n",
         options.fps,
         options.shoot_time,
         options.shoot_delay_min, options.shoot_delay_max,
         options.crosshair_number);
  printCrosshair(&crosshair);
  options.fps = 1000000 / options.fps - 100;
  options.shoot_time *= 1000;
  options.shoot_delay_min *= 1000;
  options.shoot_delay_max *= 1000;

  // INIT
  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);
  XSetErrorHandler((int (*)(Display*, XErrorEvent*)) sighandler);

  display = XOpenDisplay(NULL);
  if (! display)
    errx(1, "Can't open X display");
  Screen *scr = ScreenOfDisplay(display, DefaultScreen(display));
  Visual *vis = DefaultVisual(display, XScreenNumberOfScreen(scr));
  Window root = RootWindow(display, XScreenNumberOfScreen(scr));
  Window game = get_game_window(display);
  if (! game) {
    warnx("No game window found, falling back on root window.");
    game = root;
  }

  XWindowAttributes ra;
  XGetWindowAttributes(display, game, &ra);
  int cross_x = ra.width / 2 - RECT_WIDTH / 2;
  int cross_y = ra.height / 2 - RECT_HEIGHT / 2;

#if USE_SHM
  if (! hitbot_with_shm(vis, game, &crosshair, cross_x, cross_y))
    puts("Falling back on non shared memory version."),
#endif
  hitbot_no_shm(game, &crosshair, cross_x, cross_y);

  XCloseDisplay(display);
  puts("Bye bye");
  return 0;
}

static Window *get_window_list(unsigned long *len) {
  Atom type, prop = XInternAtom(display, "_NET_CLIENT_LIST", False);
  int form;
  unsigned long remain;
  unsigned char *list;
  XGetWindowProperty(display, XDefaultRootWindow(display), prop, 0, 1024, False,
      XA_WINDOW, &type, &form, len, &remain, &list);
  return (Window*)list;
}

static char *get_window_title(Window window) {
  Atom type, prop = XInternAtom(display, "WM_CLASS", False);
  int form;
  unsigned long remain, len;
  unsigned char *list;
  XGetWindowProperty(display, window, prop, 0, 1024, False, AnyPropertyType,
      &type, &form, &len, &remain, &list);
  return (char*)list;
}

static Window get_game_window() {
  unsigned long n;
  Window game = 0;
  Window *list = get_window_list(&n);
  for (Window *win = list; n-- >= 1; ++win) {
    char *title = get_window_title(*win);
    for (unsigned int i = 0; i < array_size(game_titles); ++i) {
      if (strcasestr(title, game_titles[i].win_title)) {
        printf("Found game '%s' ('%s' =~ '%s')\n", game_titles[i].game, title, game_titles[i].win_title);
        game = *win;
      }
    }
    free(title);
  }
  free(list);
  return game;
}
