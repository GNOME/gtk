
#ifndef __GTK_BENCHMARK_H__
#define __GTK_BENCHMARK_H__

#include <valgrind/callgrind.h>
#include <glib.h>

#define SAMPLE_SIZE 5


typedef struct _Benchmark Benchmark;
typedef void (*BenchmarkFunc)(Benchmark *b, gsize size);

struct _Benchmark
{
  char *name;
  gint64 start_time;
  gint64 end_time;
  gsize size;
  BenchmarkFunc func;
  guint profile : 1;
};

static void
benchmark_destroy (Benchmark *b)
{
  g_free (b->name);
}

static void
benchmark_start (Benchmark *b)
{
  b->start_time = g_get_monotonic_time ();

  if (b->profile)
    CALLGRIND_START_INSTRUMENTATION;
}

static void
benchmark_stop (Benchmark *b)
{
  if (b->profile)
    CALLGRIND_STOP_INSTRUMENTATION;

  b->end_time = g_get_monotonic_time ();
}

typedef struct
{
  GArray *benchmarks;
  char *profile_benchmark_name;
} BenchmarkSuite;

static void
benchmark_suite_init (BenchmarkSuite *bs,
                      const char     *profile_benchmark_name)
{
  bs->benchmarks = g_array_new (FALSE, TRUE, sizeof (Benchmark));
  g_array_set_clear_func (bs->benchmarks, (GDestroyNotify)benchmark_destroy);
  bs->profile_benchmark_name = (char *)profile_benchmark_name; // XXX strdup

  g_assert (SAMPLE_SIZE % 2 == 1);
}

static void
benchmark_suite_add (BenchmarkSuite *bs,
                     const char     *benchmark_name,
                     gsize           size,
                     BenchmarkFunc   benchmark_func)
{
  Benchmark *b;

  g_array_set_size (bs->benchmarks, bs->benchmarks->len + 1);
  b = &g_array_index (bs->benchmarks, Benchmark, bs->benchmarks->len - 1);

  b->name = (char *)benchmark_name; /* XXX strdup? */
  b->size = size;
  b->func = benchmark_func;
}

static int
benchmark_suite_run (BenchmarkSuite *bs)
{
  const guint n_benchmarks = bs->benchmarks->len;
  const gboolean profile = bs->profile_benchmark_name != NULL;
  guint i;

  if (profile)
    {
      /* For profiling, we only run the selected benchmark. */
      gboolean found = FALSE;


      for (i = 0; i < n_benchmarks; i ++)
        {
          Benchmark *b = &g_array_index (bs->benchmarks, Benchmark, i);

          if (strcmp (bs->profile_benchmark_name, b->name) == 0)
            {
              b->profile = TRUE;
              b->func (b, b->size);
              found = TRUE;
              break;
            }
        }

      if (!found)
        g_error ("No benchmark '%s' found", bs->profile_benchmark_name);
    }
  else
    {
      for (i = 0; i < n_benchmarks; i ++)
        {
          gint64 samples[SAMPLE_SIZE];
          Benchmark *b = &g_array_index (bs->benchmarks, Benchmark, i);
          int s, x, y;

          for (s = 0; s < SAMPLE_SIZE; s ++)
            {
              b->start_time = 0;
              b->end_time = 0;

              b->func (b, b->size);

              if (b->start_time == 0)
                g_error ("Benchmark '%s' did not call benchmark_start()", b->name);

              if (b->end_time == 0)
                g_error ("Benchmark '%s' did not call benchmark_stop()", b->name);

              samples[s] = b->end_time - b->start_time;
            }

          /* Bubble sort \o/ */
          for (x = 0; x < SAMPLE_SIZE; x ++)
            for (y = 0; y < SAMPLE_SIZE; y ++)
              if (samples[x] < samples[y])
                {
                  int k = samples[x];
                  samples[x] = samples[y];
                  samples[y] = k;
                }

          /* Median of SAMPLE_SIZE */
          printf ("%s (%" G_GSIZE_FORMAT ") |  %.2f\n", b->name, b->size,
                  samples[SAMPLE_SIZE / 2 + 1] / 1000.0);
        }
    }

  return 0;
}

#endif
