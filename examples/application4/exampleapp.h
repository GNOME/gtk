#ifndef __EXAMPLEAPP_H
#define __EXAMPLEAPP_H

#include <gtk/gtk.h>


#define EXAMPLE_APP_TYPE (example_app_get_type ())
#define EXAMPLE_APP(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXAMPLE_APP_TYPE, ExampleApp))


typedef struct _ExampleApp       ExampleApp;
typedef struct _ExampleAppClass  ExampleAppClass;


GType           example_app_get_type    (void);
ExampleApp     *example_app_new         (void);


#endif /* __EXAMPLEAPP_H */
