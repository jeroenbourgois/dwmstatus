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
 **/


#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

/* Return 1 if XI2 is available, 0 otherwise */
static int has_xi2(Display *dpy)
{
    int major, minor;
    int rc;

    /* We support XI 2.2 */
    major = 2;
    minor = 2;

    rc = XIQueryVersion(dpy, &major, &minor);
    if (rc == BadRequest) {
    printf("No XI2 support. Server supports version %d.%d only.\n", major, minor);
    return 0;
    } else if (rc != Success) {
    fprintf(stderr, "Internal Error! This is a bug in Xlib.\n");
    }

    printf("XI2 supported. Server provides version %d.%d.\n", major, minor);

    return 1;
}

static void select_events(Display *dpy, Window win)
{
    XIEventMask evmasks[1];
    unsigned char mask1[(XI_LASTEVENT + 7)/8];

    memset(mask1, 0, sizeof(mask1));

    /* select for button and key events from all master devices */
    XISetMask(mask1, XI_RawMotion);

    evmasks[0].deviceid = XIAllMasterDevices;
    evmasks[0].mask_len = sizeof(mask1);
    evmasks[0].mask = mask1;

    XISelectEvents(dpy, win, evmasks, 1);
    XFlush(dpy);
}

int main (int argc, char **argv)
{
    Display *dpy;
    int xi_opcode, event, error;
    XEvent ev;

    dpy = XOpenDisplay(NULL);

    if (!dpy) {
    fprintf(stderr, "Failed to open display.\n");
    return -1;
    }

    if (!XQueryExtension(dpy, "XInputExtension", &xi_opcode, &event, &error)) {
       printf("X Input extension not available.\n");
          return -1;
    }

    if (!has_xi2(dpy))
    return -1;

    /* select for XI2 events */
    select_events(dpy, DefaultRootWindow(dpy));

    while(1) {
    XGenericEventCookie *cookie = &ev.xcookie;
    XIRawEvent      *re;
    Window          root_ret, child_ret;
    int         root_x, root_y;
    int         win_x, win_y;
    unsigned int        mask;

    XNextEvent(dpy, &ev);

    if (cookie->type != GenericEvent ||
        cookie->extension != xi_opcode ||
        !XGetEventData(dpy, cookie))
        continue;

    switch (cookie->evtype) {
    case XI_RawMotion:
        re = (XIRawEvent *) cookie->data;
        XQueryPointer(dpy, DefaultRootWindow(dpy),
                  &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask);
        printf ("raw %g,%g root %d,%d\n",
            re->raw_values[0], re->raw_values[1],
            root_x, root_y);
        break;
    }
    XFreeEventData(dpy, cookie);
    }

    return 0;
}


/*
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <linux/input.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <X11/Xlib.h>

#define EVTMASK (ButtonPressMask | PointerMotionMask)

void set_status(Display *display, Window window, char *str);
char* get_mem_usage();
char* get_date_time();
char* get_disk_usage(const char *path);
void process_mouse(Display *display, Window window, XEvent xevent);
void setup_mouse(void *args);

char *key_name[] = {
    "first",
    "second (or middle)",
    "third"
};

struct mt_args {
  Display *display;
  Window window;
};

int 
main(void) 
{
  const int MSIZE = 1024;
  char *status;
  char *bg_color    = "#000000";
  char *clr_blue    = "#46d9ff";
  char *clr_yellow  = "#ecbe7b";
  char *disk_home_free;
  char *disk_sys_free;
  char *datetime;
  char *mem_usage;
  time_t previousTime = time(NULL);
  time_t interval_status = 1;
  time_t currentTime;
  Display *display;
  Window window;

 display = XOpenDisplay(0);
    window = XRootWindow(display, 0);

  // if (!(display = XOpenDisplay(NULL))) {
  //  fprintf(stderr, "Cannot open display.\n");
  //  return EXIT_FAILURE;
  //}

  //
  //window = DefaultRootWindow(display); 

  status = (char*) malloc(sizeof(char)*MSIZE);
  if(!status)
    return EXIT_FAILURE;

  // Mouse processing on a different thread
  // since XNextEvent blocks thread until input is received
  // No mouse movement = no input
  struct mt_args *mouse_thread_args = (struct mt_args *)malloc(sizeof(struct mt_args));
  mouse_thread_args->display = display;
  mouse_thread_args->window = window;

    fprintf(stderr, "Start mouse thread.\n");
  pthread_t tid;
  pthread_create(&tid, NULL, setup_mouse, (void *)mouse_thread_args);
  pthread_join(tid, NULL);

  // Main loop
  while(1)
  { 
    if((time(&currentTime) - previousTime) >= interval_status)
    {
      mem_usage = get_mem_usage(); 
      datetime = get_date_time();
      disk_home_free = get_disk_usage("/home");
      disk_sys_free = get_disk_usage("/"); 

      int ret = snprintf(
          status, 
          MSIZE, 
          "^b%s^^c%s^%s |  %s  %s | ^c%s^%s", 
          bg_color,
          clr_yellow,
          mem_usage,
          disk_sys_free,
          disk_home_free,
          clr_blue,
          datetime
          );
      if(ret >= MSIZE)
        fprintf(stderr, "error: buffer too small %d/%d\n", MSIZE, ret);
      set_status(display, window, status);
      previousTime += interval_status;
    }
  }

  pthread_exit(NULL);

  return 0;
}

void 
set_status(Display *display, Window window, char *str) 
{
  XStoreName(display, window, str);
  XSync(display, False);
}

// unsigned long
char *
get_mem_usage()
{
  char *buf;
  struct sysinfo si; 
  int error = sysinfo(&si);
  buf = (char*) malloc(sizeof(char)*65);
  if(error == 0) {
    const unsigned int GB = 1024 * 1024 * 1024;
    const double used = (double)(si.totalram-si.freeram-(si.bufferram+si.sharedram))/GB; 
    const char* color = "#ff00ff";
    sprintf(buf, "^c%s^MEM %.1f GB", color, used);
  } else {
    buf = "";
  }

  return buf;
}

char *
get_date_time() 
{
  char *buf;
  time_t result;
  struct tm *resulttm;
  
  buf = (char*) malloc(sizeof(char)*65);

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
    {
      fprintf(stderr, "Error getting localtime.\n");
      exit(1);
    }
  
  if(!strftime(buf, sizeof(char)*65-1, "[%a %b %d (%H:%M:%S)]", resulttm))
    {
      fprintf(stderr, "strftime is 0.\n");
      exit(1);
    }
	
  return buf;
}

char *
get_disk_usage(const char *path)
{
  char *buf;
	struct statvfs fs;

	if (statvfs(path, &fs) < 0) {
    fprintf(stderr, "statvfs failure.\n");
		return NULL;
	} 

  if((buf = (char*) malloc(sizeof(char)*64)) == NULL) {
    fprintf(stderr, "Cannot allocate memory for buf.\n");
    exit(1);
  }

  const unsigned int GB = 1024 * 1024 * 1024;
  const double total = (double)(fs.f_blocks * fs.f_frsize) / GB;
  const double available = (double)(fs.f_bfree * fs.f_frsize) / GB;
  const double used = total - available;
  const double usedPct = (double)(used / total) * (double)100;

  sprintf(buf, "%.0fGB / %.0fGB (%.0f%%)", used, available, usedPct);

	return buf;
}

void
setup_mouse(void *args)
{
  Display *display;
  Window window;
  display = ((struct mt_args*)args)->display;
  window = ((struct mt_args*)args)->window;

  fprintf(stdout, "Inside mouse thread\n");

  // Setup X11 for mouse grabbing
   XEvent xevent;
  //
    //XSelectInput(display, window, SubstructureNotifyMask);

   XGrabButton(display, 
      AnyButton,
      AnyModifier,
      window,
      1, 
      ButtonReleaseMask , 
      // PointerMotionMask | ButtonPressMask | ButtonReleaseMask , 
      GrabModeAsync,
      GrabModeAsync, 
      None,
      None);  
XGrabPointer(display, window, True,
                 ButtonReleaseMask ,
               GrabModeAsync,
               GrabModeAsync,
               None,
               None,
               CurrentTime);

  XAllowEvents(display, AsyncBoth, CurrentTime);
  XSync(display, 1); 

  while(1) {
    XNextEvent( display, &xevent );
    switch( xevent.type ) {
      case MotionNotify:
        printf("Mouse move      : [%d, %d]\n", xevent.xmotion.x_root, xevent.xmotion.y_root);
        break;
      case ButtonRelease:
        printf("Button pressed  : %s\n", key_name[xevent.xbutton.button - 1]);
        break;
    }
  }

// XSelectInput(display, window, PointerMotionMask);


  // PointerMotionMask | ButtonPressMask | ButtonReleaseMask
  // XSelectInput(display, window, NoEventMask);

   XGrabPointer(display, 
      window,
      1, 
      ButtonReleaseMask , 
      // PointerMotionMask | ButtonPressMask | ButtonReleaseMask , 
      GrabModeAsync,
      GrabModeAsync, 
      None,
      None,
      CurrentTime); 

   while(1) {
    XNextEvent(display, &xevent);
// XWindowEvent(display, 1, );
   // XWindowEvent(display, 1,  ButtonReleaseMask, &xevent);
    switch (xevent.type) {
      case MotionNotify:
        printf("Mouse move      : [%d, %d]\n", xevent.xmotion.x_root, xevent.xmotion.y_root);
        break;
      case ButtonPress:
        printf("Button pressed  : %s\n", key_name[xevent.xbutton.button - 1]);
        break;
      case ButtonRelease:
        printf("Button released : %s\n", key_name[xevent.xbutton.button - 1]);
        break;
    }
    // XPutBackEvent(display, &xevent);
  } 
}

/* void
process_mouse(Display *display, Window window, XEvent xevent)
{
  // if(XCheckWindowEvent(display, window, EVTMASK, &xevent)) {
} 
*/
