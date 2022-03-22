/** ********************************************************************
 * DWM STATUS by <jeroen@daele.be>
 *
 * Sets the name of the root window, which DWM uses to render the status bar.
 * DWM has to be patched with status2d for the colors to work.
 *
 * https://dwm.suckless.org/patches/status2d/
 * 
 * Compile with:
 * gcc -Wall -pedantic -std=c99 -lX11 -lasound dwmstatus.c
 *
 * version: 1.0
 * 
 **/

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

static void select_events(Display *display, Window win);
static int has_xi2(Display *display);
void setup();
void setup_mouse();
void run();
void update_mouse();
void update_status();
void spawn_htop();
void set_status(Display *display, Window window, char *str);
char* get_mem_usage();
char* get_date_time();
char* get_disk_usage(const char *path);

/* variables */
Display *display;
Window window;
int xi_opcode, event, error;
XEvent ev;
XIEvent *xi_event;
XIRawEvent *xev;
int running = 0;
const int STATUS_BUFF_SIZE = 512;
char *status;
const char *BG_COLOR    = "#000000";
const char *CLR_BLUE    = "#46d9ff";
const char *CLR_YELLOW  = "#ecbe7b";
const char *CLR_WHITE   = "#ffffff";
const char *CLR_RED     = "#ff3024";
char *disk_home_free;
char *disk_sys_free;
char *datetime;
char *mem_usage;
time_t previousTime;
time_t interval_status = 1;
time_t currentTime;

/* Return 1 if XI2 is available, 0 otherwise */
static int has_xi2(Display *display)
{
  int major, minor;
  int rc;

  /* We support XI 2.2 */
  major = 2;
  minor = 2;

  rc = XIQueryVersion(display, &major, &minor);
  if (rc == BadRequest) {
    printf("No XI2 support. Server supports version %d.%d only.\n", major, minor);
    return 0;
  } else if (rc != Success) {
    fprintf(stderr, "Internal Error! This is a bug in Xlib.\n");
  }

  printf("XI2 supported. Server provides version %d.%d.\n", major, minor);

  return 1;
}

static void select_events(Display *display, Window win)
{
  XIEventMask evmasks[1];
  unsigned char xie2mask[(XI_LASTEVENT + 7)/8];

  memset(xie2mask, 0, sizeof(xie2mask));

  /* select for button and key events from all master devices */
  XISetMask(xie2mask, XI_RawButtonPress);

  evmasks[0].deviceid = XIAllMasterDevices;
  evmasks[0].mask_len = sizeof(xie2mask);
  evmasks[0].mask = xie2mask;

  XISelectEvents(display, win, evmasks, 1);
  XFlush(display);
}

void set_status(Display *display, Window window, char *str) 
{
  XStoreName(display, window, str);
  XSync(display, False);
}

// unsigned long
char * get_mem_usage()
{
  struct sysinfo si; 
  const int BUFF_SIZE = 64;
  int error = sysinfo(&si);
  double free_pct, used_pct;
  char *clr;
  if(error != 0) return "MEM_ERR";

  char *buf;
  buf = (char*) malloc(sizeof(char)*256);

  unsigned int mem_unit;
  const unsigned int GB = 1000 * 1000;

  mem_unit = 1;
  if (si.mem_unit != 0) {
    mem_unit = si.mem_unit;
  }

  /* Convert values to kbytes */
  if (mem_unit == 1) {
    si.totalram >>= 10;
    si.freeram >>= 10;
    si.sharedram >>= 10;
    si.bufferram >>= 10;
  } else {
    mem_unit >>= 10;
    /* TODO:  Make all this stuff not overflow when mem >= 4 Tb */
    si.totalram *= mem_unit;
    si.freeram *= mem_unit;
    si.sharedram *= mem_unit;
    si.bufferram *= mem_unit;
  }

  si.totalram /= GB;
  si.freeram /= GB;
  si.sharedram /= GB;
  si.bufferram /= GB;

  free_pct = ((double) si.freeram / si.totalram) * 100;
  used_pct = 100 - free_pct;

  if((int) used_pct > 75) {
    clr = malloc(strlen(CLR_RED) + 1);
    strcpy(clr, CLR_RED);
  } else {
    clr = malloc(strlen(CLR_BLUE) + 1);
    strcpy(clr, CLR_WHITE);
  }

  snprintf(buf, 
           BUFF_SIZE + 1, 
           "^c%s^ %s %.f%%^c%s^", 
           clr,
           "MEM:",
           used_pct,
           CLR_WHITE
           );
  return buf;
}

char * get_date_time() 
{
  char *buf;
  time_t result;
  struct tm *resulttm;
  const int BUFF_SIZE = 65;

  buf = (char*) malloc(sizeof(char) * BUFF_SIZE);

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
  {
    fprintf(stderr, "Error getting localtime.\n");
    exit(1);
  }

  if(!strftime(buf, sizeof(char) * BUFF_SIZE - 1, "%a %b %d %H:%M:%S", resulttm))
  {
    fprintf(stderr, "strftime is 0.\n");
    exit(1);
  }

  return buf;
}

char * get_disk_usage(const char *path)
{
  char *buf;
  const int BUFF_SIZE = 32;
  struct statvfs fs;

  if (statvfs(path, &fs) < 0) {
    fprintf(stderr, "statvfs failure.\n");
    return NULL;
  } 

  if((buf = (char*) malloc(sizeof(char) * BUFF_SIZE)) == NULL) {
    fprintf(stderr, "Cannot allocate memory for buf.\n");
    exit(EXIT_FAILURE);
  }

  const unsigned int GB = 1024 * 1024 * 1024;
  const double total = (double)(fs.f_blocks * fs.f_frsize) / GB;
  const double available = (double)(fs.f_bfree * fs.f_frsize) / GB;
  const double used = total - available;
  const double usedPct = (double)(used / total) * (double)100;

  snprintf(buf, BUFF_SIZE + 1, "%.0f%%", usedPct);

  return buf;
}

void setup_mouse()
{
  if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
    fprintf(stderr, "X Input extension not available.\n");
    exit(EXIT_FAILURE);
  }

  if (!has_xi2(display)) exit(EXIT_FAILURE);

  // select for XI2 events
  select_events(display, DefaultRootWindow(display));
}

void setup() 
{
  display = XOpenDisplay(NULL);
  window = XRootWindow(display, 0);

  if (!display) {
    fprintf(stderr, "Failed to open display.\n");
    exit(EXIT_FAILURE);
  }

  setup_mouse();

  previousTime = time(0);

  status = (char*) malloc(sizeof(char)* STATUS_BUFF_SIZE);
  if(!status)
    exit(EXIT_FAILURE);

  running = 1;
}

void run() 
{
  while(running) {
    update_mouse();
    update_status();
  }
}

void update_mouse()
{
  XGenericEventCookie *cookie = &ev.xcookie;
  if (XCheckTypedEvent(display, GenericEvent ,&ev)){
    if (cookie->type != GenericEvent ||
      cookie->extension != xi_opcode ||
      !XGetEventData(display, cookie)) {
    } else {
      Window              root_ret, child_ret;
      int                 root_x, root_y;
      int                 win_x, win_y;
      unsigned int        mask;

      xi_event = (XIEvent *) cookie->data;
      xev = (XIRawEvent *) xi_event;

      switch (cookie->evtype) {
        case XI_RawButtonPress:
          XQueryPointer(display, 
                        DefaultRootWindow(display),
                        &root_ret, 
                        &child_ret, 
                        &root_x, 
                        &root_y, 
                        &win_x, 
                        &win_y,
                        &mask);
          if (root_x >= 2960 && root_x <= 3630 && root_y >= 0 && root_y <= 20) {
            spawn_htop();
          }
          break;
      }
      XFreeEventData(display, cookie);
    }
  }
}

void update_status()
{
  if((time(&currentTime) - previousTime) >= interval_status)
  {
    mem_usage = get_mem_usage(); 
    datetime = get_date_time();
    disk_home_free = get_disk_usage("/home");
    disk_sys_free = get_disk_usage("/"); 

    snprintf(
        status, STATUS_BUFF_SIZE + 1, 
        "^b%s^^c%s^%s | HDD R: %s H: %s | ^c%s^%s", 
        BG_COLOR, CLR_WHITE, mem_usage, disk_sys_free, disk_home_free, CLR_WHITE, datetime
      );

    set_status(display, window, status);
    previousTime += interval_status;
  }
}

void spawn_htop()
{
  char *cmd = "kitty -e htop";    
  FILE *fp;
  if ((fp = popen(cmd, "r")) == NULL) {
    printf("Error opening pipe!\n");
  }
}

int main(int argc, char **argv)
{
  setup();
  run();
  XCloseDisplay(display);
  free(display);
  free(xi_event);
  free(xev);
  free(status);
  free(disk_home_free);
  free(disk_sys_free);
  free(mem_usage);
  free(datetime);
  return 0;
}
