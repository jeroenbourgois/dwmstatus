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
 * version: 1.5
 * 
 **/

#define _DEFAULT_SOURCE
#define BATT_NOW        "/sys/class/power_supply/BAT0/energy_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/energy_full"
#define BATT_STATUS     "/sys/class/power_supply/BAT0/status"
#define MEM_FILE        "/proc/meminfo"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <errno.h>

void setup();
void run();
void update_status();
void spawn_htop();
void set_status(Display *display, Window window, char *str);
// double get_memory_usage();
double get_mem_percentage();
double get_mem_total();
char* get_mem_usage();
char* get_date_time();
char* get_disk_usage(const char *path);
char* smprintf(char *fmt, ...);
char* getbattery();

/* variables */
Display *display;
Window window;
int xi_opcode, event, error;
int running = 0;
const int STATUS_BUFF_SIZE = 512;
char *status;
const char *BG_COLOR    = "#000000";
const char *CLR_BLUE    = "#46d9ff";
const char *CLR_YELLOW  = "#ecbe7b";
const char *CLR_WHITE   = "#ffffff";
const char *CLR_RED     = "#ff3024";
const char *CLR_GREEN   = "#00ffa4";
char *disk_home_free;
char *disk_sys_free;
char *datetime;
char *mem_usage;
char *battery;
time_t previousTime;
time_t interval_status  = 1;
time_t currentTime;
const float NAP_TIME    = 0.2; // 0.2 seconds

void set_status(Display *display, Window window, char *str) 
{
  XStoreName(display, window, str);
  XSync(display, False);
}

/* double get_mem_usage(){
    FILE* file;
    unsigned long long memtotal, memfree, memavailable, buffers, cached;

    file = fopen(MEM_FILE, "r");
    fscanf(file, "MemTotal: %llu kB MemFree: %llu kB MemAvailable: %llu kB Buffers: %llu kB Cached: %llu kB",
           &memtotal, &memfree, &memavailable, &buffers, &cached);
    fclose(file);

    double result = (memtotal - (memfree + buffers + cached));

    return result;
} */

double get_mem_total(){
    FILE* file;
    unsigned long long memtotal;
    file = fopen(MEM_FILE, "r");
    fscanf(file, "MemTotal: %llu kB",&memtotal);
    fclose(file);
    return memtotal;
}

double get_mem_percentage(){
    FILE* file;
    double percentage;
    int memory_used;
    //int memory_free;
    unsigned long long memtotal, memfree, memavailable, buffers, cached;

    file = fopen(MEM_FILE, "r");
    fscanf(file, "MemTotal: %llu kB MemFree: %llu kB MemAvailable: %llu kB Buffers: %llu kB Cached: %llu kB",
           &memtotal, &memfree, &memavailable, &buffers, &cached);
    fclose(file);

    memory_used = memtotal - memfree - cached - buffers;
    //memory_free = memtotal - memory_used;

    percentage = memory_used * 100 / memtotal;

    return percentage;
}

// unsigned long
char * get_mem_usage()
{
  const int BUF_SIZE = 64;
  double pct;
  char *clr;

  char *buf;
  buf = (char*) malloc(sizeof(char)*256);

  pct = get_mem_percentage();

  if((int) pct > 75) {
    clr = malloc(strlen(CLR_RED) + 1);
    strcpy(clr, CLR_RED);
  } else {
    clr = malloc(strlen(CLR_BLUE) + 1);
    strcpy(clr, CLR_GREEN);
  }

  snprintf(buf, 
           BUF_SIZE + 1, 
           "^c%s^ %s %.f%%^c%s^", 
           clr,
           "MEM:",
           pct,
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

char * smprintf(char *fmt, ...)
{
	va_list fmtargs;
	// char *buf = NULL;
  char *buf = malloc(sizeof(char)*100);

	va_start(fmtargs, fmt);
	if (vsprintf(buf, fmt, fmtargs) == -1){
		fprintf(stderr, "malloc vasprintf\n");
		exit(1);
    }
	va_end(fmtargs);

	return buf;
}

char * get_battery()
{
  long lnum1, lnum2 = 0;
  char *status = malloc(sizeof(char)*12);
  char s = '?';
  FILE *fp = NULL;
  if ((fp = fopen(BATT_NOW, "r"))) {
    fscanf(fp, "%ld\n", &lnum1);
    fclose(fp);
    fp = fopen(BATT_FULL, "r");
    fscanf(fp, "%ld\n", &lnum2);
    fclose(fp);
    fp = fopen(BATT_STATUS, "r");
    fscanf(fp, "%s\n", status);
    fclose(fp);
    if (strcmp(status,"Charging") == 0)
      s = '+';
    if (strcmp(status,"Discharging") == 0)
      s = '-';
    if (strcmp(status,"Full") == 0)
      s = '=';
    return smprintf("BAT: %c%ld%% |", s,(lnum1/(lnum2/100)));
  }
  else return smprintf("");
}

void setup() 
{
  display = XOpenDisplay(NULL);
  window = XRootWindow(display, 0);

  if (!display) {
    fprintf(stderr, "Failed to open display.\n");
    exit(EXIT_FAILURE);
  }

  // setup_mouse();

  previousTime = time(0);

  status = (char*) malloc(sizeof(char)* STATUS_BUFF_SIZE);
  if(!status)
    exit(EXIT_FAILURE);

  running = 1;
}

void run() 
{
  while(running) {
    // update_mouse();
    update_status();
    sleep(NAP_TIME);
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
    battery = get_battery();

    snprintf(
        status, STATUS_BUFF_SIZE + 1, 
        "^b%s^^c%s^%s %s | HDD R: %s H: %s | ^c%s^%s", 
        BG_COLOR, CLR_WHITE, battery, mem_usage, disk_sys_free, disk_home_free, CLR_WHITE, datetime
      );

    set_status(display, window, status);
    previousTime += interval_status;
  }
}

int main(int argc, char **argv)
{
  setup();
  run();
  XCloseDisplay(display);
  free(display);
  free(status);
  free(disk_home_free);
  free(disk_sys_free);
  free(mem_usage);
  free(datetime);
  return 0;
}
