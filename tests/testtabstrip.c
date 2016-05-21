#include <gtk/gtk.h>

typedef struct {
  GtkTab parent_instance;
  GtkWidget *label;
  GtkWidget *popover;
} MyTab;

typedef struct {
  GtkTabClass parent_class;
} MyTabClass;

G_DEFINE_TYPE (MyTab, my_tab, GTK_TYPE_TAB)

static void
close_tab (MyTab *tab)
{
  GtkWidget *widget;
  GtkWidget *stack;

  widget = gtk_tab_get_widget (GTK_TAB (tab));
  stack = gtk_widget_get_parent (widget);
  gtk_container_remove (GTK_CONTAINER (stack), widget);
}

static void
my_tab_init (MyTab *tab)
{
  GtkWidget *button;

  tab->label = gtk_label_new ("");
  gtk_widget_show (tab->label);
  gtk_widget_set_halign (tab->label, GTK_ALIGN_CENTER);

  gtk_tab_set_child (GTK_TAB (tab), tab->label);

  g_object_bind_property (tab, "title", tab->label, "label", G_BINDING_DEFAULT);

  tab->popover = gtk_popover_new (tab->label);
  button = g_object_new (GTK_TYPE_MODEL_BUTTON,
                         "text", "Close",
                         "visible", TRUE,
                         "margin", 10,
                         NULL);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (close_tab), tab);
  gtk_container_add (GTK_CONTAINER (tab->popover), button);
}

static gboolean
my_tab_button_press (GtkWidget      *widget,
                     GdkEventButton *event)
{
  MyTab *tab = (MyTab *)widget;

  if (event->button == GDK_BUTTON_SECONDARY)
    {
      gtk_widget_show (tab->popover);
      return TRUE;
    }

  return GTK_WIDGET_CLASS (my_tab_parent_class)->button_press_event (widget, event);
}

static void
my_tab_destroy (GtkWidget *widget)
{
  MyTab *tab = (MyTab *)widget;

  if (tab->popover)
    {
      gtk_widget_destroy (tab->popover);
      tab->popover = NULL;
    }

  GTK_WIDGET_CLASS (my_tab_parent_class)->destroy (widget);
}

static void
my_tab_class_init (MyTabClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->button_press_event = my_tab_button_press;
  widget_class->destroy = my_tab_destroy;
}

static GtkTab *
create_tab (GtkTabStrip *strip,
            GtkWidget   *widget,
            gpointer     data)
{
  return g_object_new (my_tab_get_type (), "widget", widget, NULL);
}

static void
add_stack_child (GtkWidget *stack)
{
  static int count;
  char *name;
  char *title;
  GtkWidget *sw, *tv;

  count++;

  name = g_strdup_printf ("tab%d", count);
  title = g_strdup_printf ("Page %d", count);

  sw = gtk_scrolled_window_new (NULL, NULL);
  tv = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)), title, -1);
  g_object_set (tv, "expand", TRUE, NULL);
  gtk_container_add (GTK_CONTAINER (sw), tv);
  gtk_widget_show_all (sw);

  gtk_stack_add_titled (GTK_STACK (stack), sw, name, title);

  g_free (name);
  g_free (title);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *tabs;
  GtkWidget *box;
  GtkWidget *stack;
  GtkWidget *button;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (window), vbox);

  stack = gtk_stack_new ();
  g_object_set (stack, "expand", TRUE, NULL);

  tabs = gtk_tab_strip_new ();
  gtk_tab_strip_set_closable (GTK_TAB_STRIP (tabs), TRUE);
  gtk_tab_strip_set_scrollable (GTK_TAB_STRIP (tabs), TRUE);
  gtk_tab_strip_set_stack (GTK_TAB_STRIP (tabs), GTK_STACK (stack));
  g_signal_connect (tabs, "create-tab", G_CALLBACK (create_tab), NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), tabs, TRUE, TRUE, 0);

  button = gtk_button_new_from_icon_name ("tab-new-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (add_stack_child), stack);

  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), stack, TRUE, TRUE, 0);

  add_stack_child (stack);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
