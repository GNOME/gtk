#include <gtk/gtk.h>
#include "gsk/gskroundedrectprivate.h"
#include "gsk/gskpathprivate.h"
#include "gsk/gskcontourprivate.h"

#include "gtk/svg/gtksvgpathutilsprivate.h"

typedef struct
{
  const char *path_str;
  SvgPathClassification expected;
} TestData;

static void
test_data_free (gpointer data)
{
  TestData *d = data;
  g_free ((char *) d->path_str);
  g_free (d);
}

static void
test_path (gconstpointer data)
{
  const TestData *d = data;
  GskPath *path;
  SvgPathClassification result;
  GskRoundedRect rect;
  const char *kind[] = {
    "empty", "rect", "rounded", "circle", "general"
  };

  if (g_test_verbose ())
    g_test_message ("Classify %s", d->path_str);

  path = gsk_path_parse (d->path_str);

  result = svg_path_classify (path, &rect);
  if (result != d->expected)
    g_test_fail_printf ("Expected '%s', got '%s'", kind[d->expected], kind[result]);

  gsk_path_unref (path);
}

static void
add_test (int                 pos,
          const char         *path_str,
          SvgPathClassification  c)
{
  TestData *data;
  char *test_name;

  data = g_new (TestData, 1);
  data->path_str = g_strdup (path_str);
  data->expected = c;

  test_name = g_strdup_printf ("/path/classify/%d", pos);
  g_test_add_data_func_full (test_name, data, test_path, test_data_free);
  g_free (test_name);
}

static void
add_path_test (int                 pos,
               GskPath            *path,
               SvgPathClassification  c)
{
  char *path_str;

  path_str = gsk_path_to_string (path);
  add_test (pos, path_str, c);
  g_free (path_str);
}

static void
add_rounded_rect_test (int                    pos,
                       GskRoundedRect        *rr,
                       SvgPathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rounded_rect (builder, rr);
  path = gsk_path_builder_free_to_path (builder);
  add_path_test (pos, path, c);
  gsk_path_unref (path);
}

static void
add_rect_test (int                    pos,
               graphene_rect_t       *rect,
               SvgPathClassification  c)
{
  GskPathBuilder *builder;
  GskPath *path;

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_rect (builder, rect);
  path = gsk_path_builder_free_to_path (builder);
  add_path_test (pos, path, c);
  gsk_path_unref (path);
}

#define RR(x,y,w,h,r) \
  (GskRoundedRect) { \
  .bounds = { \
    .origin = { x, y, }, \
    .size = { w, h, }, \
  }, \
  .corner[0] = { r, r, }, \
  .corner[1] = { r, r, }, \
  .corner[2] = { r, r, }, \
  .corner[3] = { r, r, }, \
}

static TestData tests[] = {
  { "", PATH_EMPTY },
  { "M10,10h20M20,20", PATH_GENERAL },
  /* the following are from org.gnome.Loupe.svg */
  { "m 2 2 h 124 v 124 h -124 z m 0 0", PATH_RECT },
  { "M 106 20 L 22 20 C 20.8945312 20, 20 20.8945312, 20 22 L 20 86 C 20 87.1054688, 20.8945312 88, 22 88 L 106 88 C 107.105469 88, 108 87.1054688, 108 86 L 108 22 C 108 20.8945312, 107.105469 20, 106 20 Z M 106 20", PATH_ROUNDED_RECT },
  { "M 0 66 L 98 66 L 98 152 L 0 152 Z M 0 66", PATH_RECT },
  { "M 0 0 h 192 v 152 h -192 z", PATH_RECT },
  { "M 93 67"
    "C 87.4765625 67"
    "  83 62.5234375"
    "  83 57"
    "C 83 51.4765625"
    "  87.4765625 47"
    "  93 47"
    "C 98.5234375 47"
    "  103 51.4765625"
    "  103 57"
    "C 103 62.5234375"
    "  98.5234375 67"
    "  93 67"
    "Z"
    "M 94 67", PATH_CIRCLE },
  { "M 52 44.125 C 35.4648438 44.125, 22.0625 57.5273438, 22.0625 74.0625 C 22.0625 90.5976562, 35.4648438 104, 52 104 C 68.5351562 104, 81.9375 90.5976562, 81.9375 74.0625 C 81.9375 57.5273438, 68.5351562 44.125, 52 44.125 Z M 52 44.125", PATH_CIRCLE },
  /* from Adwaitas x-package-repository.svg */
  { "M 64.9921875 74"
    "L 67 74"
    "C 67.5507812 74"
    "  68 74.4492188"
    "  68 75"
    "C 68 75.5507812"
    "  67.5507812 76"
    "  67 76"
    "L 64.9921875 76"
    "C 64.4414062 76"
    "  63.9921875 75.5507812"
    "  63.9921875 75"
    "C 63.9921875 74.4492188"
    "  64.4414062 74"
    "  64.9921875 74"
    "Z"
    "M 64.9921875 74", PATH_ROUNDED_RECT },
  { "M 58.9453125 44 L 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 Z M 58.9453125 44", PATH_ROUNDED_RECT },
  /* from panel-right-symbolic.svg */
  { "m 104 502 c 0 0.550781 -0.449219 1 -1 1 s -1 -0.449219 -1 -1 s 0.449219 -1 1 -1 s 1 0.449219 1 1 z m 0 0", PATH_CIRCLE },

  /* from network-wireless-signal-weak-symbolic.svg
   * This is approximating a circle with 5 cubics - too weird to bother with
   */
  { "M 8 10 C 7.48828077 10, 6.97656202 10.1953115, 6.58593798 10.5859385 C 5.80468798 11.3671885, 5.80468798 12.6328115, 6.58593798 13.4140615 C 7.36718798 14.1953115, 8.63281155 14.1953115, 9.41406155 13.4140615 C 10.1953115 12.6328115, 10.1953115 11.3671885, 9.41406155 10.5859385 C 9.02343845 10.1953115, 8.51171875 10, 8 10 Z  M 8 10", PATH_GENERAL },
 /* from org.gnome.SystemMonitor.Devel.svg */
 { "M 113 62 C 113 61.4500008, 113.449997 61, 114 61 C 114.550003 61, 115 61.4500008, 115 62 C 115 62.5499992, 114.550003 63, 114 63 C 113.449997 63, 113 62.5499992, 113 62 Z M 113 62", PATH_CIRCLE },
  /* not from anywhere */
  { "M 69.0546875 44 C 70.6875 44, 72 45.3125, 72 46.9453125 L 72 59.0546875 C 72 60.6875, 70.6875 62, 69.0546875 62 L 58.9453125 62 C 57.3125 62, 56 60.6875, 56 59.0546875 L 56 46.9453125 C 56 45.3125, 57.3125 44, 58.9453125 44 L 58.9453125 44 Z M 58.9453125 44", PATH_ROUNDED_RECT },
  { "M 23.078125 17 L 61.3671875 17 C 62.8007812 17, 63.9609375 18.1601562, 63.9609375 19.5898438 L 63.9609375 56.4101562 C 63.9609375 57.8398438, 62.8007812 59, 61.3671875 59 L 23.078125 59 C 21.6445312 59, 20.484375 57.8398438, 20.484375 56.4101562 L 20.484375 19.5898438 C 20.484375 18.1601562, 21.6445312 17, 23.078125 17 Z M 23.078125 17", PATH_ROUNDED_RECT },
  { "M 66.8554688 42.4921875"
    "L 109.113281 42.4921875"
    "C 113.972656 42.4921875"
    "  117.914062 46.4335938"
    "  117.914062 51.296875"
    "L 117.914062 81.2578125"
    "C 117.914062 86.1210938"
    "  113.972656 90.0625"
    "  109.113281 90.0625"
    "L 66.8554688 90.0625"
    "C 61.9921875 90.0625"
    "  58.0507812 86.1210938"
    "  58.0507812 81.2578125"
    "L 58.0507812 51.296875"
    "C 58.0507812 46.4335938"
    "  61.9921875 42.4921875"
    "  66.8554688 42.4921875"
    "Z"
    "M 66.8554688 42.4921875", PATH_ROUNDED_RECT },
  { "M 87.984375 8.0625"
    "C 104.515625 8.0625"
    "  117.914062 21.4648438"
    "  117.914062 37.9960938"
    "L 117.914062 60.1289062"
    "C 117.914062 76.6601562"
    "  104.515625 90.0625"
    "  87.984375 90.0625"
    "C 71.453125 90.0625"
    "  58.0507812 76.6601562"
    "  58.0507812 60.1289062"
    "L 58.0507812 37.9960938"
    "C 58.0507812 21.4648438"
    "  71.453125 8.0625"
    "  87.984375 8.0625"
    "Z"
    "M 87.984375 8.0625", PATH_ROUNDED_RECT },
  { "M 72.359375 111.140625"
    "L 103.519531 111.140625"
    "C 105.960938 111.140625"
    "  107.9375 113.121094"
    "  107.9375 115.5625"
    "C 107.9375 118.003906"
    "  105.960938 119.984375"
    "  103.519531 119.984375"
    "L 72.359375 119.984375"
    "C 69.9179688 119.984375"
    "  67.9375 118.003906"
    "  67.9375 115.562"
    "C 67.9375 113.121094"
    "  69.9179688 111.140625"
    "  72.359375 111.140625"
    "  Z"
    "M 72.359375 111.140625", PATH_ROUNDED_RECT },
  { "M 20 12"
    "L 108 12"
    "C 112.417969 12"
    "  116 15.7304688"
    "  116 20.3320312"
    "L 116 103.667969"
    "C 116 108.269531"
    "  112.417969 112"
    "  108 112"
    "L 20 112"
    "C 15.5820312 112"
    "  12 108.269531"
    "  12 103.667969"
    "L 12 20.3320312"
    "C 12 15.7304688"
    "  15.5820312 12"
    "  20 12"
    "Z"
    "M 20 12", PATH_ROUNDED_RECT },
};

int
main (int   argc,
      char *argv[])
{
  GskRoundedRect r;
  unsigned int i;

  gtk_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    add_test (i + 1, tests[i].path_str, tests[i].expected);

  i++;

  add_rect_test (i++, &GRAPHENE_RECT_INIT (0, 0, 10, 10), PATH_RECT);
  add_rounded_rect_test (i++, &RR (0, 0, 10, 10, 1), PATH_ROUNDED_RECT);
  add_rounded_rect_test (i++, &RR (0, 0, 10, 10, 0), PATH_GENERAL);

  r = GSK_ROUNDED_RECT_INIT (0, 0, 10, 10);
  r.corner[0].width = r.corner[1].height = 1;

  add_rounded_rect_test (i++, &r, PATH_GENERAL);

  return g_test_run ();
}

/* vim:set foldmethod=marker: */
