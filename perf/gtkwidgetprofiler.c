#include "config.h"
#include <string.h>
#include "gtkwidgetprofiler.h"
#include "marshalers.h"
#include "typebuiltins.h"

typedef enum {
  STATE_NOT_CREATED,
  STATE_INSTRUMENTED_NOT_MAPPED,
  STATE_INSTRUMENTED_MAPPED
} State;

struct _GtkWidgetProfilerPrivate {
  State state;

  GtkWidget *profiled_widget;
  GtkWidget *toplevel;

  int n_iterations;

  GTimer *timer;

  gulong toplevel_expose_event_id;
  gulong toplevel_property_notify_event_id;

  GdkAtom profiler_atom;

  guint profiling : 1;
};

G_DEFINE_TYPE (GtkWidgetProfiler, gtk_widget_profiler, G_TYPE_OBJECT);

static void gtk_widget_profiler_finalize (GObject *object);

enum {
  CREATE_WIDGET,
  REPORT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
gtk_widget_profiler_class_init (GtkWidgetProfilerClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) class;

  signals[CREATE_WIDGET] =
    g_signal_new ("create-widget",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetProfilerClass, create_widget),
		  NULL, NULL,
		  _gtk_marshal_OBJECT__VOID,
		  G_TYPE_OBJECT, 0);

  signals[REPORT] =
    g_signal_new ("report",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetProfilerClass, report),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_OBJECT_DOUBLE,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_WIDGET_PROFILER_REPORT,
		  G_TYPE_OBJECT,
		  G_TYPE_DOUBLE);

  object_class->finalize = gtk_widget_profiler_finalize;
}

static void
gtk_widget_profiler_init (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;

  priv = g_new0 (GtkWidgetProfilerPrivate, 1);
  profiler->priv = priv;

  priv->state = STATE_NOT_CREATED;
  priv->n_iterations = 1;

  priv->timer = g_timer_new ();

  priv->profiler_atom = gdk_atom_intern ("GtkWidgetProfiler", FALSE);
}

static void
reset_state (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;

  priv = profiler->priv;

  if (priv->toplevel)
    {
      g_signal_handler_disconnect (priv->toplevel, priv->toplevel_expose_event_id);
      priv->toplevel_expose_event_id = 0;

      g_signal_handler_disconnect (priv->toplevel, priv->toplevel_property_notify_event_id);
      priv->toplevel_property_notify_event_id = 0;

      gtk_widget_destroy (priv->toplevel);
      priv->toplevel = NULL;
      priv->profiled_widget = NULL;
    }

  priv->state = STATE_NOT_CREATED;
}

static void
gtk_widget_profiler_finalize (GObject *object)
{
  GtkWidgetProfiler *profiler;
  GtkWidgetProfilerPrivate *priv;

  profiler = GTK_WIDGET_PROFILER (object);
  priv = profiler->priv;

  reset_state (profiler);
  g_timer_destroy (priv->timer);

  g_free (priv);

  G_OBJECT_CLASS (gtk_widget_profiler_parent_class)->finalize (object);
}

GtkWidgetProfiler *
gtk_widget_profiler_new (void)
{
  return g_object_new (GTK_TYPE_WIDGET_PROFILER, NULL);
}

void
gtk_widget_profiler_set_num_iterations (GtkWidgetProfiler *profiler,
					gint               n_iterations)
{
  GtkWidgetProfilerPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_PROFILER (profiler));
  g_return_if_fail (n_iterations > 0);

  priv = profiler->priv;
  priv->n_iterations = n_iterations;
}

static void
report (GtkWidgetProfiler      *profiler,
	GtkWidgetProfilerReport report,
	gdouble                 elapsed)
{
  GtkWidgetProfilerPrivate *priv;

  priv = profiler->priv;

  g_signal_emit (profiler, signals[REPORT], 0, report, priv->profiled_widget, elapsed);
}

static GtkWidget *
create_widget_via_emission (GtkWidgetProfiler *profiler)
{
  GtkWidget *widget;

  widget = NULL;
  g_signal_emit (profiler, signals[CREATE_WIDGET], 0, &widget);
  if (!widget)
    g_error ("The profiler emitted the \"create-widget\" signal but the signal handler returned no widget!");

  if (GTK_WIDGET_VISIBLE (widget) || GTK_WIDGET_MAPPED (widget))
    g_error ("The handler for \"create-widget\" must return an unmapped and unshown widget");

  return widget;
}

static gboolean
toplevel_property_notify_event_cb (GtkWidget *widget, GdkEventProperty *event, gpointer data)
{
  GtkWidgetProfiler *profiler;
  GtkWidgetProfilerPrivate *priv;
  gdouble elapsed;

  profiler = GTK_WIDGET_PROFILER (data);
  priv = profiler->priv;

  if (event->atom != priv->profiler_atom)
    return FALSE;

  /* Finish timing map/expose */

  elapsed = g_timer_elapsed (priv->timer, NULL);
  report (profiler, GTK_WIDGET_PROFILER_REPORT_EXPOSE, elapsed);

  gtk_main_quit (); /* This will get us back to the end of profile_map_expose() */
  return TRUE;
}

static gboolean
toplevel_idle_after_expose_cb (gpointer data)
{
  GtkWidgetProfiler *profiler;
  GtkWidgetProfilerPrivate *priv;

  profiler = GTK_WIDGET_PROFILER (data);
  priv = profiler->priv;

  gdk_property_change (priv->toplevel->window,
		       priv->profiler_atom,
		       gdk_atom_intern ("STRING", FALSE),
		       8,
		       GDK_PROP_MODE_REPLACE,
		       "hello",
		       strlen ("hello"));

  return FALSE;
}

static gboolean
toplevel_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GtkWidgetProfiler *profiler;

  profiler = GTK_WIDGET_PROFILER (data);

  g_idle_add_full (G_PRIORITY_HIGH, toplevel_idle_after_expose_cb, profiler, NULL);
  return FALSE;
}

static void
instrument_toplevel (GtkWidgetProfiler *profiler,
		     GtkWidget         *toplevel)
{
  GtkWidgetProfilerPrivate *priv;

  priv = profiler->priv;

  priv->toplevel_expose_event_id = g_signal_connect (toplevel, "expose-event",
						     G_CALLBACK (toplevel_expose_event_cb), profiler);

  gtk_widget_add_events (toplevel, GDK_PROPERTY_CHANGE_MASK);
  priv->toplevel_property_notify_event_id = g_signal_connect (toplevel, "property-notify-event",
							      G_CALLBACK (toplevel_property_notify_event_cb), profiler);
}

static GtkWidget *
ensure_and_get_toplevel (GtkWidget *widget)
{
	GtkWidget *toplevel;
	GtkWidget *window;

	toplevel = gtk_widget_get_toplevel (widget);
	if (GTK_WIDGET_TOPLEVEL (toplevel))
		return toplevel;

	g_assert (toplevel == widget); /* we don't want extraneous ancestors */

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (window), widget);

	return window;
}

static GtkWidget *
get_instrumented_toplevel (GtkWidgetProfiler *profiler,
			   GtkWidget         *widget)
{
  GtkWidget *window;

  window = ensure_and_get_toplevel (widget);
  instrument_toplevel (profiler, window);

  return window;
}

static void
profile_map_expose (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;
  gdouble elapsed;

  priv = profiler->priv;

  g_assert (priv->state == STATE_INSTRUMENTED_NOT_MAPPED);

  /* Time map.
   *
   * FIXME: we are really timing a show_all(); we don't really wait for all the "map_event" signals
   * to happen.  Should we rename GTK_WIDGET_PROFILER_REPORT_MAP report to something else?
   */

  g_timer_reset (priv->timer);

  gtk_widget_show_all (priv->toplevel);
  priv->state = STATE_INSTRUMENTED_MAPPED;

  elapsed = g_timer_elapsed (priv->timer, NULL);
  report (profiler, GTK_WIDGET_PROFILER_REPORT_MAP, elapsed);

  /* Time expose; this gets recorded in toplevel_property_notify_event_cb() */

  g_timer_reset (priv->timer);
  gtk_main ();
}

static void
profile_destroy (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;
  gdouble elapsed;

  priv = profiler->priv;

  g_assert (priv->state != STATE_NOT_CREATED);

  g_timer_reset (priv->timer);
  reset_state (profiler);
  elapsed = g_timer_elapsed (priv->timer, NULL);

  report (profiler, GTK_WIDGET_PROFILER_REPORT_DESTROY, elapsed);
}

static void
profile_boot (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;
  gdouble elapsed;

  priv = profiler->priv;

  g_assert (priv->state == STATE_NOT_CREATED);

  /* Time creation */

  g_timer_reset (priv->timer);

  priv->profiled_widget = create_widget_via_emission (profiler);
  priv->toplevel = get_instrumented_toplevel (profiler, priv->profiled_widget);

  priv->state = STATE_INSTRUMENTED_NOT_MAPPED;

  /* Here we include the time to anchor the widget to a toplevel, if
   * the toplevel was missing --- hopefully not a too-long extra time.
   */

  elapsed = g_timer_elapsed (priv->timer, NULL);
  report (profiler, GTK_WIDGET_PROFILER_REPORT_CREATE, elapsed);

  /* Start timing map/expose */

  profile_map_expose (profiler);

  /* Profile destruction */

  profile_destroy (profiler);
}

void
gtk_widget_profiler_profile_boot (GtkWidgetProfiler *profiler)
{
  GtkWidgetProfilerPrivate *priv;
  int i, n;

  g_return_if_fail (GTK_IS_WIDGET_PROFILER (profiler));

  priv = profiler->priv;
  g_return_if_fail (!priv->profiling);

  reset_state (profiler);
  priv->profiling = TRUE;

  n = priv->n_iterations;
  for (i = 0; i < n; i++)
    profile_boot (profiler);

  priv->profiling = FALSE;
}
