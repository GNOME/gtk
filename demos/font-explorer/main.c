#include <gtk/gtk.h>
#include <fontexplorerapp.h>

int
main (int argc, char *argv[])
{
  return g_application_run (G_APPLICATION (font_explorer_app_new ()), argc, argv);
}
