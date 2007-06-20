#include <config.h>
#include <string.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#define test(condition) \
        log_test((condition), G_STRFUNC, #condition)
#define testf(condition, format, ...) \
        log_test((condition), G_STRFUNC, \
                 #condition " (" format ")", __VA_ARGS__)
#define testi(expected, number) G_STMT_START { \
          const gint i = (expected), j = (number); \
          log_test(i == j, G_STRFUNC, #number " is " #expected \
                                      " (actual number %d)", j); \
        } G_STMT_END

#define log_int_array(values, length) \
        log_int_array_impl (G_STRFUNC, #values, (values), (length))

static int num_failures = 0;
static int num_warnings = 0;
static int num_errors = 0;
static int num_criticals = 0;

static GLogFunc default_log_handler;

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
log_test (gboolean     passed, 
          const gchar *function,
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

static void
test_label (void)
{
  GtkExtendedLayoutFeatures features;
  GtkExtendedLayoutIface *iface;
  GtkExtendedLayout *layout;
  GtkWidget *widget;
  GtkLabel *label;

  gint *baselines;
  gint num_baselines;

  widget = g_object_ref_sink (gtk_label_new (NULL));
  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (widget);
  layout = GTK_EXTENDED_LAYOUT (widget);
  label = GTK_LABEL (widget);

  /* basic properties */

  test (GTK_IS_EXTENDED_LAYOUT (label));
  features = gtk_extended_layout_get_features (layout);

  test (NULL != iface->get_features);
  test (NULL != iface->get_height_for_width);
  test (NULL == iface->get_width_for_height);
  test (NULL != iface->get_natural_size);
  test (NULL != iface->get_baselines);

  test (0 != (features & GTK_EXTENDED_LAYOUT_HEIGHT_FOR_WIDTH));
  test (0 == (features & GTK_EXTENDED_LAYOUT_WIDTH_FOR_HEIGHT));
  test (0 != (features & GTK_EXTENDED_LAYOUT_NATURAL_SIZE));
  test (0 != (features & GTK_EXTENDED_LAYOUT_BASELINES));

  /* baseline support */

  baselines = NULL;
  gtk_label_set_text (label, NULL);
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (1, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (1, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (1, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line\n");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (2, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_text (label, "First Line\nSecond Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (2, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  baselines = NULL;
  gtk_label_set_markup (label, "First Line\n<big>Second Line</big>\nThird Line");
  num_baselines = gtk_extended_layout_get_baselines (layout, &baselines);
  log_int_array (baselines, num_baselines);
  testi (3, num_baselines);
  test (NULL != baselines); 
  g_free (baselines);

  g_object_unref (widget);
}

int
main(int argc, char **argv)
{
  default_log_handler = g_log_set_default_handler (log_override_cb, NULL);

  gtk_init (&argc, &argv);

  test_label ();

  testi (0, num_warnings);
  testi (0, num_errors);
  testi (0, num_criticals);
  testi (0, num_failures);
  
  return MAX(0, num_failures - 1);
}
