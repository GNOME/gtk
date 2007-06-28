#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

/*****************************************************************************/

#define log_test(condition) \
  log_test_impl(G_STRFUNC, __LINE__, (condition), #condition)
#define log_testf(condition, format, ...) \
  log_test_impl(G_STRFUNC, __LINE__, (condition), #condition " (" format ")", __VA_ARGS__)
#define log_testi(expected, number) G_STMT_START { \
    const gint i = (expected), j = (number); \
    log_test_impl(G_STRFUNC, __LINE__, i == j, \
                  #number " is " #expected " (actual number %d, expected: %d)", j, i); \
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
               gint         lineno,
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

  g_printf ("%s: %s, line %d: %s\033[0m\n",
            passed ? "PASS" : "\033[1;31mFAIL", 
            function, lineno, str);

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

  log_info ("preferred layout size: %d\303\227%d", rcx, rcy);

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
      log_testf (cy_min <= cy && cy <= cy_max, "%f \342\211\244 %d  \342\211\244 %f",
                 cy_min, cy, cy_max);
    }

  g_object_unref (widget);
}

static void
gtk_label_test_natural_size (void)
{
  GtkExtendedLayout *layout;
  GtkWidget *widget;
  GtkLabel *label;

  const GEnumClass *ellipsize_class;
  const GEnumClass *wrap_class;

  GtkRequisition minimum;
  GtkRequisition natural;

  gint i, j;

  widget = g_object_ref_sink (gtk_label_new (lorem_ipsum));
  layout = GTK_EXTENDED_LAYOUT (widget);
  label = GTK_LABEL (widget);

  ellipsize_class = g_type_class_peek_static (PANGO_TYPE_ELLIPSIZE_MODE);
  wrap_class = g_type_class_peek_static (PANGO_TYPE_WRAP_MODE);

  for (i = -1; i < (gint)wrap_class->n_values; ++i)
    { /*      ^^^^^^^^^^^^ deal with the unsigned integer (some really annyoing invention) */
      const GEnumValue *const wrap = (i >= 0 ? &wrap_class->values[i] : NULL);
      const gchar *wrap_mode = (wrap ? wrap->value_nick : "none");

      gtk_label_set_line_wrap (label, NULL != wrap);

      if (wrap)
        gtk_label_set_line_wrap_mode (label, wrap->value);

      gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_NONE);
      gtk_widget_size_request (widget, &minimum);

      log_test (minimum.width > 100);

      log_info ("wrap mode `%s' leads to a minimum size of %d\303\227%d.", 
                wrap_mode, minimum.width, minimum.height);

      for (j = 0; j < ellipsize_class->n_values; ++j)
        {
          const GEnumValue *const ellipsize = &ellipsize_class->values[j];

          gtk_label_set_ellipsize (label, ellipsize->value);
          gtk_extended_layout_get_natural_size (layout, &natural);

          log_info ("ellipsize mode `%s' leads to a natural size of %d\303\227%d.", 
                    ellipsize->value_nick, natural.width, natural.height);
          log_test (!memcmp (&natural, &minimum, sizeof natural)); 
        }
    }
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

  gtk_label_test_baselines ();
  gtk_label_test_height_for_width ();
  gtk_label_test_natural_size ();
}

static void
gtk_bin_test_extended_layout (void)
{
  const GType types[] = 
    { 
      GTK_TYPE_ALIGNMENT, GTK_TYPE_BUTTON,
      GTK_TYPE_EVENT_BOX, GTK_TYPE_FRAME,
      G_TYPE_INVALID
    };

  GtkExtendedLayoutFeatures features;
  GtkExtendedLayoutIface *iface;
  GtkExtendedLayout *layout;

  GtkWindow *window;
  GtkWidget *label;
  GtkWidget *bin;

  gint baseline_label;
  gint baseline_bin;

  int i, y;

  for (i = 0; types[i]; ++i)
    {
      log_info ("Testing %s", g_type_name (types[i]));

      label = gtk_label_new (g_type_name (types[i]));

      bin = g_object_new (types[i], NULL);
      gtk_container_add (GTK_CONTAINER (bin), label);
      layout = GTK_EXTENDED_LAYOUT (bin);

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_container_add (GTK_CONTAINER (window), bin);
      gtk_widget_show_all (window);

      /* vtable */

      log_test (GTK_IS_EXTENDED_LAYOUT (bin));
      iface = GTK_EXTENDED_LAYOUT_GET_IFACE (bin);

      log_test (NULL != iface->get_features);
      log_test (NULL != iface->get_height_for_width);
      log_test (NULL != iface->get_width_for_height);
      log_test (NULL != iface->get_natural_size);
      log_test (NULL != iface->get_baselines);

      /* feature set */

      features = gtk_extended_layout_get_features (layout);

      log_test (0 == (features & GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH));
      log_test (0 == (features & GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT));
      log_test (0 != (features & GTK_EXTENDED_LAYOUT_NATURAL_SIZE));
      log_test (0 != (features & GTK_EXTENDED_LAYOUT_BASELINES));

      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      features = gtk_extended_layout_get_features (layout);

      log_test (0 != (features & GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH));
      log_test (0 == (features & GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT));
      log_test (0 != (features & GTK_EXTENDED_LAYOUT_NATURAL_SIZE));
      log_test (0 != (features & GTK_EXTENDED_LAYOUT_BASELINES));

      /* verify baseline propagation */

      baseline_label =
        gtk_extended_layout_get_single_baseline (
        GTK_EXTENDED_LAYOUT (label), GTK_BASELINE_FIRST);
      baseline_bin =
        gtk_extended_layout_get_single_baseline (
        GTK_EXTENDED_LAYOUT (bin), GTK_BASELINE_FIRST);

      if (!gtk_widget_translate_coordinates (label, bin, 0, 0, NULL, &y))
        log_testf (FALSE, "failed to translate GtkLabel coordinates "
                          "into %s coordinates", g_type_name (types[i]));

      log_testi (baseline_label + y, baseline_bin);

      gtk_container_set_border_width (GTK_CONTAINER (bin), 23);

      baseline_bin =
        gtk_extended_layout_get_single_baseline (
        GTK_EXTENDED_LAYOUT (bin), GTK_BASELINE_FIRST);

      log_testi (baseline_label + y + 23, baseline_bin);

      /* verify padding injection */

      if (GTK_TYPE_ALIGNMENT == types[i])
        {
          log_test (0 != (features & GTK_EXTENDED_LAYOUT_PADDING));

          gtk_alignment_set_padding (GTK_ALIGNMENT (bin), 5, 7, 11, 13);

          baseline_bin =
            gtk_extended_layout_get_single_baseline (
            GTK_EXTENDED_LAYOUT (bin), GTK_BASELINE_FIRST);

          log_testi (baseline_label + y + 28, baseline_bin);
        }

      gtk_widget_destroy (window);
    }
}

/*****************************************************************************/

int
main(int argc, char **argv)
{
  default_log_handler = g_log_set_default_handler (log_override_cb, NULL);

  gtk_init (&argc, &argv);

  gtk_label_test_extended_layout ();
  gtk_bin_test_extended_layout ();

  log_testi (0, num_warnings);
  log_testi (0, num_errors);
  log_testi (0, num_criticals);
  log_testi (0, num_failures);
  
  return MAX(0, num_failures - 1);
}
