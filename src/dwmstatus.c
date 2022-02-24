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
#include <X11/Xlib.h>

void set_status(Display *display, Window window, char *str);
char* get_mem_usage();
char* get_date_time();
char* get_disk_usage(const char *path);
void process_mouse(Display *display, XEvent xevent);

char *key_name[] = {
    "first",
    "second (or middle)",
    "third"
};

int 
main(void) 
{
  const int MSIZE = 1024;
  Display *display;
  XEvent xevent;
  Window window;
  char *status;
  char *bg_color    = "#000000";
  // char *clr_green   = "#bbd5bd";
  char *clr_blue    = "#46d9ff";
  char *clr_yellow  = "#ecbe7b";
  // char *clr_c       = "#d9dbda";
  // char *clr_purple  = "#c8c7dc";
  char *disk_home_free;
  char *disk_sys_free;
  char *datetime;
  // int mfd;
  // struct input_event ie;
  char *mem_usage;
  time_t previousTime = time(NULL);
  time_t interval_status = 1;
  time_t currentTime;

  if (!(display = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  // Setup X11 for mouse grabbing
  window = DefaultRootWindow(display);
  XAllowEvents(display, AsyncBoth, CurrentTime);

  XGrabPointer(display, 
      window,
      1, 
      PointerMotionMask | ButtonPressMask | ButtonReleaseMask , 
      GrabModeAsync,
      GrabModeAsync, 
      None,
      None,
      CurrentTime);

  status = (char*) malloc(sizeof(char)*MSIZE);
  if(!status)
    return EXIT_FAILURE;

  while(1)
  { 
    // process_mouse(display, xevent);
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

// process_mouse(int mfd, struct input_event ie, Display *display)
void
process_mouse(Display *display, XEvent xevent)
{
  XNextEvent(display, &xevent);

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
}
