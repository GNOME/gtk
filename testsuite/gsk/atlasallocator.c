#include "config.h"

#include <gtk/gtk.h>

#include "gsk/gpu/gskatlasallocatorprivate.h"

static void
test_atlas_allocator_simple (void)
{
  GskAtlasAllocator *allocator;

  allocator = gsk_atlas_allocator_new (512, 512);

  gsk_atlas_allocator_free (allocator);
}

static void
test_atlas_allocator_allocate_all (void)
{
  GskAtlasAllocator *allocator;
  gsize pos;
  int width, height;
  const cairo_rectangle_int_t *area;

  width = g_test_rand_int_range (1, 1024);
  height = g_test_rand_int_range (1, 1024);

  allocator = gsk_atlas_allocator_new (width, height);

  pos = gsk_atlas_allocator_allocate (allocator, width, height);
  g_assert_cmpuint (pos, <, G_MAXSIZE);

  area = gsk_atlas_allocator_get_area (allocator, pos);
  g_assert_nonnull (area);
  g_assert_cmpint (area->x, ==, 0); /* current implementation detail */
  g_assert_cmpint (area->y, ==, 0); /* current implementation detail */
  g_assert_cmpint (area->width, ==, width);
  g_assert_cmpint (area->height, ==, height);

  gsk_atlas_allocator_deallocate (allocator, pos);

  gsk_atlas_allocator_free (allocator);
}

static void
test_atlas_allocator_simple_allocation (void)
{
  GskAtlasAllocator *allocator;
  gsize pos;
  int width, height;
  const cairo_rectangle_int_t *area;

  allocator = gsk_atlas_allocator_new (512, 512);

  width = g_test_rand_int_range (1, 512);
  height = g_test_rand_int_range (1, 512);

  pos = gsk_atlas_allocator_allocate (allocator, width, height);
  g_assert_cmpuint (pos, <, G_MAXSIZE);

  area = gsk_atlas_allocator_get_area (allocator, pos);
  g_assert_nonnull (area);
  g_assert_cmpint (area->x, ==, 0); /* current implementation detail */
  g_assert_cmpint (area->y, ==, 0); /* current implementation detail */
  g_assert_cmpint (area->width, ==, width);
  g_assert_cmpint (area->height, ==, height);

  gsk_atlas_allocator_deallocate (allocator, pos);

  gsk_atlas_allocator_free (allocator);
}

static void
test_atlas_allocator_exact_match (void)
{
  GskAtlasAllocator *allocator;
  gsize i, n;
  int width, height, item_width, item_height;
  gsize allocations[20 * 20];

  item_width = g_test_rand_int_range (1, 64);
  item_height = g_test_rand_int_range (1, 64);
  width = item_width * g_test_rand_int_range (1, 21);
  height = item_height * g_test_rand_int_range (1, 21);
  n = (width / item_width) * (height / item_height);

  allocator = gsk_atlas_allocator_new (width, height);

  for (i = 0; i < n; i++)
    {
      allocations[i] = gsk_atlas_allocator_allocate (allocator, item_width, item_height);
      g_assert_cmpuint (allocations[i], !=, G_MAXSIZE);
    }

  /* assert that the atlas if full */
  i = gsk_atlas_allocator_allocate (allocator, 1, 1);
  g_assert_cmpuint (i, ==, G_MAXSIZE);

  for (i = 0; i < n; i++)
    {
      gsk_atlas_allocator_deallocate (allocator, allocations[i]);
    }

  /* assert that the atlas is empty */
  i = gsk_atlas_allocator_allocate (allocator, width, height);
  g_assert_cmpuint (i, !=, G_MAXSIZE);
  gsk_atlas_allocator_deallocate (allocator, i);

  gsk_atlas_allocator_free (allocator);
}

static void
test_atlas_allocator_full_run (void)
{
#define WIDTH 1024
#define HEIGHT 1024
#define MAX_ALLOCATIONS 4096
#define RUNS 65536
  gsize allocations[MAX_ALLOCATIONS];
  GskAtlasAllocator *allocator;
  gsize cur_allocations, max_allocations, successes, fails, size, max_size;

  cur_allocations = 0;
  max_allocations = 0;
  successes = 0;
  fails = 0;
  size = 0;
  max_size = 0;

  allocator = gsk_atlas_allocator_new (WIDTH, HEIGHT);

  for (gsize i = 0; i < RUNS; i++)
    {
      gsize pos = g_test_rand_int_range (0, MAX_ALLOCATIONS);
      if (pos < cur_allocations)
        {
          cairo_rectangle_int_t area = *gsk_atlas_allocator_get_area (allocator, allocations[pos]);

          gsk_atlas_allocator_deallocate (allocator, allocations[pos]);
          cur_allocations--;
          allocations[pos] = allocations[cur_allocations];
          size -= area.width * area.height;
          if (g_test_verbose ())
            g_test_message ("%6zu del %4zu %4dx%d\tallocs: %zu/%zu size: %zu(%zu%%)/%zu(%zu%%) avg %zu attempts: %zu/%zu (%zu%%)",
                            i, pos, area.width, area.height,
                            cur_allocations, max_allocations,
                            size, size * 100 / (WIDTH * HEIGHT),
                            max_size, max_size * 100 / (WIDTH * HEIGHT),
                            size / cur_allocations,
                            successes, successes + fails, successes * 100 / (successes + fails));
        }
      else
        {
          gsize width = g_test_rand_int_range (1, 17) * g_test_rand_int_range (1, 33);
          gsize height = g_test_rand_int_range (1, 17) * g_test_rand_int_range (1, 33);

          allocations[cur_allocations] = gsk_atlas_allocator_allocate (allocator, width, height);
          if (allocations[cur_allocations] < G_MAXSIZE)
            {
              const cairo_rectangle_int_t *area = gsk_atlas_allocator_get_area (allocator, allocations[cur_allocations]);

              g_assert_cmpuint (width, ==, area->width);
              g_assert_cmpuint (height, ==, area->height);
              successes++;
              cur_allocations++;
              max_allocations = MAX (cur_allocations, max_allocations);
              size += width * height;
              max_size = MAX (size, max_size);
              if (g_test_verbose ())
                g_test_message ("%6zu add %4zu %4zux%zu\tallocs: %zu/%zu size: %zu(%zu%%)/%zu(%zu%%) avg %zu attempts: %zu/%zu (%zu%%)",
                                i, cur_allocations - 1, width, height,
                                cur_allocations, max_allocations,
                                size, size * 100 / (WIDTH * HEIGHT),
                                max_size, max_size * 100 / (WIDTH * HEIGHT),
                                size / cur_allocations,
                                successes, successes + fails, successes * 100 / (successes + fails));
            }
          else
            {
              fails++;
              if (g_test_verbose ())
                g_test_message ("%6zu fail     %4zux%zu\tallocs: %zu/%zu size: %zu(%zu%%)/%zu(%zu%%) avg %zu attempts: %zu/%zu (%zu%%)",
                                i, width, height,
                                cur_allocations, max_allocations,
                                size, size * 100 / (WIDTH * HEIGHT),
                                max_size, max_size * 100 / (WIDTH * HEIGHT),
                                size / cur_allocations,
                                successes, successes + fails, successes * 100 / (successes + fails));
            }
        }
    }

  for (gsize i = 0; i < cur_allocations; i++)
    {
      cairo_rectangle_int_t area = *gsk_atlas_allocator_get_area (allocator, allocations[i]);

      gsk_atlas_allocator_deallocate (allocator, allocations[i]);
      size -= area.width * area.height;
    }

  g_assert_cmpuint (size, ==, 0);

  gsk_atlas_allocator_free (allocator);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/atlasallocator/simple", test_atlas_allocator_simple);
  g_test_add_func ("/atlasallocator/allocate-all", test_atlas_allocator_allocate_all);
  g_test_add_func ("/atlasallocator/simple-allocation", test_atlas_allocator_simple_allocation);
  g_test_add_func ("/atlasallocator/exact-match", test_atlas_allocator_exact_match);
  g_test_add_func ("/atlasallocator/full-run", test_atlas_allocator_full_run);

  return g_test_run ();
}
