/* Exercise the event storm log without an X server. */
#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static char logfile[PATH_MAX];

static size_t loglines(char *last, size_t size) {
  FILE *f = fopen(logfile, "r");
  char line[1024];
  size_t n = 0;

  if (!f)
    return 0;
  while (fgets(line, sizeof line, f)) {
    n++;
    snprintf(last, size, "%s", line);
  }
  fclose(f);
  return n;
}

/* count events of one type, spread evenly over [from, from + span) ms */
static void feed(int type, Window w, unsigned long count, long long from,
                 long long span) {
  XEvent ev = {0};

  ev.type = type;
  ev.xany.window = w;
  if (type == MapRequest) {
    ev.xmaprequest.parent = root;
    ev.xmaprequest.window = w;
  }
  for (unsigned long i = 0; i < count; i++)
    stormcount(&ev, from + (long long)i * span / (long long)count);
}

int main(void) {
  static union { long align; char pad[8192]; } fake; /* holds the public _XPrivDisplay fields */
  char dir[] = "/tmp/dwm-storm-XXXXXX", last[1024] = "";
  Monitor monitor = {.barwin = 7};
  Client client = {.win = 42, .mon = &monitor};

  assert(mkdtemp(dir));
  setenv("XDG_STATE_HOME", dir, 1);
  snprintf(logfile, sizeof logfile, "%s/dwm/storm.log", dir);
  dpy = (Display *)&fake;
  root = 1;
  mons = &monitor;
  monitor.clients = &client;
  strcpy(client.appname, "firefox");

  /* A client retitling 3000 times a second, plus 30000 requests from dwm. */
  feed(PropertyNotify, 42, 3000, 1000, 990);
  ((_XPrivDisplay)dpy)->request += 30000;
  feed(MapRequest, 42, 5, 1990, 9);
  feed(Expose, 7, 1, 2000, 1);
  assert(loglines(last, sizeof last) == 1);
  assert(strstr(last, "3005 events, 30000 requests"));
  assert(strstr(last, "PropertyNotify 3000"));
  assert(strstr(last, "MapRequest 5")); /* counted on the client, not on root */
  assert(strstr(last, "0x2a (firefox) 3005"));

  /* A normal second (bar redraws, a few events) leaves no line. */
  feed(Expose, 7, 200, 2001, 990);
  ((_XPrivDisplay)dpy)->request += 500;
  feed(Expose, 7, 1, 3001, 1);
  assert(loglines(last, sizeof last) == 1);

  /* Requests alone are enough: dwm flooding the server with few events. */
  feed(Expose, 7, 10, 3002, 990);
  ((_XPrivDisplay)dpy)->request += 25000;
  feed(Expose, 7, 1, 4002, 1);
  assert(loglines(last, sizeof last) == 2);
  assert(strstr(last, "25000 requests") && strstr(last, "0x7 (bar)"));

  remove(logfile);
  snprintf(last, sizeof last, "%s/dwm", dir);
  rmdir(last);
  rmdir(dir);
  puts("PASS: storm log names event types and windows, stays quiet below the limits");
  return 0;
}
