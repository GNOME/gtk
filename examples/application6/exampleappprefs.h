#ifndef __EXAMPLEAPPPREFS_H
#define __EXAMPLEAPPPREFS_H

#include <gtk/gtk.h>
#include "exampleappwin.h"


#define EXAMPLE_TYPE_APP_PREFS (example_app_prefs_get_type ())
G_DECLARE_FINAL_TYPE (ExampleAppPrefs, example_app_prefs, EXAMPLE, APP_PREFS, GtkDialog)


ExampleAppPrefs        *example_app_prefs_new          (ExampleAppWindow *win);


#endif /* __EXAMPLEAPPPREFS_H */
