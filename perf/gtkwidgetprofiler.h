#include <gtk/gtk.h>

#ifndef GTK_WIDGET_PROFILER_H
#define GTK_WIDGET_PROFILER_H

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_PROFILER		(gtk_widget_profiler_get_type ())
#define GTK_WIDGET_PROFILER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WIDGET_PROFILER, GtkWidgetProfiler))
#define GTK_WIDGET_PROFILER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WIDGET_PROFILER, GtkWidgetProfilerClass))
#define GTK_IS_WIDGET_PROFILER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WIDGET_PROFILER))
#define GTK_IS_WIDGET_PROFILER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WIDGET_PROFILER))
#define GTK_WIDGET_PROFILER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WIDGET_PROFILER, GtkWidgetProfilerClass))

typedef enum
{
  GTK_WIDGET_PROFILER_REPORT_CREATE,
  GTK_WIDGET_PROFILER_REPORT_MAP,
  GTK_WIDGET_PROFILER_REPORT_EXPOSE,
  GTK_WIDGET_PROFILER_REPORT_DESTROY
} GtkWidgetProfilerReport;

typedef struct _GtkWidgetProfiler GtkWidgetProfiler;
typedef struct _GtkWidgetProfilerClass GtkWidgetProfilerClass;
typedef struct _GtkWidgetProfilerPrivate GtkWidgetProfilerPrivate;

struct _GtkWidgetProfiler {
	GObject object;

	GtkWidgetProfilerPrivate *priv;
};

struct _GtkWidgetProfilerClass {
	GObjectClass parent_class;

	/* signals */

	GtkWidget *(* create_widget) (GtkWidgetProfiler *profiler);

	void (* report) (GtkWidgetProfiler      *profiler,
			 GtkWidgetProfilerReport report,
			 GtkWidget              *widget,
			 gdouble                 elapsed);
};

GType gtk_widget_profiler_get_type (void) G_GNUC_CONST;

GtkWidgetProfiler *gtk_widget_profiler_new (void);

void gtk_widget_profiler_set_num_iterations (GtkWidgetProfiler *profiler,
					     gint               n_iterations);

void gtk_widget_profiler_profile_boot (GtkWidgetProfiler *profiler);

void gtk_widget_profiler_profile_expose (GtkWidgetProfiler *profiler);


G_END_DECLS

#endif
