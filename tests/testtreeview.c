
#include <gtk/gtk.h>
#include <string.h>


static void
get_param_specs (GObject *object,
                 GParamSpec ***specs,
                 gint         *n_specs)
{
  /* Use private interface for now, fix later */
  *specs = G_OBJECT_GET_CLASS (object)->property_specs;
  *n_specs = G_OBJECT_GET_CLASS (object)->n_property_specs;
}

static void
g_object_connect_property (GObject *object,
                           const gchar *prop_name,
                           GtkSignalFunc func,
                           gpointer data)
{
  gchar *with_detail = g_strconcat ("notify::", prop_name, NULL);
  
  g_signal_connect_data (object, with_detail,
                         func, data,
                         NULL, FALSE, FALSE);

  g_free (with_detail);
}

typedef struct 
{
  GObject *obj;
  gchar *prop;
} ObjectProperty;

static void
free_object_property (ObjectProperty *p)
{
  g_free (p->prop);
  g_free (p);
}

static void
connect_controller (GObject *controller,
                    const gchar *signal,
                    GObject *model,
                    const gchar *prop_name,
                    GtkSignalFunc func)
{
  ObjectProperty *p;

  p = g_new (ObjectProperty, 1);
  p->obj = model;
  p->prop = g_strdup (prop_name);

  g_signal_connect_data (controller, signal, func, p,
                         (GDestroyNotify)free_object_property,
                         FALSE, FALSE);
}

static void
int_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (int) adj->value, NULL);
}

static void
int_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_INT);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_int (&val) != (int)adj->value)
    gtk_adjustment_set_value (adj, g_value_get_int (&val));

  g_value_unset (&val);
}


static void
string_modified (GtkEntry *entry, gpointer data)
{
  ObjectProperty *p = data;
  gchar *text;

  text = gtk_entry_get_text (entry);

  g_object_set (p->obj, p->prop, text, NULL);
}

static void
string_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = { 0, };  
  gchar *str;
  gchar *text;
  
  g_value_init (&val, G_TYPE_STRING);
  g_object_get_property (object, pspec->name, &val);

  str = g_value_get_string (&val);
  if (str == NULL)
    str = "";
  text = gtk_entry_get_text (entry);

  if (strcmp (str, text) != 0)
    gtk_entry_set_text (entry, str);
  
  g_value_unset (&val);
}

static void
bool_modified (GtkToggleButton *tb, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (int) tb->active, NULL);
}

static void
bool_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (data);
  GValue val = { 0, };  
  
  g_value_init (&val, G_TYPE_BOOLEAN);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_boolean (&val) != tb->active)
    gtk_toggle_button_set_active (tb, g_value_get_boolean (&val));

  gtk_label_set_text (GTK_LABEL (GTK_BIN (tb)->child), g_value_get_boolean (&val) ?
                      "TRUE" : "FALSE");
  
  g_value_unset (&val);
}

static GtkWidget*
create_prop_editor (GObject *object)
{
  GtkWidget *win;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *prop_edit;
  GtkWidget *sw;
  gint n_specs = 0;
  GParamSpec **specs = NULL;
  gint i;
  GtkAdjustment *adj;
  
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* hold a strong ref to the object we're editing */
  g_object_ref (G_OBJECT (object));
  g_object_set_data_full (G_OBJECT (win), "model-object",
                          object, (GDestroyNotify)g_object_unref);
  
  vbox = gtk_vbox_new (TRUE, 2);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);
  gtk_container_add (GTK_CONTAINER (win), sw);
  
  get_param_specs (object, &specs, &n_specs);
  
  i = 0;
  while (i < n_specs)
    {
      GParamSpec *spec = specs[i];
      gboolean can_modify;
      
      prop_edit = NULL;

      can_modify = ((spec->flags & G_PARAM_WRITABLE) != 0 &&
                    (spec->flags & G_PARAM_CONSTRUCT_ONLY) == 0);
      
      if ((spec->flags & G_PARAM_READABLE) == 0)
        {
          /* can't display unreadable properties */
          ++i;
          continue;
        }
      
      switch (spec->value_type)
        {
        case G_TYPE_INT:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
          adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_INT (spec)->default_value,
                                                    G_PARAM_SPEC_INT (spec)->minimum,
                                                    G_PARAM_SPEC_INT (spec)->maximum,
                                                    1,
                                                    MAX ((G_PARAM_SPEC_INT (spec)->maximum -
                                                          G_PARAM_SPEC_INT (spec)->minimum) / 10, 1),
                                                    0.0));

          prop_edit = gtk_spin_button_new (adj, 1.0, 0);
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (int_changed),
                                     adj);

          if (can_modify)
            connect_controller (G_OBJECT (adj), "value_changed",
                                object, spec->name, (GtkSignalFunc) int_modified);
          break;

        case G_TYPE_STRING:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_entry_new ();
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (string_changed),
                                     prop_edit);

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "changed",
                                object, spec->name, (GtkSignalFunc) string_modified);
          break;

        case G_TYPE_BOOLEAN:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_toggle_button_new_with_label ("");
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (bool_changed),
                                     prop_edit);

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "toggled",
                                object, spec->name, (GtkSignalFunc) bool_modified);
          break;

        default:
          {
            gchar *msg = g_strdup_printf ("%s: don't know how to edit type %s",
                                          spec->nick, g_type_name (spec->value_type));
            hbox = gtk_hbox_new (FALSE, 10);
            label = gtk_label_new (msg);            
            g_free (msg);
            gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
            gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
          }
          break;
        }

      if (prop_edit)
        {
          if (!can_modify)
            gtk_widget_set_sensitive (prop_edit, FALSE);
          
          /* set initial value */
          g_object_notify (object, spec->name);
        }
      
      ++i;
    }

  gtk_window_set_default_size (GTK_WINDOW (win), 300, 500);
  
  gtk_widget_show_all (win);

  return win;
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  /* I didn't write the tree test yet, just the property editor to use
   * inside the tree test ;-)
   */
  window = create_prop_editor (G_OBJECT (gtk_text_tag_new ("foo")));

  gtk_main ();

  return 0;
}
