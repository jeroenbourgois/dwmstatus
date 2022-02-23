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

#define MOUSE_FILE "/dev/input/event23"

void setStatus(Display *dpy, char *str);
double getMemUsed();
char* getDateTime();
char * getDiskUsage(const char *path);
void processMouse(int mfd, struct input_event ie, Display *dpy);

int 
main(void) 
{
  const int MSIZE = 1024;
  Display *dpy;
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
  int mfd;
  struct input_event ie;
  double mem_used;

  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  if((mfd = open(MOUSE_FILE, O_RDONLY)) == -1) {
    perror("Opening mouse device");
    return EXIT_FAILURE;
  }

  status = (char*) malloc(sizeof(char)*MSIZE);
  if(!status)
    return EXIT_FAILURE;

  processMouse(mfd, ie, dpy);

  while(1)
  { 
    mem_used = getMemUsed(); 
    datetime = getDateTime();
    disk_home_free = getDiskUsage("/home");
    disk_sys_free = getDiskUsage("/"); 

    int ret = snprintf(
        status, 
        MSIZE, 
        "^b%s^^c%s^Mem %f |  %s  %s | ^c%s^%s", 
        bg_color,
        clr_yellow,
        mem_used,
        disk_sys_free,
        disk_home_free,
        clr_blue,
        datetime
        );
    if(ret >= MSIZE)
      fprintf(stderr, "error: buffer too small %d/%d\n", MSIZE, ret);
    setStatus(dpy, status);
    sleep(1);
  }
}

void 
setStatus(Display *dpy, char *str) 
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

// unsigned long
double
getMemUsed()
{
  struct sysinfo si; 
  int error = sysinfo(&si);
  if(error == 0) {
    /* printf("totalram: %lu\n", si.totalram);
    printf("freeram: %lu\n", si.freeram);
    printf("mem_unit: %u\n", si.mem_unit); */
    si.mem_unit*=1024.0f;
    si.mem_unit*=1024.0f;
    const unsigned int GB = 1024 * 1024 * 1024;
    return (double)(si.totalram-si.freeram-(si.bufferram+si.sharedram))/GB; 
    // return ((si.totalram-si.freeram)*si.mem_unit)/1024/1024;
  } else {
    return -1;
  }
}

char *
getDateTime() 
{
  char *buf;
  time_t result;
  struct tm *resulttm;
  
  if((buf = (char*) malloc(sizeof(char)*65)) == NULL)
  {
    fprintf(stderr, "Cannot allocate memory for buf.\n");
    exit(1);
  }

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
    {
      fprintf(stderr, "Error getting localtime.\n");
      exit(1);
    }
  
  if(!strftime(buf, sizeof(char)*65-1, "[%a %b %d] [%H:%M:%S]", resulttm))
    {
      fprintf(stderr, "strftime is 0.\n");
      exit(1);
    }
	
  return buf;
}

char *
getDiskUsage(const char *path)
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

  sprintf(buf, "%.0fG/%.0fG (%.0f%)", used, available, usedPct);

	return buf;
}

void
processMouse(int mfd, struct input_event ie, Display *dpy)
{
  Window root, child;
  int rootX, rootY, winX, winY;
  unsigned int mask;
  // read
  while(read(mfd, &ie, sizeof(struct input_event))) {
    if (ie.type == EV_SYN) {
      if (ie.code == REL_X || ie.code == REL_Y) {
        XQueryPointer(dpy, DefaultRootWindow(dpy), &root, &child, &rootX, &rootY, &winX, &winY, &mask); 
      }
    } else if (ie.type == EV_KEY) {
      switch (ie.code) {
        case BTN_LEFT:
          // printf("Left mouse button ");
          break;
        case BTN_RIGHT:
          // printf("Right mouse button ");
          break;
        default:
          break;
      }
      /* if (ie.value == 0)  printf("released!!\n");
         if (ie.value == 1)  printf("pressed!!\n"); */
      if(ie.code == BTN_LEFT && ie.value == 1) {
        // check coords
        if(rootY >= 0 && rootY < 20 && rootX > 3350 && rootX < 3470) {
          char *cmd = "kitty -e htop";    
          FILE *fp;
          if ((fp = popen(cmd, "r")) == NULL) {
            printf("Error opening pipe!\n");
          }
        }
      }
      // printf("x %d\ty %d\n", rootX, rootY);
    }
  }
}
