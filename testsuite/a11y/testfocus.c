#include <gtk/gtk.h>

static const gchar *
get_name (gpointer obj)
{
  GtkWidget *widget;
  if (obj == NULL)
    return "(nil)";
  else if (GTK_IS_WIDGET (obj))
    widget = GTK_WIDGET (obj);
  else if (GTK_IS_ACCESSIBLE (obj))
    widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  else
    return "OOPS";
  if (GTK_IS_BUILDABLE (widget))
    return gtk_buildable_get_name (GTK_BUILDABLE (widget));
  else
    return G_OBJECT_TYPE_NAME (widget);
}

static gboolean
compare_focus (gpointer data)
{
  AtkObject *atk_focus;
  AtkObject *gtk_focus;
  GtkWidget *focus_widget;
  GList *list, *l;

  atk_focus = atk_get_focus_object ();

  focus_widget = NULL;
  list = gtk_window_list_toplevels ();
  for (l = list; l; l = l->next)
    {
      GtkWindow *w = l->data;
      if (gtk_window_is_active (w))
        {
          focus_widget = gtk_window_get_focus (w);
          break;
        }
    }
  g_list_free (list);

  if (GTK_IS_WIDGET (focus_widget))
    gtk_focus = gtk_widget_get_accessible (focus_widget);
  else
    gtk_focus = NULL;

  if (gtk_focus != atk_focus)
    g_print ("gtk focus: %s != atk focus: %s\n",
             get_name (gtk_focus), get_name (atk_focus));

  return G_SOURCE_CONTINUE;
}

static void
notify_cb (GObject *obj, GParamSpec *pspec, gpointer data)
{
  gboolean value;

  if (g_strcmp0 (pspec->name, "has-focus") != 0)
    return;

  g_object_get (obj, "has-focus", &value, NULL);
  g_print ("widget %s %p has-focus -> %d\n", get_name (obj), obj, value);
}

static void
state_change_cb (AtkObject *obj, const gchar *name, gboolean state_set)
{
  AtkStateSet *set;

  set = atk_object_ref_state_set (obj);
  g_print ("accessible %s %p focused -> %d\n", get_name (obj), obj,
           atk_state_set_contains_state (set, ATK_STATE_FOCUSED));
  g_object_unref (set);
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget *window;
  GSList *o, *l;
  GtkWidget *widget;
  AtkObject *accessible;

  gtk_init (&argc, &argv);

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, argv[1], NULL);

  window = (GtkWidget *)gtk_builder_get_object (builder, "window1");

  o = gtk_builder_get_objects (builder);
  for (l = o; l;l = l->next)
    {
       if (!GTK_IS_WIDGET (l->data))
         continue;

       widget = l->data;
       g_signal_connect (widget, "notify::has-focus", G_CALLBACK (notify_cb), NULL);
       accessible = gtk_widget_get_accessible (widget);
       g_signal_connect (accessible, "state-change::focused", G_CALLBACK (state_change_cb), NULL);

    }
  g_slist_free (o);

  g_timeout_add (100, compare_focus, NULL);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}

