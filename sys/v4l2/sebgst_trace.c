#include <sys/time.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>

#define MAX_NUM_EVENTS 3000
#define MAX_EVENT_SIZE 80

struct SebGstEvent
{
  double time;
  char chars[MAX_EVENT_SIZE];
};

void sebgst_trace (const char *format, ...);
static void sebgst_trace_ (const char *format, va_list ap);

static struct SebGstEvent events[MAX_NUM_EVENTS];
static int num_events = 0;
static bool written = false;

static double
sebgst_clock (void)
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
sebgst_write_ (bool force)
{
  FILE *fh;
  static const char *filename = NULL;
  static double write_delay = -1.0;
  struct SebGstEvent *e;
  int i;
  double t;

  if (written)
    return;

  if (filename == NULL) {
    filename = getenv ("SEBGST_TRACE_FILENAME");
    if (filename == NULL)
      written = true;
  }

  if (!force) {
    if (write_delay < 0.0) {
      char *v = getenv ("SEBGST_TRACE_WRITE_DELAY");
      if (v != NULL)
        write_delay = atoi (v);
      else
        write_delay = 12000.0;
    }
    t = sebgst_clock ();
    if (t < write_delay)
      return;
  }

  fh = fopen (filename, "w");
  for (i = 0; i < num_events; i++) {
    e = &events[i];
    fprintf (fh, "[%f] %s\n", e->time, e->chars);
  }

  printf ("*** %s written\n", filename);
  fclose (fh);
  written = true;
}



static void
sebgst_trace_ (const char *format, va_list ap)
{
  struct SebGstEvent *e;
  double t;

  if (written)
    return;

  t = sebgst_clock ();

  if (num_events < MAX_NUM_EVENTS) {
    e = &events[num_events];
    vsnprintf (e->chars, MAX_EVENT_SIZE, format, ap);
    e->time = t;
    num_events++;
  }

  sebgst_write_ (false);
}


void
sebgst_write ()
{
  sebgst_write_ (true);
}


void
sebgst_trace (const char *format, ...)
{
  va_list ap;
  va_start (ap, format);
  sebgst_trace_ (format, ap);
  va_end (ap);
}
