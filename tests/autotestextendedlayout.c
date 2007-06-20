#include <config.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

/*****************************************************************************/

#define log_test(condition) \
  log_test_impl(G_STRFUNC, (condition), #condition)
#define log_testf(condition, format, ...) \
  log_test_impl(G_STRFUNC, (condition), #condition " (" format ")", __VA_ARGS__)
#define log_testi(expected, number) G_STMT_START { \
    const gint i = (expected), j = (number); \
    log_test_impl(G_STRFUNC, i == j, \
                  #number " is " #expected " (actual number %d)", j); \
  } G_STMT_END

#define log_info(format, ...) \
  g_print ("INFO: %s: " format "\n", G_STRFUNC, __VA_ARGS__);
#define log_int_array(values, length) \
  log_int_array_impl (G_STRFUNC, #values, (values), (length))
#define log_int(value) \
  log_info (#value " is %d\n", (value));

/*****************************************************************************/

static const gchar lorem_ipsum[] =
  "<span weight=\"bold\" size=\"xx-large\">"
  "Lorem ipsum</span> dolor sit amet, consectetuer "
  "adipiscing elit. Aliquam sed erat. Proin lectus "
  "orci, venenatis pharetra, egestas id, tincidunt "
  "vel, eros. Integer fringilla. Aenean justo ipsum, "        
  "luctus ut, volutpat laoreet, vehicula in, libero.";

static int num_failures = 0;
static int num_warnings = 0;
static int num_errors = 0;
static int num_criticals = 0;

static GLogFunc default_log_handler;

/*****************************************************************************/

static void
log_int_array_impl (const gchar *function,
                    const gchar *var_name,
                    const gint  *values,
                    gsize        length)
{
  GString *tmp = g_string_new ("");
  
  if (length--)
    {
      g_string_append_printf (tmp, "{ %d", *values++);

      while (length--)
        g_string_append_printf (tmp, ", %d", *values++);

      g_string_append (tmp, " }");
    }
  else
    {
      g_string_append (tmp, "empty");
    }

  g_print ("INFO: %s: %s is %s\n", function, var_name, tmp->str);
  g_string_free (tmp, TRUE);
}

static void
log_test_impl (const gchar *function,
               gboolean     passed, 
               const gchar *test_name, ...)
{
  va_list args;
  char *str;

  if (!passed)
    ++num_failures;

  va_start (args, test_name);
  str = g_strdup_vprintf (test_name, args);
  va_end (args);

  g_printf ("%s: %s: %s\n", passed ? "PASS" : "FAIL", function, str);
  g_free (str);
}

static void
log_override_cb (const gchar   *log_domain,
		 GLogLevelFlags log_level,
		 const gchar   *message,
		 gpointer       user_data)
{
  if (log_level & G_LOG_LEVEL_WARNING)
    num_warnings++;

  if (log_level & G_LOG_LEVEL_ERROR)
    num_errors++;

  if (log_level & G_LOG_LEVEL_CRITICAL)
    num_criticals++;

  (* default_log_handler) (log_domain, log_level, message, user_data);
}

/*****************************************************************************/

static void
gtk_label_test_baselines (void)
{
  GtkExtendedLayout *layout;
  GtkWidget *widget;
  GtkLabel *label;

  gint num_baselines;
  gint *baselines;

  widget = g_object_ref_sink (gtk_label_new (NULL));
  layout = GTK_EXTENDED_LAYOUT (widget);
  label = GTK_LABEL (widget);

  baselines = NULL;
  gtk_label_set_text (label, NULL);
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (1, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (1, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (1, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line\n");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (2, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line\nSecond Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (2, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_markup (label, "First Line\n<big>Second Line</big>\nThird Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  log_testi (3, num_baselines);
  log_test (NULL != baselines); 
  g_free (baselines);

  g_object_unref (widget);
}

static void
gtk_label_test_height_for_width (void)
{
  GtkExtendedLayout *layout;
  GtkWidget *widget;
  GtkLabel *label;

  PangoLayout *ref;
  gint i, cx, cy, rcx, rcy;
  double cy_min, cy_max;

  widget = g_object_ref_sink (gtk_label_new (NULL));
  layout = GTK_EXTENDED_LAYOUT (widget);
  label = GTK_LABEL (widget);

  gtk_label_set_markup (label, lorem_ipsum);
  gtk_label_set_line_wrap_mode (label, PANGO_WRAP_CHAR);
  gtk_label_set_line_wrap (label, TRUE);

  ref = pango_layout_copy (gtk_label_get_layout (label));
  pango_layout_set_width (ref, -1);
  pango_layout_get_pixel_size (ref, &rcx, &rcy);
  g_object_unref (ref);

  log_info ("preferred layout size: %d \303\227 %d", rcx, rcy);

  for (i = 5; i >= 1; --i)
    {
      cy = gtk_extended_layout_get_height_for_width (layout, cx = rcx * i);
      log_info ("scale is %d, so width is %d. results in height of %d.", i, cx, cy);
      log_testi (rcy, cy);
    }

  cy_min = rcy;
  cy_max = rcy * 2.5;

  for (i = 2, cx = rcx / 2; cx >= rcy; ++i, cx = rcx / i, cy_min = cy, cy_max += rcy)
    {
      cy = gtk_extended_layout_get_height_for_width (layout, cx);
      log_info ("scale is 1/%d, so width is %d. results in height of %d.", i, cx, cy);
      log_testf (cy_min <= cy && cy <= cy_max, "%f <= %d <= %f", cy_min, cy, cy_max);
    }

  g_object_unref (widget);
}

static void
gtk_label_test_extended_layout (void)
{
  GtkExtendedLayoutFeatures features;
  GtkExtendedLayoutIface *iface;
  GtkExtendedLayout *layout;
  GtkWidget *widget;
  GtkLabel *label;

  widget = g_object_ref_sink (gtk_label_new (NULL));
  layout = GTK_EXTENDED_LAYOUT (widget);
  label = GTK_LABEL (widget);

  /* vtable */

  log_test (GTK_IS_EXTENDED_LAYOUT (label));
  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (label);

  log_test (NULL != iface->get_features);
  log_test (NULL != iface->get_height_for_width);
  log_test (NULL == iface->get_width_for_height);
  log_test (NULL != iface->get_natural_size);
  log_test (NULL != iface->get_baselines);

  /* feature set */

  features = gtk_extended_layout_get_features (layout);

  log_test (0 == (features & GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH));
  log_test (0 == (features & GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT));
  log_test (0 != (features & GTK_EXTENDED_LAYOUT_NATURAL_SIZE));
  log_test (0 != (features & GTK_EXTENDED_LAYOUT_BASELINES));

  gtk_label_set_line_wrap (label, TRUE);
  features = gtk_extended_layout_get_features (layout);

  log_test (0 != (features & GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH));
  log_test (0 == (features & GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT));
  log_test (0 != (features & GTK_EXTENDED_LAYOUT_NATURAL_SIZE));
  log_test (0 != (features & GTK_EXTENDED_LAYOUT_BASELINES));

  g_object_unref (widget);

  /* baseline support */

  gtk_label_test_baselines ();
  gtk_label_test_height_for_width ();

  /* height for width */
/*
  PangoLayout *tmp;

  log_info ("requisition: %d, %d", req.width, req.height);

  for (i = -5; i < 6; ++i)
    {
      const gint e = i < 4 ? 1 : 2;

      const gdouble sy_lower =
        i + 1 < 0 ? (abs(i + 1) + 1) : 
        i + 1 > 0 ? 1.0 / (abs(i + 1) + 1) : 1;
      const gdouble sy_upper = 
        i - e < 0 ? (abs(i - e) + 1) :
        i - e > 0 ? 1.0 / (abs(i - e) + 1) : 1;

      double sy;

      width =
        i < 0 ? req.width / (abs(i) + 1) :
        i > 0 ? req.width * (abs(i) + 1) :
        req.width;

      height = gtk_extended_layout_get_height_for_width (layout, width);
      sy = (double)height / req.height;

      log_info ("scale is %s%d, so width is %d, height is %d",
                i < 0 ? "1/" : "", abs(i) + 1, width, height);

      log_testf (sy_lower <= sy && sy <= sy_upper,
             "%f <= %f <= %f", sy_lower, sy, sy_upper);
    }
*/
}

/*****************************************************************************/

int
main(int argc, char **argv)
{
  default_log_handler = g_log_set_default_handler (log_override_cb, NULL);

  gtk_init (&argc, &argv);

  gtk_label_test_extended_layout ();

  log_testi (0, num_warnings);
  log_testi (0, num_errors);
  log_testi (0, num_criticals);
  log_testi (0, num_failures);
  
  return MAX(0, num_failures - 1);
}
