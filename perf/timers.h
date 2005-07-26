#include <gtk/gtkwidget.h>

typedef enum
{
  TIMER_REPORT_WIDGET_CREATION,
  TIMER_REPORT_WIDGET_SHOW,
  TIMER_REPORT_WIDGET_DESTRUCTION
} TimerReport;

typedef GtkWidget *(* TimerWidgetCreateFunc) (gpointer user_data);

typedef void (* TimerReportFunc) (TimerReport report, gdouble elapsed, gpointer user_data);

void timer_time_widget (TimerWidgetCreateFunc create_func,
			TimerReportFunc       report_func,
			gpointer              user_data);
