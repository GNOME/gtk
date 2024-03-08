#include "config.h"

#include "constraint-editor-application.h"
#include "constraint-editor-window.h"

struct _ConstraintEditorApplication {
  GtkApplication parent_instance;
};

G_DEFINE_TYPE(ConstraintEditorApplication, constraint_editor_application, GTK_TYPE_APPLICATION);

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       data)
{
  g_application_quit (G_APPLICATION (data));
}

static GActionEntry app_entries[] = {
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
constraint_editor_application_startup (GApplication *app)
{
  const char *quit_accels[2] = { "<Ctrl>Q", NULL };
  const char *open_accels[2] = { "<Ctrl>O", NULL };
  GtkCssProvider *provider;

  G_APPLICATION_CLASS (constraint_editor_application_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "app.quit", quit_accels);
  gtk_application_set_accels_for_action (GTK_APPLICATION (app), "win.open", open_accels);

  // Allow loading custom CSS files
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gtk/gtk4/constraint-editor/custom.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
constraint_editor_application_activate (GApplication *app)
{
  ConstraintEditorWindow *win;

  win = constraint_editor_window_new (CONSTRAINT_EDITOR_APPLICATION (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
constraint_editor_application_open (GApplication  *app,
                                    GFile        **files,
                                    int            n_files,
                                    const char    *hint)
{
  ConstraintEditorWindow *win;
  int i;

  for (i = 0; i < n_files; i++)
    {
      // Check if file is valid before opening
      if (is_valid_constraint_file(files[i])) {
        win = constraint_editor_window_new (CONSTRAINT_EDITOR_APPLICATION (app));
        constraint_editor_window_load (win, files[i]);
        gtk_window_present (GTK_WINDOW (win));
      } else {
        // Display error message for invalid file
        GtkWidget *error_dialog = gtk_message_dialog_new(NULL,
                                                          GTK_DIALOG_MODAL,
                                                          GTK_MESSAGE_ERROR,
                                                          GTK_BUTTONS_CLOSE,
                                                          "Invalid constraint file.");
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
      }
    }
}

static void
constraint_editor_application_class_init (ConstraintEditorApplicationClass *class)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  application_class->startup = constraint_editor_application_startup;
  application_class->activate = constraint_editor_application_activate;
  application_class->open = constraint_editor_application_open;
}

ConstraintEditorApplication *
constraint_editor_application_new (void)
{
  return g_object_new (CONSTRAINT_EDITOR_APPLICATION_TYPE,
                       "application-id", "org.gtk.gtk4.ConstraintEditor",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

// Check if the file is a valid constraint file
static gboolean is_valid_constraint_file(GFile *file) {
  // Add your validation logic here
  // For example, check file extension or format
  return TRUE; // Placeholder, always return true for now
}
