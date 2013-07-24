#ifndef __EXAMPLEAPPPREFS_H
#define __EXAMPLEAPPPREFS_H

#include <gtk/gtk.h>
#include "exampleappwin.h"


#define EXAMPLE_APP_PREFS_TYPE (example_app_prefs_get_type ())
#define EXAMPLE_APP_PREFS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXAMPLE_APP_PREFS_TYPE, ExampleAppPrefs))


typedef struct _ExampleAppPrefs          ExampleAppPrefs;
typedef struct _ExampleAppPrefsClass     ExampleAppPrefsClass;


GType                   example_app_prefs_get_type     (void);
ExampleAppPrefs        *example_app_prefs_new          (ExampleAppWindow *win);


#endif /* __EXAMPLEAPPPREFS_H */
