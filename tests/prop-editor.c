/* prop-editor.c
 * Copyright (C) 2000  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include <gtk/gtk.h>

#include "prop-editor.h"

static void
get_param_specs (GType         type,
                 GParamSpec ***specs,
                 gint         *n_specs)
{
  GObjectClass *class = g_type_class_peek (type);

  /* We count on the fact we have an instance, or else we'd have
   * to use g_type_class_ref ();
   */
  
  /* Use private interface for now, fix later */
  *specs = class->property_specs;
  *n_specs = class->n_property_specs;
}

typedef struct
{
  gpointer instance;
  GObject *alive_object;
  guint id;
} DisconnectData;

static void
disconnect_func (gpointer data)
{
  DisconnectData *dd = data;
  
  g_signal_handler_disconnect (dd->instance, dd->id);
}

static void
signal_removed (gpointer  data,
		GClosure *closure)
{
  DisconnectData *dd = data;

  g_object_steal_data (dd->alive_object, "alive-object-data");
  g_free (dd);
}

static void
g_object_connect_property (GObject *object,
                           const gchar *prop_name,
                           GtkSignalFunc func,
                           gpointer data,
                           GObject *alive_object)
{
  GClosure *closure;
  gchar *with_detail = g_strconcat ("notify::", prop_name, NULL);
  DisconnectData *dd;

  dd = g_new (DisconnectData, 1);

  closure = g_cclosure_new (func, data, NULL);

  g_closure_add_invalidate_notifier (closure, dd, signal_removed);

  dd->id = g_signal_connect_closure (object, with_detail,
				     closure, FALSE);

  dd->instance = object;
  dd->alive_object = alive_object;
  
  g_object_set_data_full (G_OBJECT (alive_object),
                          "alive-object-data",
                          dd,
                          disconnect_func);
  
  g_free (with_detail);
}

typedef struct 
{
  GObject *obj;
  gchar *prop;
  gint modified_id;
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

  p->modified_id = g_signal_connect_data (controller, signal, func, p,
					  (GClosureNotify)free_object_property,
					  FALSE, FALSE);
  g_object_set_data (controller, "object-property", p);
}

static void
block_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  g_signal_handler_block (controller, p->modified_id);
}

static void
unblock_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  g_signal_handler_unblock (controller, p->modified_id);
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
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_int (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}

static void
float_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (float) adj->value, NULL);
}

static void
float_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_FLOAT);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_float (&val) != (float) adj->value)
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_float (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}

static void
string_modified (GtkEntry *entry, gpointer data)
{
  ObjectProperty *p = data;
  const gchar *text;

  text = gtk_entry_get_text (entry);

  g_object_set (p->obj, p->prop, text, NULL);
}

static void
string_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = { 0, };  
  const gchar *str;
  const gchar *text;
  
  g_value_init (&val, G_TYPE_STRING);
  g_object_get_property (object, pspec->name, &val);

  str = g_value_get_string (&val);
  if (str == NULL)
    str = "";
  text = gtk_entry_get_text (entry);

  if (strcmp (str, text) != 0)
    {
      block_controller (G_OBJECT (entry));
      gtk_entry_set_text (entry, str);
      unblock_controller (G_OBJECT (entry));
    }
  
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
    {
      block_controller (G_OBJECT (tb));
      gtk_toggle_button_set_active (tb, g_value_get_boolean (&val));
      unblock_controller (G_OBJECT (tb));
    }

  gtk_label_set_text (GTK_LABEL (GTK_BIN (tb)->child), g_value_get_boolean (&val) ?
                      "TRUE" : "FALSE");
  
  g_value_unset (&val);
}


static void
enum_modified (GtkOptionMenu *om, gpointer data)
{
  ObjectProperty *p = data;
  gint i;
  GParamSpec *spec;
  GEnumClass *eclass;
  
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (p->obj),
                                       p->prop);

  eclass = G_ENUM_CLASS (g_type_class_peek (spec->value_type));
  
  i = gtk_option_menu_get_history (om);

  g_object_set (p->obj, p->prop, eclass->values[i].value, NULL);
}

static void
enum_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkOptionMenu *om = GTK_OPTION_MENU (data);
  GValue val = { 0, };  
  GEnumClass *eclass;
  gint i;

  eclass = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));
  
  g_value_init (&val, pspec->value_type);
  g_object_get_property (object, pspec->name, &val);

  i = 0;
  while (i < eclass->n_values)
    {
      if (eclass->values[i].value == g_value_get_enum (&val))
        break;
      ++i;
    }
  
  if (gtk_option_menu_get_history (om) != i)
    {
      block_controller (G_OBJECT (om));
      gtk_option_menu_set_history (om, i);
      unblock_controller (G_OBJECT (om));
    }
  
  g_value_unset (&val);

}

static gunichar
unichar_get_value (GtkEntry *entry)
{
  const gchar *text = gtk_entry_get_text (entry);
  
  if (text[0])
    return g_utf8_get_char (text);
  else
    return 0;
}

static void
unichar_modified (GtkEntry *entry, gpointer data)
{
  ObjectProperty *p = data;
  gunichar val = unichar_get_value (entry);

  g_object_set (p->obj, p->prop, val, NULL);
}

static void
unichar_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  gunichar new_val;
  gunichar old_val = unichar_get_value (entry);
  gchar buf[7];
  gint len;

  g_object_get (object, pspec->name, &new_val, NULL);

  if (new_val != old_val)
    {
      if (!new_val)
	len = 0;
      else
	len = g_unichar_to_utf8 (new_val, buf);
      
      buf[len] = '\0';
      
      block_controller (G_OBJECT (entry));
      gtk_entry_set_text (entry, buf);
      unblock_controller (G_OBJECT (entry));
    }
}

void
model_destroy (gpointer data)
{
  g_object_steal_data (data, "model-object");
  gtk_widget_destroy (data);
}

void
window_destroy (gpointer data)
{
  g_object_steal_data (data, "prop-editor-win");
}

GtkWidget*
create_prop_editor (GObject *object,
		    GType    type)
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
  GtkTooltips *tips;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  tips = gtk_tooltips_new ();
  gtk_signal_connect_object (GTK_OBJECT (win), "destroy",
			     GTK_SIGNAL_FUNC (gtk_object_destroy), GTK_OBJECT (tips));

  /* hold a weak ref to the object we're editing */
  g_object_set_data_full (G_OBJECT (object), "prop-editor-win", win, model_destroy);
  g_object_set_data_full (G_OBJECT (win), "model-object", object, window_destroy);
  
  vbox = gtk_vbox_new (TRUE, 2);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);
  gtk_container_add (GTK_CONTAINER (win), sw);
  
  get_param_specs (type, &specs, &n_specs);
  
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
      
      switch (G_PARAM_SPEC_TYPE (spec))
        {
        case G_TYPE_PARAM_INT:
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
                                     adj, G_OBJECT (adj));

          if (can_modify)
            connect_controller (G_OBJECT (adj), "value_changed",
                                object, spec->name, (GtkSignalFunc) int_modified);
          break;

        case G_TYPE_PARAM_FLOAT:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
          adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_FLOAT (spec)->default_value,
                                                    G_PARAM_SPEC_FLOAT (spec)->minimum,
                                                    G_PARAM_SPEC_FLOAT (spec)->maximum,
                                                    0.1,
                                                    MAX ((G_PARAM_SPEC_FLOAT (spec)->maximum -
                                                          G_PARAM_SPEC_FLOAT (spec)->minimum) / 10, 0.1),
                                                    0.0));

          prop_edit = gtk_spin_button_new (adj, 0.1, 2);
          
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (float_changed),
                                     adj, G_OBJECT (adj));

          if (can_modify)
            connect_controller (G_OBJECT (adj), "value_changed",
                                object, spec->name, (GtkSignalFunc) float_modified);
          break;
          
        case G_TYPE_PARAM_STRING:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_entry_new ();
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (string_changed),
                                     prop_edit, G_OBJECT (prop_edit));

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "changed",
                                object, spec->name, (GtkSignalFunc) string_modified);
          break;

        case G_TYPE_PARAM_BOOLEAN:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

          prop_edit = gtk_toggle_button_new_with_label ("");
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (bool_changed),
                                     prop_edit, G_OBJECT (prop_edit));

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "toggled",
                                object, spec->name, (GtkSignalFunc) bool_modified);
          break;
          
        case G_TYPE_PARAM_ENUM:
	  {
	    GtkWidget *menu;
	    GEnumClass *eclass;
	    gint i;
            
	    hbox = gtk_hbox_new (FALSE, 10);
	    label = gtk_label_new (spec->nick);
	    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
            
	    prop_edit = gtk_option_menu_new ();
	    
	    menu = gtk_menu_new ();

	    eclass = G_ENUM_CLASS (g_type_class_ref (spec->value_type));
	    
	    i = 0;
	    while (i < eclass->n_values)
	      {
		GtkWidget *mi;
                
		mi = gtk_menu_item_new_with_label (eclass->values[i].value_name);
		
		gtk_widget_show (mi);
                
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
                
                  ++i;
	      }
	    
	    g_type_class_unref (eclass);
	    
	    gtk_option_menu_set_menu (GTK_OPTION_MENU (prop_edit), menu);
	    
	    gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
            
	    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 
            
	    g_object_connect_property (object, spec->name,
				       GTK_SIGNAL_FUNC (enum_changed),
				       prop_edit, G_OBJECT (prop_edit));
            
	    if (can_modify)
	      connect_controller (G_OBJECT (prop_edit), "changed",
				  object, spec->name, (GtkSignalFunc) enum_modified);
	  }
	  
        case G_TYPE_PARAM_UNICHAR:
          hbox = gtk_hbox_new (FALSE, 10);
          label = gtk_label_new (spec->nick);
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
          gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	  prop_edit = gtk_entry_new ();
	  gtk_entry_set_max_length (GTK_ENTRY (prop_edit), 1);
          gtk_box_pack_end (GTK_BOX (hbox), prop_edit, FALSE, FALSE, 0);
          
          gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); 

          g_object_connect_property (object, spec->name,
                                     GTK_SIGNAL_FUNC (unichar_changed),
                                     prop_edit, G_OBJECT (prop_edit));

          if (can_modify)
            connect_controller (G_OBJECT (prop_edit), "changed",
                                object, spec->name, (GtkSignalFunc) unichar_modified);
          break;

	default:
	  {
	    gchar *msg = g_strdup_printf ("%s: don't know how to edit property type %s",
					  spec->nick, g_type_name (G_PARAM_SPEC_TYPE (spec)));
	    hbox = gtk_hbox_new (FALSE, 10);
	    label = gtk_label_new (msg);            
	    g_free (msg);
	    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	  }
        }

      if (prop_edit)
        {
          if (!can_modify)
            gtk_widget_set_sensitive (prop_edit, FALSE);

	  if (spec->blurb)
	    gtk_tooltips_set_tip (tips, prop_edit, spec->blurb, NULL);
	  
          /* set initial value */
          g_object_notify (object, spec->name);
        }
      
      ++i;
    }

  gtk_window_set_default_size (GTK_WINDOW (win), 300, 500);
  
  gtk_widget_show_all (win);

  return win;
}

