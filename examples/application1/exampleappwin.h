#ifndef __EXAMPLEAPPWIN_H
#define __EXAMPLEAPPWIN_H

#include <gtk/gtk.h>
#include "exampleapp.h"


#define EXAMPLE_TYPE_APP_WINDOW (example_app_window_get_type ())
G_DECLARE_FINAL_TYPE (ExampleAppWindow, example_app_window, EXAMPLE, APP_WINDOW, GtkApplicationWindow)


ExampleAppWindow       *example_app_window_new          (ExampleApp *app);
void                    example_app_window_open         (ExampleAppWindow *win,
                                                         GFile            *file);


#endif /* __EXAMPLEAPPWIN_H */
