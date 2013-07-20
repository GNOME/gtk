#ifndef __EXAMPLEAPPWIN_H
#define __EXAMPLEAPPWIN_H

#include <gtk/gtk.h>
#include "exampleapp.h"


#define EXAMPLE_APP_WINDOW_TYPE (example_app_window_get_type ())
#define EXAMPLE_APP_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXAMPLE_APP_WINDOW_TYPE, ExampleAppWindow))


typedef struct ExampleAppWindow         ExampleAppWindow;
typedef struct ExampleAppWindowClass    ExampleAppWindowClass;


GType                   example_app_window_get_type     (void);
ExampleAppWindow       *example_app_window_new          (ExampleApp *app);
void                    example_app_window_open         (ExampleAppWindow *win,
                                                         GFile            *file);


#endif /* __EXAMPLEAPPWIN_H */
