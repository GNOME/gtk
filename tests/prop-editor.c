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
					  0);
  g_object_set_data (controller, "object-property", p);
}

static void
block_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  if (p)
    g_signal_handler_block (controller, p->modified_id);
}

static void
unblock_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  if (p)
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
uint_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (guint) adj->value, NULL);
}

static void
uint_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_UINT);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_uint (&val) != (guint)adj->value)
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_uint (&val));
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
double_modified (GtkAdjustment *adj, gpointer data)
{
  ObjectProperty *p = data;

  g_object_set (p->obj, p->prop, (double) adj->value, NULL);
}

static void
double_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = { 0, };  

  g_value_init (&val, G_TYPE_DOUBLE);
  g_object_get_property (object, pspec->name, &val);

  if (g_value_get_double (&val) != adj->value)
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_double (&val));
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

static void
pointer_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkLabel *label = GTK_LABEL (data);
  gchar *str;
  gpointer ptr;
  
  g_object_get (object, pspec->name, &ptr, NULL);

  str = g_strdup_printf ("Pointer: %p", ptr);
  gtk_label_set_text (label, str);
  g_free (str);
}

static void
object_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkLabel *label = GTK_LABEL (data);
  gchar *str;
  GObject *obj;
  const gchar *name;
  
  g_object_get (object, pspec->name, &obj, NULL);

  if (obj)
    name = g_type_name (G_TYPE_FROM_INSTANCE (obj));
  else
    name = "unknown";
  str = g_strdup_printf ("Object: %p (%s)", obj, name);
  
  gtk_label_set_text (label, str);
  g_free (str);
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

static GtkWidget *
property_widget (GObject *object, GParamSpec *spec, gboolean can_modify)
{
  GtkWidget *prop_edit;
  GtkAdjustment *adj;
  gchar *msg;
  
#if 0
  switch (G_PARAM_SPEC_TYPE (spec))
    {
    case G_TYPE_PARAM_INT:
      adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_INT (spec)->default_value,
						G_PARAM_SPEC_INT (spec)->minimum,
						G_PARAM_SPEC_INT (spec)->maximum,
						1,
						MAX ((G_PARAM_SPEC_INT (spec)->maximum -
						      G_PARAM_SPEC_INT (spec)->minimum) / 10, 1),
						0.0));
      
      prop_edit = gtk_spin_button_new (adj, 1.0, 0);
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (int_changed),
				 adj, G_OBJECT (adj));
      
      if (can_modify)
	connect_controller (G_OBJECT (adj), "value_changed",
			    object, spec->name, (GtkSignalFunc) int_modified);
      break;
      
    case G_TYPE_PARAM_UINT:
      adj = GTK_ADJUSTMENT (
	     gtk_adjustment_new (G_PARAM_SPEC_UINT (spec)->default_value,
				 G_PARAM_SPEC_UINT (spec)->minimum,
				 G_PARAM_SPEC_UINT (spec)->maximum,
				 1,
				 MAX ((G_PARAM_SPEC_UINT (spec)->maximum -
				       G_PARAM_SPEC_UINT (spec)->minimum) / 10, 1),
				 0.0));
      
      prop_edit = gtk_spin_button_new (adj, 1.0, 0);
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (uint_changed),
				 adj, G_OBJECT (adj));
      
      if (can_modify)
	connect_controller (G_OBJECT (adj), "value_changed",
			    object, spec->name, (GtkSignalFunc) uint_modified);
      break;
      
    case G_TYPE_PARAM_FLOAT:
      adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_FLOAT (spec)->default_value,
						G_PARAM_SPEC_FLOAT (spec)->minimum,
						G_PARAM_SPEC_FLOAT (spec)->maximum,
						0.1,
						MAX ((G_PARAM_SPEC_FLOAT (spec)->maximum -
						      G_PARAM_SPEC_FLOAT (spec)->minimum) / 10, 0.1),
						0.0));
      
      prop_edit = gtk_spin_button_new (adj, 0.1, 2);
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (float_changed),
				 adj, G_OBJECT (adj));
      
      if (can_modify)
	connect_controller (G_OBJECT (adj), "value_changed",
			    object, spec->name, (GtkSignalFunc) float_modified);
      break;
      
    case G_TYPE_PARAM_DOUBLE:
      adj = GTK_ADJUSTMENT (gtk_adjustment_new (G_PARAM_SPEC_DOUBLE (spec)->default_value,
						G_PARAM_SPEC_DOUBLE (spec)->minimum,
						G_PARAM_SPEC_DOUBLE (spec)->maximum,
						0.1,
						MAX ((G_PARAM_SPEC_DOUBLE (spec)->maximum -
						      G_PARAM_SPEC_DOUBLE (spec)->minimum) / 10, 0.1),
						0.0));
      
      prop_edit = gtk_spin_button_new (adj, 0.1, 2);
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (double_changed),
				 adj, G_OBJECT (adj));
      
      if (can_modify)
	connect_controller (G_OBJECT (adj), "value_changed",
			    object, spec->name, (GtkSignalFunc) double_modified);
      break;
      
    case G_TYPE_PARAM_STRING:
      prop_edit = gtk_entry_new ();
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (string_changed),
				 prop_edit, G_OBJECT (prop_edit));
      
      if (can_modify)
	connect_controller (G_OBJECT (prop_edit), "changed",
			    object, spec->name, (GtkSignalFunc) string_modified);
      break;
      
    case G_TYPE_PARAM_BOOLEAN:
      prop_edit = gtk_toggle_button_new_with_label ("");
      
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
	gint j;
	
	prop_edit = gtk_option_menu_new ();
	
	menu = gtk_menu_new ();
	
	eclass = G_ENUM_CLASS (g_type_class_ref (spec->value_type));
	
	j = 0;
	while (j < eclass->n_values)
	  {
	    GtkWidget *mi;
	    
	    mi = gtk_menu_item_new_with_label (eclass->values[j].value_name);
	    
	    gtk_widget_show (mi);
	    
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	    
	    ++j;
	  }
	
	g_type_class_unref (eclass);
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (prop_edit), menu);
	
	g_object_connect_property (object, spec->name,
				   GTK_SIGNAL_FUNC (enum_changed),
				   prop_edit, G_OBJECT (prop_edit));
	
	if (can_modify)
	  connect_controller (G_OBJECT (prop_edit), "changed",
			      object, spec->name, (GtkSignalFunc) enum_modified);
      }
      break;
      
    case G_TYPE_PARAM_UNICHAR:
      prop_edit = gtk_entry_new ();
      gtk_entry_set_max_length (GTK_ENTRY (prop_edit), 1);
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (unichar_changed),
				 prop_edit, G_OBJECT (prop_edit));
      
      if (can_modify)
	connect_controller (G_OBJECT (prop_edit), "changed",
			    object, spec->name, (GtkSignalFunc) unichar_modified);
      break;
      
    case G_TYPE_PARAM_POINTER:
      prop_edit = gtk_label_new ("");
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (pointer_changed),
				 prop_edit, G_OBJECT (prop_edit));
      break;
      
    case G_TYPE_PARAM_OBJECT:
      prop_edit = gtk_label_new ("");
      
      g_object_connect_property (object, spec->name,
				 GTK_SIGNAL_FUNC (object_changed),
				 prop_edit, G_OBJECT (prop_edit));
      break;
      
      
    default:
      msg = g_strdup_printf ("uneditable property type: %s",
			     g_type_name (G_PARAM_SPEC_TYPE (spec)));
      prop_edit = gtk_label_new (msg);            
      g_free (msg);
      gtk_misc_set_alignment (GTK_MISC (prop_edit), 0.0, 0.5);
    }
#endif
  
  return prop_edit;
}

static GtkWidget *
properties_from_type (GObject     *object,
		      GType        type,
		      GtkTooltips *tips)
{
  GtkWidget *prop_edit;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *vbox;
  GtkWidget *table;
  GObjectClass *class;
  GParamSpec **specs;
  gint n_specs;
  int i;

  class = G_OBJECT_CLASS (g_type_class_peek (type));
  specs = g_object_class_list_properties (class, &n_specs);
        
  if (n_specs == 0)
    return NULL;
  
  table = gtk_table_new (n_specs, 2, FALSE);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 10);
  gtk_table_set_row_spacings (GTK_TABLE (table), 3);

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
      
      if (spec->owner_type != type)
	{
	  /* we're only interested in params of type */
	  ++i;
	  continue;
	}

      label = gtk_label_new (g_param_spec_get_nick (spec));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
      gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, i, i + 1);
      
      prop_edit = property_widget (object, spec, can_modify);
      gtk_table_attach_defaults (GTK_TABLE (table), prop_edit, 1, 2, i, i + 1);

      if (prop_edit)
        {
          if (!can_modify)
            gtk_widget_set_sensitive (prop_edit, FALSE);

	  if (g_param_spec_get_blurb (spec))
	    gtk_tooltips_set_tip (tips, prop_edit, g_param_spec_get_blurb (spec), NULL);
	  
          /* set initial value */
          g_object_notify (object, spec->name);
        }
      
      ++i;
    }


  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);

  g_free (specs);

  return sw;
}


/* Pass zero for type if you want all properties */
GtkWidget*
create_prop_editor (GObject *object, GType type)
{
  GtkWidget *win;
  GtkWidget *notebook;
  GtkTooltips *tips;
  GtkWidget *properties;
  GtkWidget *label;
  gchar *title;

  if ((win = g_object_get_data (G_OBJECT (object), "prop-editor-win")))
    {
      gtk_window_present (GTK_WINDOW (win));
      return win;
    }

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  tips = gtk_tooltips_new ();
  gtk_signal_connect_object (GTK_OBJECT (win), "destroy",
			     GTK_SIGNAL_FUNC (gtk_object_destroy), GTK_OBJECT (tips));

  /* hold a weak ref to the object we're editing */
  g_object_set_data_full (G_OBJECT (object), "prop-editor-win", win, model_destroy);
  g_object_set_data_full (G_OBJECT (win), "model-object", object, window_destroy);

  if (type == 0)
    {
      notebook = gtk_notebook_new ();
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);
      
      gtk_container_add (GTK_CONTAINER (win), notebook);
      
      type = G_TYPE_FROM_INSTANCE (object);

      title = g_strdup_printf ("Properties of %s widget", g_type_name (type));
      gtk_window_set_title (GTK_WINDOW (win), title);
      g_free (title);
      
      while (type)
	{
	  properties = properties_from_type (object, type, tips);
	  if (properties)
	    {
	      label = gtk_label_new (g_type_name (type));
	      gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
					properties, label);
	    }
	  
	  type = g_type_parent (type);
	}
    }
  else
    {
      properties = properties_from_type (object, type, tips);
      gtk_container_add (GTK_CONTAINER (win), properties);
      title = g_strdup_printf ("Properties of %s", g_type_name (type));
      gtk_window_set_title (GTK_WINDOW (win), title);
    }
  
  gtk_window_set_default_size (GTK_WINDOW (win), -1, 400);
  
  gtk_widget_show_all (win);

  return win;
}

