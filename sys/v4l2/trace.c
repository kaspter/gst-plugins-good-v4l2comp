#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#define _MAX_NUM_EVENTS 3000
#define _MAX_DELAY 6000
#define _MAX_NUM_ARGS 4

struct _Event
{
  const char *format;
  void *args[_MAX_NUM_ARGS];
  double time;
};

void trace_event (const char *format, void *a0, void *a1, void *a2, void *a3);

static struct _Event _events[_MAX_NUM_EVENTS];
static int _num_events = 0;

static double
_clock (void)
{
  static bool t0_set = false;
  static double t0;
  double t;
  struct timeval x;

  gettimeofday (&x, NULL);
  t = (x.tv_sec * 1000.0) + (x.tv_usec / 1000.0);
  if (!t0_set) {
    t0 = t;
    t0_set = true;
  }

  return t - t0;
}



static void
_write_events (void)
{
  FILE *fh;
  static const char *filename = "/data/sebgst-prefix/events.log";
  static bool written = false;
  struct _Event *e;
  int i;
  char buf[1000];

  if (written)
    return;

  fh = fopen (filename, "w");
  for (i = 0; i < _num_events; i++) {
    e = &_events[i];
    sprintf (buf, e->format, e->time, e->args[0], e->args[1], e->args[2],
        e->args[3]);
    fprintf (fh, "%s\n", buf);
  }

  printf ("*** %s written\n", filename);
  fclose (fh);
  written = true;
}



void
trace_event (const char *format, void *a0, void *a1, void *a2, void *a3)
{
  struct _Event *e;
  double t;

  t = _clock ();

  if (_num_events < _MAX_NUM_EVENTS) {
    e = &_events[_num_events];
    e->format = format;
    e->args[0] = a0;
    e->args[1] = a1;
    e->args[2] = a2;
    e->args[3] = a3;
    e->time = t;
    _num_events++;
  }

  if (t > _MAX_DELAY)
    _write_events ();
}
