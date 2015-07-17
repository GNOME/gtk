#include <gtk/gtk.h>

static void
show_message_dialog1 (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = GTK_WIDGET (gtk_message_dialog_new (parent,
                                               GTK_DIALOG_MODAL|
                                               GTK_DIALOG_DESTROY_WITH_PARENT|
                                               GTK_DIALOG_USE_HEADER_BAR,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "Oops! Something went wrong."));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "Unhandled error message: SSH program unexpectedly exited");

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_message_dialog1a (GtkWindow *parent)
{
  GtkWidget *dialog;
  GtkWidget *image;

  dialog = GTK_WIDGET (gtk_message_dialog_new (parent,
                                               GTK_DIALOG_MODAL|
                                               GTK_DIALOG_DESTROY_WITH_PARENT|
                                               GTK_DIALOG_USE_HEADER_BAR,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_OK,
                                               "The system network services are not compatible with this version."));

  image = gtk_image_new_from_icon_name ("computer-fail", GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (image);
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
  G_GNUC_END_IGNORE_DEPRECATIONS;


  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_message_dialog2 (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = GTK_WIDGET (gtk_message_dialog_new (parent,
                                               GTK_DIALOG_MODAL|
                                               GTK_DIALOG_DESTROY_WITH_PARENT|
                                               GTK_DIALOG_USE_HEADER_BAR,
                                               GTK_MESSAGE_INFO,
                                               GTK_BUTTONS_NONE,
                                               "Empty all items from Wastebasket?"));
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "All items in the Wastebasket will be permanently deleted");
  gtk_dialog_add_buttons (GTK_DIALOG (dialog), 
                          "Cancel", GTK_RESPONSE_CANCEL,
                          "Empty Wastebasket", GTK_RESPONSE_OK,
                          NULL);  

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_color_chooser (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = gtk_color_chooser_dialog_new ("Builtin", parent);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_color_chooser_generic (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = g_object_new (GTK_TYPE_COLOR_CHOOSER_DIALOG,
                         "title", "Generic Builtin",
                         "transient-for", parent,
                         NULL);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
add_content (GtkWidget *dialog)
{
  GtkWidget *label;

  label = gtk_label_new ("content");
  g_object_set (label, "margin", 50, NULL);
  gtk_widget_show (label);

  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), label);
}

static void
add_buttons (GtkWidget *dialog)
{
  gtk_dialog_add_button (GTK_DIALOG (dialog), "Done", GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
show_dialog (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons ("Simple", parent, 
					GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				        "Close", GTK_RESPONSE_CLOSE,
                                        NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_dialog_with_header (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons ("With Header", parent, 
					GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_USE_HEADER_BAR,
				        "Close", GTK_RESPONSE_CLOSE,
                                        NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_dialog_with_buttons (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons ("With Buttons", parent, 
					GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
				        "Close", GTK_RESPONSE_CLOSE,
				        "Frob", 25,
                                        NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_dialog_with_header_buttons (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = gtk_dialog_new_with_buttons ("Header & Buttons", parent, 
					GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_USE_HEADER_BAR,
				        "Close", GTK_RESPONSE_CLOSE,
				        "Frob", 25,
                                        NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_dialog_with_header_buttons2 (GtkWindow *parent)
{
  GtkBuilder *builder;
  GtkWidget *dialog;

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, "dialog.ui", NULL);
  dialog = (GtkWidget *)gtk_builder_get_object (builder, "dialog");
  g_object_unref (builder);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

typedef struct {
  GtkDialog parent;
} MyDialog;

typedef struct {
  GtkDialogClass parent_class;
} MyDialogClass;

G_DEFINE_TYPE (MyDialog, my_dialog, GTK_TYPE_DIALOG);

static void
my_dialog_init (MyDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

static void
my_dialog_class_init (MyDialogClass *class)
{
  gchar *buffer;
  gsize size;
  GBytes *bytes;

  if (!g_file_get_contents ("mydialog.ui", &buffer, &size, NULL))
    g_error ("Template file mydialog.ui not found");

  bytes = g_bytes_new_static (buffer, size);
  gtk_widget_class_set_template (GTK_WIDGET_CLASS (class), bytes);
  g_bytes_unref (bytes);
}

static void
show_dialog_from_template (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = g_object_new (my_dialog_get_type (),
                         "title", "Template",
                         "transient-for", parent,
                         NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
show_dialog_flex_template (GtkWindow *parent)
{
  GtkWidget *dialog;
  gboolean use_header;

  g_object_get (gtk_settings_get_default (),
                "gtk-dialogs-use-header", &use_header,
                NULL);
  dialog = g_object_new (my_dialog_get_type (),
                         "title", "Flexible Template",
                         "transient-for", parent,
                         "use-header-bar", use_header,
                         NULL);

  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

typedef struct {
  GtkDialog parent;

  GtkWidget *content;
} MyDialog2;

typedef struct {
  GtkDialogClass parent_class;
} MyDialog2Class;

G_DEFINE_TYPE (MyDialog2, my_dialog2, GTK_TYPE_DIALOG);

static void
my_dialog2_init (MyDialog2 *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

static void
my_dialog2_class_init (MyDialog2Class *class)
{
  gchar *buffer;
  gsize size;
  GBytes *bytes;

  if (!g_file_get_contents ("mydialog2.ui", &buffer, &size, NULL))
    g_error ("Template file mydialog2.ui not found");

  bytes = g_bytes_new_static (buffer, size);
  gtk_widget_class_set_template (GTK_WIDGET_CLASS (class), bytes);
  g_bytes_unref (bytes);

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), MyDialog2, content);
}

static void
show_dialog_from_template_with_header (GtkWindow *parent)
{
  GtkWidget *dialog;

  dialog = g_object_new (my_dialog2_get_type (),
                         "transient-for", parent,
                         "use-header-bar", TRUE,
                         NULL);

  add_buttons (dialog);
  add_content (dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *button;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
  gtk_widget_set_valign (vbox, GTK_ALIGN_CENTER);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  box = gtk_flow_box_new ();
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_NONE);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  button = gtk_button_new_with_label ("Message dialog");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_message_dialog1), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Message with icon");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_message_dialog1a), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Confirmation dialog");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_message_dialog2), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Builtin");
  button = gtk_button_new_with_label ("Builtin");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_color_chooser), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Generic Builtin");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_color_chooser_generic), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Simple");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("With Header");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_with_header), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("With Buttons");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_with_buttons), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Header & Buttons");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_with_header_buttons), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Header & Buttons & Builder");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_with_header_buttons2), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Template");
  button = gtk_button_new_with_label ("Template");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_from_template), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Template With Header");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_from_template_with_header), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_button_new_with_label ("Flexible Template");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (show_dialog_flex_template), window);
  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  button = gtk_check_button_new_with_label ("Dialogs have headers");
  g_object_bind_property (gtk_settings_get_default (), "gtk-dialogs-use-header",
                          button, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
  gtk_widget_show (button);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  button = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (button));
  gtk_widget_show (button);
  gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), button);

  gtk_widget_show (window);
  gtk_main ();
  
  return 0;
}

