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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <gtk/gtk.h>

#include "prop-editor.h"


typedef struct
{
  gpointer instance;
  GObject *alive_object;
  gulong id;
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

static gboolean
is_child_property (GParamSpec *pspec)
{
  return g_param_spec_get_qdata (pspec, g_quark_from_string ("is-child-prop")) != NULL;
}

static void
mark_child_property (GParamSpec *pspec)
{
  g_param_spec_set_qdata (pspec, g_quark_from_string ("is-child-prop"),
                          GINT_TO_POINTER (TRUE));
}

static void
g_object_connect_property (GObject     *object,
                           GParamSpec  *spec,
                           GCallback    func,
                           gpointer     data,
                           GObject     *alive_object)
{
  GClosure *closure;
  gchar *with_detail;
  DisconnectData *dd;

  if (is_child_property (spec))
    with_detail = g_strconcat ("child-notify::", spec->name, NULL);
  else
    with_detail = g_strconcat ("notify::", spec->name, NULL);

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
  GParamSpec *spec;
  gulong modified_id;
} ObjectProperty;

static void
free_object_property (ObjectProperty *p)
{
  g_free (p);
}

static void
connect_controller (GObject     *controller,
                    const gchar *signal,
                    GObject     *model,
                    GParamSpec  *spec,
                    GCallback    func)
{
  ObjectProperty *p;

  p = g_new (ObjectProperty, 1);
  p->obj = model;
  p->spec = spec;

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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, (int) gtk_adjustment_get_value (adj), NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, (int) gtk_adjustment_get_value (adj), NULL);
}

static void
get_property_value (GObject *object, GParamSpec *pspec, GValue *value)
{
  if (is_child_property (pspec))
    {
      GtkWidget *widget = GTK_WIDGET (object);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_get_property (GTK_CONTAINER (parent),
                                        widget, pspec->name, value);
    }
  else
    g_object_get_property (object, pspec->name, value);
}

static void
int_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_INT);

  get_property_value (object, pspec, &val);

  if (g_value_get_int (&val) != (int)gtk_adjustment_get_value (adj))
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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, (guint) gtk_adjustment_get_value (adj), NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, (guint) gtk_adjustment_get_value (adj), NULL);
}

static void
uint_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_UINT);
  get_property_value (object, pspec, &val);

  if (g_value_get_uint (&val) != (guint)gtk_adjustment_get_value (adj))
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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, (float) gtk_adjustment_get_value (adj), NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, (float) gtk_adjustment_get_value (adj), NULL);
}

static void
float_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_FLOAT);
  get_property_value (object, pspec, &val);

  if (g_value_get_float (&val) != (float) gtk_adjustment_get_value (adj))
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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, (double) gtk_adjustment_get_value (adj), NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, (double) gtk_adjustment_get_value (adj), NULL);
}

static void
double_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_DOUBLE);
  get_property_value (object, pspec, &val);

  if (g_value_get_double (&val) != gtk_adjustment_get_value (adj))
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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, text, NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, text, NULL);
}

static void
string_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = G_VALUE_INIT;
  const gchar *str;
  const gchar *text;

  g_value_init (&val, G_TYPE_STRING);
  get_property_value (object, pspec, &val);

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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent), widget,
                               p->spec->name, (int) gtk_toggle_button_get_active (tb),
                               NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, (int) gtk_toggle_button_get_active (tb), NULL);
}

static void
bool_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkToggleButton *tb = GTK_TOGGLE_BUTTON (data);
  GtkWidget *child;
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_BOOLEAN);
  get_property_value (object, pspec, &val);

  if (g_value_get_boolean (&val) != gtk_toggle_button_get_active (tb))
    {
      block_controller (G_OBJECT (tb));
      gtk_toggle_button_set_active (tb, g_value_get_boolean (&val));
      unblock_controller (G_OBJECT (tb));
    }

  child = gtk_bin_get_child (GTK_BIN (tb));
  gtk_label_set_text (GTK_LABEL (child),
                      g_value_get_boolean (&val) ? "TRUE" : "FALSE");

  g_value_unset (&val);
}


static void
enum_modified (GtkComboBox *cb, gpointer data)
{
  ObjectProperty *p = data;
  gint i;
  GEnumClass *eclass;

  eclass = G_ENUM_CLASS (g_type_class_peek (p->spec->value_type));

  i = gtk_combo_box_get_active (cb);


  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, eclass->values[i].value, NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, eclass->values[i].value, NULL);
}

static void
enum_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkComboBox *cb = GTK_COMBO_BOX (data);
  GValue val = G_VALUE_INIT;
  GEnumClass *eclass;
  gint i;

  eclass = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);

  i = 0;
  while (i < eclass->n_values)
    {
      if (eclass->values[i].value == g_value_get_enum (&val))
        break;
      ++i;
    }

  if (gtk_combo_box_get_active (cb) != i)
    {
      block_controller (G_OBJECT (cb));
      gtk_combo_box_set_active (cb, i);
      unblock_controller (G_OBJECT (cb));
    }

  g_value_unset (&val);

}

static void
flags_modified (GtkCheckButton *button, gpointer data)
{
  ObjectProperty *p = data;
  gboolean active;
  GFlagsClass *fclass;
  guint flags;
  gint i;

  fclass = G_FLAGS_CLASS (g_type_class_peek (p->spec->value_type));

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "index"));

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_get (GTK_CONTAINER (parent),
                               widget, p->spec->name, &flags, NULL);
      if (active)
        flags |= fclass->values[i].value;
      else
        flags &= ~fclass->values[i].value;

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, flags, NULL);
    }
  else
    {
      g_object_get (p->obj, p->spec->name, &flags, NULL);

      if (active)
        flags |= fclass->values[i].value;
      else
        flags &= ~fclass->values[i].value;

      g_object_set (p->obj, p->spec->name, flags, NULL);
    }
}

static void
flags_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GList *children, *c;
  GValue val = G_VALUE_INIT;
  GFlagsClass *fclass;
  guint flags;
  gint i;

  fclass = G_FLAGS_CLASS (g_type_class_peek (pspec->value_type));

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);
  flags = g_value_get_flags (&val);
  g_value_unset (&val);

  children = gtk_container_get_children (GTK_CONTAINER (data));

  for (c = children, i = 0; c; c = c->next, i++)
    {
      block_controller (G_OBJECT (c->data));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c->data),
                                    (fclass->values[i].value & flags) != 0);
      unblock_controller (G_OBJECT (c->data));
    }

  g_list_free (children);
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

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, val, NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, val, NULL);
}

static void
unichar_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  gunichar new_val;
  gunichar old_val = unichar_get_value (entry);
  GValue val = G_VALUE_INIT;
  gchar buf[7];
  gint len;

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);
  new_val = (gunichar)g_value_get_uint (&val);

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

static gchar *
object_label (GObject *obj, GParamSpec *pspec)
{
  const gchar *name;

  if (obj)
    name = g_type_name (G_TYPE_FROM_INSTANCE (obj));
  else if (pspec)
    name = g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec));
  else
    name = "unknown";
  return g_strdup_printf ("Object: %p (%s)", obj, name);
}

static void
object_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkWidget *label, *button;
  gchar *str;
  GObject *obj;

  GList *children = gtk_container_get_children (GTK_CONTAINER (data));
  label = GTK_WIDGET (children->data);
  button = GTK_WIDGET (children->next->data);
  g_object_get (object, pspec->name, &obj, NULL);
  g_list_free (children);

  str = object_label (obj, pspec);

  gtk_label_set_text (GTK_LABEL (label), str);
  gtk_widget_set_sensitive (button, G_IS_OBJECT (obj));

  if (obj)
    g_object_unref (obj);

  g_free (str);
}

static void
model_destroy (gpointer data)
{
  g_object_steal_data (data, "model-object");
  gtk_widget_destroy (data);
}

static void
window_destroy (gpointer data)
{
  g_object_steal_data (data, "prop-editor-win");
}

static void
object_properties (GtkWidget *button,
                   GObject   *object)
{
  gchar *name;
  GObject *obj;

  name = (gchar *) g_object_get_data (G_OBJECT (button), "property-name");
  g_object_get (object, name, &obj, NULL);
  if (G_IS_OBJECT (obj))
    create_prop_editor (obj, 0);
}

static void
rgba_modified (GtkColorButton *cb, gpointer data)
{
  ObjectProperty *p = data;
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cb), &color);

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, &color, NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, &color, NULL);
}

static void
rgba_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkColorButton *cb = GTK_COLOR_BUTTON (data);
  GValue val = G_VALUE_INIT;
  GdkRGBA *color;
  GdkRGBA cb_color;

  g_value_init (&val, GDK_TYPE_RGBA);
  get_property_value (object, pspec, &val);

  color = g_value_get_boxed (&val);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cb), &cb_color);

  if (color != NULL && !gdk_rgba_equal (color, &cb_color))
    {
      block_controller (G_OBJECT (cb));
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cb), color);
      unblock_controller (G_OBJECT (cb));
    }

  g_value_unset (&val);
}

static void
color_modified (GtkColorButton *cb, gpointer data)
{
  ObjectProperty *p = data;
  GValue val = G_VALUE_INIT;

  g_value_init (&val, GDK_TYPE_COLOR);
  g_object_get_property (G_OBJECT (cb), "color", &val);

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set_property (GTK_CONTAINER (parent),
			                widget, p->spec->name, &val);
    }
  else
    g_object_set_property (p->obj, p->spec->name, &val);

  g_value_unset (&val);
}

static void
color_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkColorButton *cb = GTK_COLOR_BUTTON (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, GDK_TYPE_COLOR);
  get_property_value (object, pspec, &val);

  if (g_value_get_boxed (&val))
    {
      block_controller (G_OBJECT (cb));
      g_object_set_property (G_OBJECT (cb), "color", &val);
      unblock_controller (G_OBJECT (cb));
    }

  g_value_unset (&val);
}

static void
font_modified (GtkFontChooser *fb, GParamSpec *pspec, gpointer data)
{
  ObjectProperty *p = data;
  PangoFontDescription *font_desc;

  font_desc = gtk_font_chooser_get_font_desc (fb);

  if (is_child_property (p->spec))
    {
      GtkWidget *widget = GTK_WIDGET (p->obj);
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_container_child_set (GTK_CONTAINER (parent),
                               widget, p->spec->name, font_desc, NULL);
    }
  else
    g_object_set (p->obj, p->spec->name, font_desc, NULL);

  pango_font_description_free (font_desc);
}

static void
font_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkFontChooser *fb = GTK_FONT_CHOOSER (data);
  GValue val = G_VALUE_INIT;
  const PangoFontDescription *font_desc;
  PangoFontDescription *fb_font_desc;

  g_value_init (&val, PANGO_TYPE_FONT_DESCRIPTION);
  get_property_value (object, pspec, &val);

  font_desc = g_value_get_boxed (&val);
  fb_font_desc = gtk_font_chooser_get_font_desc (fb);

  if (font_desc == NULL ||
      (fb_font_desc != NULL &&
       !pango_font_description_equal (fb_font_desc, font_desc)))
    {
      block_controller (G_OBJECT (fb));
      gtk_font_chooser_set_font_desc (fb, font_desc);
      unblock_controller (G_OBJECT (fb));
    }

  g_value_unset (&val);
  pango_font_description_free (fb_font_desc);
}


static GtkWidget *
property_widget (GObject    *object,
                 GParamSpec *spec,
                 gboolean    can_modify)
{
  GtkWidget *prop_edit;
  GtkAdjustment *adj;
  gchar *msg;
  GType type = G_PARAM_SPEC_TYPE (spec);

  if (type == G_TYPE_PARAM_INT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_INT (spec)->default_value,
                                G_PARAM_SPEC_INT (spec)->minimum,
                                G_PARAM_SPEC_INT (spec)->maximum,
                                1,
                                MAX ((G_PARAM_SPEC_INT (spec)->maximum - G_PARAM_SPEC_INT (spec)->minimum) / 10, 1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 1.0, 0);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (int_changed),
                                 adj, G_OBJECT (adj));

      if (can_modify)
        connect_controller (G_OBJECT (adj), "value_changed",
                            object, spec, G_CALLBACK (int_modified));
    }
  else if (type == G_TYPE_PARAM_UINT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_UINT (spec)->default_value,
                                G_PARAM_SPEC_UINT (spec)->minimum,
                                G_PARAM_SPEC_UINT (spec)->maximum,
                                1,
                                MAX ((G_PARAM_SPEC_UINT (spec)->maximum - G_PARAM_SPEC_UINT (spec)->minimum) / 10, 1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 1.0, 0);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (uint_changed),
                                 adj, G_OBJECT (adj));

      if (can_modify)
        connect_controller (G_OBJECT (adj), "value_changed",
                            object, spec, G_CALLBACK (uint_modified));
    }
  else if (type == G_TYPE_PARAM_FLOAT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_FLOAT (spec)->default_value,
                                G_PARAM_SPEC_FLOAT (spec)->minimum,
                                G_PARAM_SPEC_FLOAT (spec)->maximum,
                                0.1,
                                MAX ((G_PARAM_SPEC_FLOAT (spec)->maximum - G_PARAM_SPEC_FLOAT (spec)->minimum) / 10, 0.1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 0.1, 2);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (float_changed),
                                 adj, G_OBJECT (adj));

      if (can_modify)
        connect_controller (G_OBJECT (adj), "value_changed",
                            object, spec, G_CALLBACK (float_modified));
    }
  else if (type == G_TYPE_PARAM_DOUBLE)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_DOUBLE (spec)->default_value,
                                G_PARAM_SPEC_DOUBLE (spec)->minimum,
                                G_PARAM_SPEC_DOUBLE (spec)->maximum,
                                0.1,
                                MAX ((G_PARAM_SPEC_DOUBLE (spec)->maximum - G_PARAM_SPEC_DOUBLE (spec)->minimum) / 10, 0.1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 0.1, 2);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (double_changed),
                                 adj, G_OBJECT (adj));

      if (can_modify)
        connect_controller (G_OBJECT (adj), "value_changed",
                            object, spec, G_CALLBACK (double_modified));
    }
  else if (type == G_TYPE_PARAM_STRING)
    {
      prop_edit = gtk_entry_new ();

      g_object_connect_property (object, spec,
                                 G_CALLBACK (string_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "changed",
                            object, spec, G_CALLBACK (string_modified));
    }
  else if (type == G_TYPE_PARAM_BOOLEAN)
    {
      prop_edit = gtk_toggle_button_new_with_label ("");

      g_object_connect_property (object, spec,
                                 G_CALLBACK (bool_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "toggled",
                            object, spec, G_CALLBACK (bool_modified));
    }
  else if (type == G_TYPE_PARAM_ENUM)
    {
      {
        GEnumClass *eclass;
        gint j;

        prop_edit = gtk_combo_box_text_new ();

        eclass = G_ENUM_CLASS (g_type_class_ref (spec->value_type));

        j = 0;
        while (j < eclass->n_values)
          {
            gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (prop_edit),
                                            eclass->values[j].value_name);
            ++j;
          }

        g_type_class_unref (eclass);

        g_object_connect_property (object, spec,
                                   G_CALLBACK (enum_changed),
                                   prop_edit, G_OBJECT (prop_edit));

        if (can_modify)
          connect_controller (G_OBJECT (prop_edit), "changed",
                              object, spec, G_CALLBACK (enum_modified));
      }
    }
  else if (type == G_TYPE_PARAM_FLAGS)
    {
      {
        GFlagsClass *fclass;
        gint j;

        prop_edit = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

        fclass = G_FLAGS_CLASS (g_type_class_ref (spec->value_type));

        for (j = 0; j < fclass->n_values; j++)
          {
            GtkWidget *b;

            b = gtk_check_button_new_with_label (fclass->values[j].value_name);
            g_object_set_data (G_OBJECT (b), "index", GINT_TO_POINTER (j));
            gtk_widget_show (b);
            gtk_box_pack_start (GTK_BOX (prop_edit), b, FALSE, FALSE, 0);
            if (can_modify)
              connect_controller (G_OBJECT (b), "toggled",
                                  object, spec, G_CALLBACK (flags_modified));
          }

        g_type_class_unref (fclass);

        g_object_connect_property (object, spec,
                                   G_CALLBACK (flags_changed),
                                   prop_edit, G_OBJECT (prop_edit));
      }
    }
  else if (type == G_TYPE_PARAM_UNICHAR)
    {
      prop_edit = gtk_entry_new ();
      gtk_entry_set_max_length (GTK_ENTRY (prop_edit), 1);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (unichar_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "changed",
                            object, spec, G_CALLBACK (unichar_modified));
    }
  else if (type == G_TYPE_PARAM_POINTER)
    {
      prop_edit = gtk_label_new ("");

      g_object_connect_property (object, spec,
                                 G_CALLBACK (pointer_changed),
                                 prop_edit, G_OBJECT (prop_edit));
    }
  else if (type == G_TYPE_PARAM_OBJECT)
    {
      GtkWidget *label, *button;

      prop_edit = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

      label = gtk_label_new ("");
      button = gtk_button_new_with_label ("Properties");
      g_object_set_data (G_OBJECT (button), "property-name", (gpointer) spec->name);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (object_properties),
                        object);

      gtk_container_add (GTK_CONTAINER (prop_edit), label);
      gtk_container_add (GTK_CONTAINER (prop_edit), button);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (object_changed),
                                 prop_edit, G_OBJECT (label));

      /* The Properties button is not really modifying, anyway */
      can_modify = TRUE;
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == GDK_TYPE_RGBA)
    {
      prop_edit = gtk_color_button_new ();
      gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (prop_edit), TRUE);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (rgba_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "color-set",
                            object, spec, G_CALLBACK (rgba_modified));
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == GDK_TYPE_COLOR)
    {
      prop_edit = gtk_color_button_new ();

      g_object_connect_property (object, spec,
                                 G_CALLBACK (color_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "color-set",
                            object, spec, G_CALLBACK (color_modified));
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == PANGO_TYPE_FONT_DESCRIPTION)
    {
      prop_edit = gtk_font_button_new ();

      g_object_connect_property (object, spec,
                                 G_CALLBACK (font_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      if (can_modify)
        connect_controller (G_OBJECT (prop_edit), "notify::font-desc",
                            object, spec, G_CALLBACK (font_modified));
    }
  else
    {
      msg = g_strdup_printf ("uneditable property type: %s",
                             g_type_name (G_PARAM_SPEC_TYPE (spec)));
      prop_edit = gtk_label_new (msg);
      g_free (msg);
      gtk_widget_set_halign (prop_edit, GTK_ALIGN_START);
      gtk_widget_set_valign (prop_edit, GTK_ALIGN_CENTER);
    }

  if (!can_modify)
    gtk_widget_set_sensitive (prop_edit, FALSE);

  if (g_param_spec_get_blurb (spec))
    gtk_widget_set_tooltip_text (prop_edit, g_param_spec_get_blurb (spec));

  return prop_edit;
}

static GtkWidget *
properties_from_type (GObject *object,
                      GType    type)
{
  GtkWidget *prop_edit;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *vbox;
  GtkWidget *grid;
  GParamSpec **specs;
  guint n_specs;
  int i;

  if (G_TYPE_IS_INTERFACE (type))
    {
      gpointer vtable = g_type_default_interface_peek (type);
      specs = g_object_interface_list_properties (vtable, &n_specs);
    }
  else
    {
      GObjectClass *class = G_OBJECT_CLASS (g_type_class_peek (type));
      specs = g_object_class_list_properties (class, &n_specs);
    }

  if (n_specs == 0) {
    g_free (specs);
    return NULL;
  }

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);

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
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);

      prop_edit = property_widget (object, spec, can_modify);
      gtk_grid_attach (GTK_GRID (grid), prop_edit, 1, i, 1, 1);

      /* set initial value */
      g_object_notify (object, spec->name);

      ++i;
    }


  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (sw), vbox);

  g_free (specs);

  return sw;
}

static GtkWidget *
child_properties_from_object (GObject *object)
{
  GtkWidget *prop_edit;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *vbox;
  GtkWidget *grid;
  GtkWidget *parent;
  GParamSpec **specs;
  guint n_specs;
  gint i;

  if (!GTK_IS_WIDGET (object))
    return NULL;

  parent = gtk_widget_get_parent (GTK_WIDGET (object));

  if (!parent)
    return NULL;

  specs = gtk_container_class_list_child_properties (G_OBJECT_GET_CLASS (parent), &n_specs);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);

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

      label = gtk_label_new (g_param_spec_get_nick (spec));
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);

      mark_child_property (spec);
      prop_edit = property_widget (object, spec, can_modify);
      gtk_grid_attach (GTK_GRID (grid), prop_edit, 1, i, 1, 1);

      /* set initial value */
      gtk_widget_child_notify (GTK_WIDGET (object), spec->name);

      ++i;
    }

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (sw), vbox);

  g_free (specs);

  return sw;
}

static void
child_properties (GtkWidget *button,
                  GObject   *object)
{
  create_prop_editor (object, 0);
}

static GtkWidget *
children_from_object (GObject *object)
{
  GList *children, *c;
  GtkWidget *grid, *label, *prop_edit, *button, *vbox, *sw;
  gchar *str;
  gint i;

  if (!GTK_IS_CONTAINER (object))
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (object));

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);

  for (c = children, i = 0; c; c = c->next, i++)
    {
      object = c->data;

      label = gtk_label_new ("Child");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);

      prop_edit = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

      str = object_label (object, NULL);
      label = gtk_label_new (str);
      g_free (str);
      button = gtk_button_new_with_label ("Properties");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (child_properties),
                        object);

      gtk_container_add (GTK_CONTAINER (prop_edit), label);
      gtk_container_add (GTK_CONTAINER (prop_edit), button);

      gtk_grid_attach (GTK_GRID (grid), prop_edit, 1, i, 1, 1);
    }

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (sw), vbox);

  g_list_free (children);

  return sw;
}

static GtkWidget *
cells_from_object (GObject *object)
{
  GList *cells, *c;
  GtkWidget *grid, *label, *prop_edit, *button, *vbox, *sw;
  gchar *str;
  gint i;

  if (!GTK_IS_CELL_LAYOUT (object))
    return NULL;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (object));

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 3);

  for (c = cells, i = 0; c; c = c->next, i++)
    {
      object = c->data;

      label = gtk_label_new ("Cell");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
      gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);

      prop_edit = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

      str = object_label (object, NULL);
      label = gtk_label_new (str);
      g_free (str);
      button = gtk_button_new_with_label ("Properties");
      g_signal_connect (button, "clicked",
                        G_CALLBACK (child_properties),
                        object);

      gtk_container_add (GTK_CONTAINER (prop_edit), label);
      gtk_container_add (GTK_CONTAINER (prop_edit), button);

      gtk_grid_attach (GTK_GRID (grid), prop_edit, 1, i, 1, 1);
    }

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), grid, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  gtk_container_add (GTK_CONTAINER (sw), vbox);

  g_list_free (cells);

  return sw;
}

static void
open_parent_widget (GtkWidget *button,
		    GObject   *object)
{
  GtkWidget *parent;

  parent = gtk_widget_get_parent (GTK_WIDGET (object));
  if (parent != NULL)
    create_prop_editor (G_OBJECT (parent), 0);
}


/* Pass zero for type if you want all properties */
GtkWidget*
create_prop_editor (GObject   *object,
                    GType      type)
{
  GtkWidget *win, *parent;
  GtkWidget *notebook;
  GtkWidget *properties;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *vbox;
  gchar *title;
  GType *ifaces;
  guint n_ifaces;

  if ((win = g_object_get_data (G_OBJECT (object), "prop-editor-win")))
    {
      gtk_window_present (GTK_WINDOW (win));
      return win;
    }

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (GTK_IS_WIDGET (object))
    gtk_window_set_screen (GTK_WINDOW (win),
                           gtk_widget_get_screen (GTK_WIDGET (object)));

  /* hold a weak ref to the object we're editing */
  g_object_set_data_full (G_OBJECT (object), "prop-editor-win", win, model_destroy);
  g_object_set_data_full (G_OBJECT (win), "model-object", object, window_destroy);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  if (type == 0)
    {
      notebook = gtk_notebook_new ();
      gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_LEFT);

      gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

      type = G_TYPE_FROM_INSTANCE (object);

      title = g_strdup_printf ("Properties of %s widget", g_type_name (type));
      gtk_window_set_title (GTK_WINDOW (win), title);
      g_free (title);

      while (type)
        {
          properties = properties_from_type (object, type);
          if (properties)
            {
              label = gtk_label_new (g_type_name (type));
              gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                        properties, label);
            }

          type = g_type_parent (type);
        }

      ifaces = g_type_interfaces (G_TYPE_FROM_INSTANCE (object), &n_ifaces);
      while (n_ifaces--)
        {
          properties = properties_from_type (object, ifaces[n_ifaces]);
          if (properties)
            {
              label = gtk_label_new (g_type_name (ifaces[n_ifaces]));
              gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                        properties, label);
            }
        }

      g_free (ifaces);

      properties = child_properties_from_object (object);
      if (properties)
        {
          label = gtk_label_new ("Child properties");
          gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                    properties, label);
        }

      properties = children_from_object (object);
      if (properties)
        {
          label = gtk_label_new ("Children");
          gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                    properties, label);
        }

      properties = cells_from_object (object);
      if (properties)
        {
          label = gtk_label_new ("Cell renderers");
          gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                                    properties, label);
        }

      if (GTK_IS_WIDGET (object))
	{
	  parent = gtk_widget_get_parent (GTK_WIDGET (object));
	  if (parent != NULL)
	    {
	      button = gtk_button_new_with_label ("Parent widget");
	      gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	      g_signal_connect (button, "clicked",
				G_CALLBACK (open_parent_widget),
				object);
	    }
	}
    }
  else
    {
      properties = properties_from_type (object, type);
      gtk_box_pack_start (GTK_BOX (vbox), properties, TRUE, TRUE, 0);
      title = g_strdup_printf ("Properties of %s", g_type_name (type));
      gtk_window_set_title (GTK_WINDOW (win), title);
      g_free (title);
    }

  gtk_window_set_default_size (GTK_WINDOW (win), -1, 400);

  gtk_widget_show_all (win);

  return win;
}

