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
#if USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
#define case break;case
#define array_size(ARRAY) (sizeof(ARRAY)/sizeof(*ARRAY))
#define likely(expr)      (__builtin_expect((!!(expr)), 1))
#define unlikely(expr)    (__builtin_expect((!!(expr)), 0))

static unsigned int die;

#define USAGE \
  "Usage: [-f FPS] [-s shoot duration] [-d shoot delay] [-WG] [-X crosshair] [-C color]\n" \
  " -f <FPS>\tSet FPS\n" \
  " -s <SHOOT TIME>  Set shoot duration in milli seconds\n" \
  " -d <SHOOT DELAY> Set shoot delay in milli seconds (Format: ShootDelayMin-ShootDelayMax)\n" \
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
  unsigned int
    fps,             // Frames per seconds
    shoot_time,      // Milli seconds
    shoot_delay_min, // Milli seconds
    shoot_delay_max, // Milli seconds
    threshold,
    crosshair_number;
  struct color_range *color_ranges;
  unsigned int color_ranges_size;
};

#define Pixel_In_Range(X_COLOR , COLOR_RANGE) (\
  (X_COLOR.red   >= COLOR_RANGE.red[0])   && \
  (X_COLOR.red   <= COLOR_RANGE.red[1])   && \
  (X_COLOR.green >= COLOR_RANGE.green[0]) && \
  (X_COLOR.green <= COLOR_RANGE.green[1]) && \
  (X_COLOR.blue  >= COLOR_RANGE.blue[0])  && \
  (X_COLOR.blue  <= COLOR_RANGE.blue[1])     )

static inline void printCrosshair(uint8_t *ch) {
  for (unsigned xy = 0; xy < RECT_HEIGHT * RECT_WIDTH; ++xy)
    printf("%c%c", ch[xy] ? '_' : 'x', ((1+xy) % RECT_WIDTH) ? ' ' : '\n');
}

static int parse_options(int argc, char **argv, struct options *opts) {
  int o;
  struct color_range cr;

  while ((o = getopt(argc, argv, "hd:f:s:T:C:WGX:")) != -1)
    switch (o) {
      default:
        errx(1, USAGE);
      case 'f':
        if (! (opts->fps = atoi(optarg)))
          errx(1, "Invalid value for FPS: %s", optarg);
      case 'd':
        if (2 != sscanf(optarg, "%u-%u", &opts->shoot_delay_min, &opts->shoot_delay_max))
          errx(1, "Invalid value for shoot delay: %s", optarg);
        if (opts->shoot_delay_min > opts->shoot_delay_max)
          errx(1, "ShootDelayMin > ShootDelayMax: %s", optarg);
      case 's':
        if (! (opts->shoot_time = atoi(optarg)))
          errx(1, "Invalid value for shoot time: %s", optarg);
      case 'T':
        if (! (opts->threshold = atoi(optarg)))
          errx(1, "Invalid value for threshold: %s", optarg);
      case 'X':
        if (! (opts->crosshair_number = atoi(optarg)) || opts->crosshair_number > NUM_CROSSHAIRS)
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
        if (6 != sscanf(optarg, "%hhu-%hhu,%hhu-%hhu,%hhu-%hhu", &cr.red[0], &cr.red[1],
              &cr.green[0], &cr.green[1], &cr.blue[0], &cr.blue[1]))
          errx(1, "Invalid custom color format: %s", optarg);
        printf("- Shooting on custom color: %s\n", optarg);
ADD_COLOR_RANGE:
        for (unsigned i = 0; i <= 1; ++i) if
          (cr.red[i]   > 255) errx(1, "Invalid value for red: %u",   cr.red[i]);  else if
          (cr.blue[i]  > 255) errx(1, "Invalid value for blue: %u",  cr.blue[i]); else if
          (cr.green[i] > 255) errx(1, "Invalid value for green: %u", cr.green[i]);
        opts->color_ranges = realloc(opts->color_ranges, ++opts->color_ranges_size);
        opts->color_ranges[opts->color_ranges_size-1] = cr;
    }

  return 1;
}

static inline
void shoot(Display *disp,
           unsigned int delay_min,
           unsigned int delay_max,
           unsigned int shoot_time)
{
  unsigned int delay = delay_min;
  if (unlikely(delay_max))
    delay += ((rand() * 1000) % (delay_max - delay_min));
  if (unlikely(delay))
    usleep(delay);
  XTestFakeButtonEvent(disp, 1, True, CurrentTime), XFlush(disp);
  if (shoot_time)
    usleep(shoot_time);
  XTestFakeButtonEvent(disp, 1, False, CurrentTime), XFlush(disp);
}

static inline
int scan_image(XImage *image,
               union crosshair_area *crosshair,
               struct color_range *color_ranges,
               unsigned int color_ranges_size,
               unsigned int threshold)
{
  BGRA pixel;
  for (unsigned int i = 0; i < color_ranges_size; ++i) {
    unsigned int matched_pixels = 0;
    unsigned int *p = (unsigned int*) image->data;
    for (unsigned y = 0; y < image->height; ++y)
      for (unsigned x = 0; x < image->width; ++x) {
        pixel.in = *p++;
        if (crosshair->d2[y][x])
          if (Pixel_In_Range(pixel.out, color_ranges[i]))
            if (++matched_pixels >= threshold)
              return 1;
      }
  }
  return 0;
}

#if USE_SHM
/* Implementation using shared memory */
static int hitbot_with_shm(
    Display *disp,
    Visual *vis,
    Window game,
    union crosshair_area *_crosshair,
    int cross_x,
    int cross_y,
    struct options *_options)
{
  union crosshair_area crosshair = *_crosshair;
  struct options opts = *_options;
  XShmSegmentInfo shminfo = {
    .shmid = 0,
    .shmaddr = 0,
    .readOnly = False
  };

  if (! XShmQueryExtension(disp))
    return printf("XShmQueryExtension(): Shared memory extension is not available\n"), 0;

  XImage *image = XShmCreateImage(disp, vis, DefaultDepth(disp, DefaultScreen(disp)),
                                  ZPixmap, NULL, &shminfo, RECT_WIDTH, RECT_HEIGHT);
  if (! image)
    return printf("XShmCreateImage(): failed\n"), 0;

  shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * RECT_HEIGHT, IPC_CREAT|0777);
  if (shminfo.shmid < 0)
    return printf("shmget(): failed\n"), 0;
  printf("Shared Memory ID is: %d\n", shminfo.shmid);

  shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
  if (! shminfo.shmaddr)
    return printf("shmat(): failed\n"), 0;

  if (! XShmAttach(disp, &shminfo))
    return printf("XShmAttach(): failed\n"), 0;

  for (; !die; usleep(opts.fps))
    if (XShmGetImage(disp, game, image, cross_x, cross_y, AllPlanes))
      if (scan_image(image, &crosshair, opts.color_ranges, opts.color_ranges_size, opts.threshold))
        shoot(disp, opts.shoot_delay_min, opts.shoot_delay_max, opts.shoot_time);

  if (! XShmDetach(disp, &shminfo))
    printf("XShmDetach(): failed\n");
  if (shmdt(shminfo.shmaddr))
    printf("shmdt(): failed\n");
  if (shmctl(shminfo.shmid, IPC_RMID, NULL))
    printf("shmctl(): failed\n");
  return 1;
}
#endif

/* Implementation without shared memory */
static int hitbot_no_shm(
    Display *disp,
    Visual *vis,
    Window game,
    union crosshair_area *_crosshair,
    int cross_x,
    int cross_y,
    struct options *_options)
{
  union crosshair_area crosshair = *_crosshair;
  struct options opts = *_options;
  XImage *image;
  for (; !die; usleep(opts.fps))
    if ((image = XGetImage(disp, game, cross_x, cross_y, RECT_WIDTH, RECT_HEIGHT, AllPlanes, ZPixmap))) {
      if (scan_image(image, &crosshair, opts.color_ranges, opts.color_ranges_size, opts.threshold))
        shoot(disp, opts.shoot_delay_min, opts.shoot_delay_max, opts.shoot_time);
      XFree(image);
    }
  return 1;
}

static void   sighandler(int _) {++die;}
static Window get_game_window(Display*);

int main(int argc, char **argv) {
  struct options opts  = {
    .fps               = 1,
    .shoot_time        = 10,
    .shoot_delay_min   = 0,
    .shoot_delay_max   = 0,
    .color_ranges      = NULL,
    .color_ranges_size = 0,
    .threshold         = 100,
    .crosshair_number  = 1
  };

  if (! parse_options(argc, argv, &opts))
    return 1;

  union crosshair_area crosshair;
  memcpy(crosshair.d1, crosshair_areas[opts.crosshair_number-1], sizeof(crosshair.d1));
  printf("- Using %d FPS\n"
         "- Using shoot duration: %ums\n"
         "- Using shoot delay: %ums - %ums\n"
         "- Using crosshair %d:\n",
         opts.fps,
         opts.shoot_time,
         opts.shoot_delay_min, opts.shoot_delay_max,
         opts.crosshair_number);
  printCrosshair(crosshair.d1);
  opts.fps = 1000000 / opts.fps - 100;
  opts.shoot_time *= 1000;
  opts.shoot_delay_min *= 1000;
  opts.shoot_delay_max *= 1000;

/*INIT*/
  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);
  XSetErrorHandler((int (*)(Display*, XErrorEvent*)) sighandler);

  Display *disp = XOpenDisplay(NULL);
  if (! disp)
    errx(1, "Can't open X display");
  Screen *scr = ScreenOfDisplay(disp, DefaultScreen(disp));
  Visual *vis = DefaultVisual(disp, XScreenNumberOfScreen(scr));
  Window root = RootWindow(disp, XScreenNumberOfScreen(scr));
  Window game = get_game_window(disp);
  if (! game) {
    warnx("No game window found, falling back on root window.");
    game = root;
  }

  XWindowAttributes ra;
  XGetWindowAttributes(disp, game, &ra);
  int cross_x = ra.width / 2 - RECT_WIDTH / 2;
  int cross_y = ra.height / 2 - RECT_HEIGHT / 2;

#if USE_SHM
  if (!hitbot_with_shm(disp, vis, game, &crosshair, cross_x, cross_y, &opts))
    printf("Falling back on non shared memory version.\n"),
#endif
  hitbot_no_shm(disp, vis, game, &crosshair, cross_x, cross_y, &opts);

  XCloseDisplay(disp);
  printf("Bye bye\n");
  return 0;
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

static Window get_game_window(Display *disp) {
  unsigned long n;
  Window game = 0;
  Window *list = get_window_list(disp, &n);
  for (Window *win = list; n-- >= 1; ++win) {
    char *title = get_window_title(disp, *win);
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
