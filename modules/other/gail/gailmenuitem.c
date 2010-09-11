/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gailmenuitem.h"
#include "gailsubmenuitem.h"
#include <libgail-util/gailmisc.h>

#define KEYBINDING_SEPARATOR ";"

static void gail_menu_item_class_init  (GailMenuItemClass *klass);
static void gail_menu_item_init        (GailMenuItem      *menu_item);

static void                  gail_menu_item_real_initialize
                                                          (AtkObject       *obj,
                                                           gpointer        data);
static gint                  gail_menu_item_get_n_children (AtkObject      *obj);
static AtkObject*            gail_menu_item_ref_child      (AtkObject      *obj,
                                                            gint           i);
static AtkStateSet*          gail_menu_item_ref_state_set  (AtkObject      *obj);
static void                  gail_menu_item_finalize       (GObject        *object);
static void                  gail_menu_item_label_map_gtk  (GtkWidget     *widget,
                                                            gpointer       data);
static void                  gail_menu_item_init_textutil  (GailMenuItem  *item,
                                                            GtkWidget     *label);
static void                  gail_menu_item_notify_label_gtk (GObject       *obj,
                                                              GParamSpec    *pspec,
                                                              gpointer      data);


static void                  atk_action_interface_init     (AtkActionIface *iface);
static gboolean              gail_menu_item_do_action      (AtkAction      *action,
                                                            gint           i);
static gboolean              idle_do_action                (gpointer       data);
static gint                  gail_menu_item_get_n_actions  (AtkAction      *action);
static G_CONST_RETURN gchar* gail_menu_item_get_description(AtkAction      *action,
                                                            gint           i);
static G_CONST_RETURN gchar* gail_menu_item_get_name       (AtkAction      *action,
                                                            gint           i);
static G_CONST_RETURN gchar* gail_menu_item_get_keybinding (AtkAction      *action,
                                                            gint           i);
static gboolean              gail_menu_item_set_description(AtkAction      *action,
                                                            gint           i,
                                                            const gchar    *desc);
static void                  menu_item_select              (GtkMenuItem        *item);
static void                  menu_item_deselect            (GtkMenuItem        *item);
static void                  menu_item_selection           (GtkMenuItem        *item,
                                                            gboolean       selected);
static gboolean              find_accel                    (GtkAccelKey    *key,
                                                            GClosure       *closure,
                                                            gpointer       data);
static gboolean              find_accel_new                (GtkAccelKey    *key,
                                                            GClosure       *closure,
                                                            gpointer       data);
/* atktext.h */
static void       atk_text_interface_init          (AtkTextIface      *iface);

static gchar*     gail_menu_item_get_text          (AtkText           *text,
                                                    gint               start_pos,
                                                    gint               end_pos);
static gunichar   gail_menu_item_get_character_at_offset (AtkText     *text,
                                                          gint         offset);
static gchar*     gail_menu_item_get_text_before_offset  (AtkText     *text,
                                                          gint         offset,
                                                          AtkTextBoundary  boundary_type,
                                                          gint        *start_offset,
                                                          gint        *end_offset);
static gchar*     gail_menu_item_get_text_at_offset      (AtkText     *text,
                                                          gint         offset,
                                                          AtkTextBoundary   boundary_type,
                                                          gint        *start_offset,
                                                          gint        *end_offset);
static gchar*     gail_menu_item_get_text_after_offset   (AtkText     *text,
                                                          gint         offset,
                                                          AtkTextBoundary   boundary_type,
                                                          gint        *start_offset,
                                                          gint        *end_offset);
static gint       gail_menu_item_get_character_count     (AtkText     *text);
static void       gail_menu_item_get_character_extents   (AtkText     *text,
                                                          gint         offset,
                                                          gint        *x,
                                                          gint        *y,
                                                          gint        *width,
                                                          gint        *height,
                                                          AtkCoordType  coords);
static gint      gail_menu_item_get_offset_at_point      (AtkText     *text,
                                                          gint         x,
                                                          gint         y,
                                                          AtkCoordType  coords);
static AtkAttributeSet* gail_menu_item_get_run_attributes (AtkText    *text,
                                                           gint        offset,
                                                           gint       *start_offset,
                                                           gint       *end_offset);
static AtkAttributeSet* gail_menu_item_get_default_attributes (AtkText *text);
static GtkWidget*       get_label_from_container   (GtkWidget    *container);


G_DEFINE_TYPE_WITH_CODE (GailMenuItem, gail_menu_item, GAIL_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init))

static void
gail_menu_item_class_init (GailMenuItemClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gail_menu_item_finalize;

  class->get_n_children = gail_menu_item_get_n_children;
  class->ref_child = gail_menu_item_ref_child;
  class->ref_state_set = gail_menu_item_ref_state_set;
  class->initialize = gail_menu_item_real_initialize;
}

static void
gail_menu_item_real_initialize (AtkObject *obj,
                                gpointer  data)
{
  GtkWidget *widget;
  GtkWidget *parent;
  GailMenuItem *item = GAIL_MENU_ITEM (obj);
  GtkWidget *label;

  ATK_OBJECT_CLASS (gail_menu_item_parent_class)->initialize (obj, data);

  item->textutil = NULL;
  item->text = NULL;

  label = get_label_from_container (GTK_WIDGET (data));
  if (GTK_IS_LABEL (label))
    {
      if (gtk_widget_get_mapped (label))
        gail_menu_item_init_textutil (item, label);
      else
        g_signal_connect (label,
                          "map",
                          G_CALLBACK (gail_menu_item_label_map_gtk),
                          item);
    }

  g_signal_connect (data,
                    "select",
                    G_CALLBACK (menu_item_select),
                    NULL);
  g_signal_connect (data,
                    "deselect",
                    G_CALLBACK (menu_item_deselect),
                    NULL);
  widget = GTK_WIDGET (data);
  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU (parent))
    {
      GtkWidget *parent_widget;

      parent_widget =  gtk_menu_get_attach_widget (GTK_MENU (parent));

      if (!GTK_IS_MENU_ITEM (parent_widget))
        parent_widget = gtk_widget_get_parent (widget);
       if (parent_widget)
        {
          atk_object_set_parent (obj, gtk_widget_get_accessible (parent_widget));
        }
    }
  g_object_set_data (G_OBJECT (obj), "atk-component-layer",
                     GINT_TO_POINTER (ATK_LAYER_POPUP));

  if (GTK_IS_TEAROFF_MENU_ITEM (data))
    obj->role = ATK_ROLE_TEAR_OFF_MENU_ITEM;
  else if (GTK_IS_SEPARATOR_MENU_ITEM (data))
    obj->role = ATK_ROLE_SEPARATOR;
  else
    obj->role = ATK_ROLE_MENU_ITEM;
}

static void
gail_menu_item_init (GailMenuItem *menu_item)
{
  menu_item->click_keybinding = NULL;
  menu_item->click_description = NULL;
}

static void
gail_menu_item_label_map_gtk (GtkWidget *widget,
                              gpointer data)
{
  GailMenuItem *item;

  item = GAIL_MENU_ITEM (data);
  gail_menu_item_init_textutil (item, widget);
}

static void
gail_menu_item_notify_label_gtk (GObject           *obj,
                                 GParamSpec        *pspec,
                                 gpointer           data)
{
  AtkObject* atk_obj = ATK_OBJECT (data);
  GtkLabel *label;
  GailMenuItem *menu_item;

  if (strcmp (pspec->name, "label") == 0)
    {
      const gchar* label_text;

      label = GTK_LABEL (obj);

      label_text = gtk_label_get_text (label);

      menu_item = GAIL_MENU_ITEM (atk_obj);
      gail_text_util_text_setup (menu_item->textutil, label_text);

      if (atk_obj->name == NULL)
      {
        /*
         * The label has changed so notify a change in accessible-name
         */
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      }
      /*
       * The label is the only property which can be changed
       */
      g_signal_emit_by_name (atk_obj, "visible_data_changed");
    }
}

static void
gail_menu_item_init_textutil (GailMenuItem  *item,
                              GtkWidget *label)
{
  const gchar *label_text;

  if (item->textutil == NULL)
    {
      item->textutil = gail_text_util_new ();
      g_signal_connect (label,
                        "notify",
                        (GCallback) gail_menu_item_notify_label_gtk,
                        item);     
    }
  label_text = gtk_label_get_text (GTK_LABEL (label));
  gail_text_util_text_setup (item->textutil, label_text);
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gail_menu_item_get_text;
  iface->get_character_at_offset = gail_menu_item_get_character_at_offset;
  iface->get_text_before_offset = gail_menu_item_get_text_before_offset;
  iface->get_text_at_offset = gail_menu_item_get_text_at_offset;
  iface->get_text_after_offset = gail_menu_item_get_text_after_offset;
  iface->get_character_count = gail_menu_item_get_character_count;
  iface->get_character_extents = gail_menu_item_get_character_extents;
  iface->get_offset_at_point = gail_menu_item_get_offset_at_point;
  iface->get_run_attributes = gail_menu_item_get_run_attributes;
  iface->get_default_attributes = gail_menu_item_get_default_attributes;
}

static gchar*
gail_menu_item_get_text (AtkText *text,
                         gint    start_pos,
                         gint    end_pos)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailMenuItem *item;
  const gchar *label_text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL (label))
    return NULL;

  item = GAIL_MENU_ITEM (text);
  if (!item->textutil) 
    gail_menu_item_init_textutil (item, label);

  label_text = gtk_label_get_text (GTK_LABEL (label));

  if (label_text == NULL)
    return NULL;
  else
  {
    return gail_text_util_get_substring (item->textutil, 
                                         start_pos, end_pos);
  }
}

static gchar*
gail_menu_item_get_text_before_offset (AtkText         *text,
                                        gint            offset,
                                        AtkTextBoundary boundary_type,
                                        gint            *start_offset,
                                        gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailMenuItem *item;
  
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_MENU_ITEM (text);
  if (!item->textutil)
    gail_menu_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_BEFORE_OFFSET, 
                           boundary_type, offset, start_offset, end_offset); 
}

static gchar*
gail_menu_item_get_text_at_offset (AtkText         *text,
                                   gint            offset,
                                   AtkTextBoundary boundary_type,
                                   gint            *start_offset,
                                   gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailMenuItem *item;
 
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  
  if (widget == NULL)
    /* State is defunct */
    return NULL;
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_MENU_ITEM (text);
  if (!item->textutil)
    gail_menu_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                              gtk_label_get_layout (GTK_LABEL (label)), GAIL_AT_OFFSET, 
                              boundary_type, offset, start_offset, end_offset);
}

static gchar*
gail_menu_item_get_text_after_offset (AtkText         *text,
                                      gint            offset,
                                      AtkTextBoundary boundary_type,
                                      gint            *start_offset,
                                      gint            *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  GailMenuItem *item;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  
  if (widget == NULL)
  {
    /* State is defunct */
    return NULL;
  }
  
  /* Get label */
  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  item = GAIL_MENU_ITEM (text);
  if (!item->textutil)
    gail_menu_item_init_textutil (item, label);

  return gail_text_util_get_text (item->textutil,
                           gtk_label_get_layout (GTK_LABEL (label)), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gail_menu_item_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return 0;

  return g_utf8_strlen (gtk_label_get_text (GTK_LABEL (label)), -1);
}

static void
gail_menu_item_get_character_extents (AtkText      *text,
                                      gint         offset,
                                      gint         *x,
                                      gint         *y,
                                      gint         *width,
                                      gint         *height,
                                      AtkCoordType coords)
{
  GtkWidget *widget;
  GtkWidget *label;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
  const gchar *label_text;
 
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));

  if (widget == NULL)
    /* State is defunct */
    return;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  index = g_utf8_offset_to_pointer (label_text, offset) - label_text;
  pango_layout_index_to_pos (gtk_label_get_layout (GTK_LABEL (label)), index, &char_rect);
  
  gail_misc_get_extents_from_pango_rectangle (label, &char_rect, 
                    x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gail_menu_item_get_offset_at_point (AtkText      *text,
                               gint         x,
                               gint         y,
                               AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkWidget *label;
  gint index, x_layout, y_layout;
  const gchar *label_text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return -1;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return -1;
  
  gtk_label_get_layout_offsets (GTK_LABEL (label), &x_layout, &y_layout);
  
  index = gail_misc_get_index_at_point_in_layout (label, 
                                              gtk_label_get_layout (GTK_LABEL (label)), 
                                              x_layout, y_layout, x, y, coords);
  label_text = gtk_label_get_text (GTK_LABEL (label));
  if (index == -1)
    {
      if (coords == ATK_XY_WINDOW || coords == ATK_XY_SCREEN)
        return g_utf8_strlen (label_text, -1);

      return index;  
    }
  else
    return g_utf8_pointer_to_offset (label_text, label_text + index);  
}

static AtkAttributeSet*
gail_menu_item_get_run_attributes (AtkText *text,
                              gint    offset,
                              gint    *start_offset,
                              gint    *end_offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;
  GtkJustification justify;
  GtkTextDirection dir;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;
  
  /* Get values set for entire label, if any */
  justify = gtk_label_get_justify (GTK_LABEL (label));
  if (justify != GTK_JUSTIFY_CENTER)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_JUSTIFICATION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_JUSTIFICATION, justify)));
    }
  dir = gtk_widget_get_direction (label);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set, 
                                        ATK_TEXT_ATTR_DIRECTION,
     g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                gtk_label_get_layout (GTK_LABEL (label)),
                                                (gchar *) gtk_label_get_text (GTK_LABEL (label)),
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gail_menu_item_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  GtkWidget *label;
  AtkAttributeSet *at_set = NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return NULL;

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_label_get_layout (GTK_LABEL (label)),
                                             widget);
  return at_set;
}

static gunichar 
gail_menu_item_get_character_at_offset (AtkText *text,
                                   gint   offset)
{
  GtkWidget *widget;
  GtkWidget *label;
  const gchar *string;
  gchar *index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  label = get_label_from_container (widget);

  if (!GTK_IS_LABEL(label))
    return '\0';
  string = gtk_label_get_text (GTK_LABEL (label));
  if (offset >= g_utf8_strlen (string, -1))
    return '\0';
  index = g_utf8_offset_to_pointer (string, offset);

  return g_utf8_get_char (index);
}

static GtkWidget*
get_label_from_container (GtkWidget *container)
{
  GtkWidget *label;
  GList *children, *tmp_list;

  if (!GTK_IS_CONTAINER (container))
    return NULL;
 
  children = gtk_container_get_children (GTK_CONTAINER (container));
  label = NULL;

  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next) 
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
           label = tmp_list->data;
           break;
        }
      /*
 *        * Get label from menu item in desktop background preferences
 *               * option menu. See bug #144084.
 *                      */
      else if (GTK_IS_BOX (tmp_list->data))
        {
           label = get_label_from_container (GTK_WIDGET (tmp_list->data));
           if (label)
             break;
        }
    }
  g_list_free (children);

  return label;
}

AtkObject*
gail_menu_item_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;
  
  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), NULL);

  if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)))
    return gail_sub_menu_item_new (widget);

  object = g_object_new (GAIL_TYPE_MENU_ITEM, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

GList *
get_children (GtkWidget *submenu)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (submenu));
  if (g_list_length (children) == 0)
    {
      /*
       * If menu is empty it may be because the menu items are created only 
       * on demand. For example, in gnome-panel the menu items are created
       * only when "show" signal is emitted on the menu.
       *
       * The following hack forces the menu items to be created.
       */
      if (!gtk_widget_get_visible (submenu))
        {
          /* FIXME GTK_WIDGET_SET_FLAGS (submenu, GTK_VISIBLE); */
          g_signal_emit_by_name (submenu, "show");
          /* FIXME GTK_WIDGET_UNSET_FLAGS (submenu, GTK_VISIBLE); */
        }
      g_list_free (children);
      children = gtk_container_get_children (GTK_CONTAINER (submenu));
    }
  return children;
}

/*
 * If a menu item has a submenu return the items of the submenu as the 
 * accessible children; otherwise expose no accessible children.
 */
static gint
gail_menu_item_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  GtkWidget *submenu;
  gint count = 0;

  g_return_val_if_fail (GAIL_IS_MENU_ITEM (obj), count);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return count;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu)
    {
      GList *children;

      children = get_children (submenu);
      count = g_list_length (children);
      g_list_free (children);
    }
  return count;
}

static AtkObject*
gail_menu_item_ref_child (AtkObject *obj,
                          gint       i)
{
  AtkObject  *accessible;
  GtkWidget *widget;
  GtkWidget *submenu;

  g_return_val_if_fail (GAIL_IS_MENU_ITEM (obj), NULL);
  g_return_val_if_fail ((i >= 0), NULL);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu)
    {
      GList *children;
      GList *tmp_list;

      children = get_children (submenu);
      tmp_list = g_list_nth (children, i);
      if (!tmp_list)
        {
          g_list_free (children);
          return NULL;
        }
      accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));
      g_list_free (children);
      g_object_ref (accessible);
    }
  else
    accessible = NULL;

  return accessible;
}

static AtkStateSet*
gail_menu_item_ref_state_set (AtkObject *obj)
{
  AtkObject *menu_item;
  AtkStateSet *state_set, *parent_state_set;

  state_set = ATK_OBJECT_CLASS (gail_menu_item_parent_class)->ref_state_set (obj);

  menu_item = atk_object_get_parent (obj);

  if (menu_item)
    {
      if (!GTK_IS_MENU_ITEM (gtk_accessible_get_widget (GTK_ACCESSIBLE (menu_item))))
        return state_set;

      parent_state_set = atk_object_ref_state_set (menu_item);
      if (!atk_state_set_contains_state (parent_state_set, ATK_STATE_SELECTED))
        {
          atk_state_set_remove_state (state_set, ATK_STATE_FOCUSED);
          atk_state_set_remove_state (state_set, ATK_STATE_SHOWING);
        }
    }
  return state_set;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_menu_item_do_action;
  iface->get_n_actions = gail_menu_item_get_n_actions;
  iface->get_description = gail_menu_item_get_description;
  iface->get_name = gail_menu_item_get_name;
  iface->get_keybinding = gail_menu_item_get_keybinding;
  iface->set_description = gail_menu_item_set_description;
}

static gboolean
gail_menu_item_do_action (AtkAction *action,
                          gint      i)
{
  if (i == 0)
    {
      GtkWidget *item;
      GailMenuItem *gail_menu_item;

      item = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
      if (item == NULL)
        /* State is defunct */
        return FALSE;

      if (!gtk_widget_get_sensitive (item) || !gtk_widget_get_visible (item))
        return FALSE;

      gail_menu_item = GAIL_MENU_ITEM (action);
      if (gail_menu_item->action_idle_handler)
        return FALSE;
      else
	{
	  gail_menu_item->action_idle_handler =
            gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE,
                                       idle_do_action,
                                       g_object_ref (gail_menu_item),
                                       (GDestroyNotify) g_object_unref);
	}
      return TRUE;
    }
  else
    return FALSE;
}

static void
ensure_menus_unposted (GailMenuItem *menu_item)
{
  AtkObject *parent;
  GtkWidget *widget;

  parent = atk_object_get_parent (ATK_OBJECT (menu_item));
  while (parent)
    {
      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
      if (GTK_IS_MENU (widget))
        {
          if (gtk_widget_get_mapped (widget))
            gtk_menu_shell_cancel (GTK_MENU_SHELL (widget));

          return;
        }
      parent = atk_object_get_parent (parent);
    }
}

static gboolean
idle_do_action (gpointer data)
{
  GtkWidget *item;
  GtkWidget *item_parent;
  GailMenuItem *menu_item;
  gboolean item_mapped;

  menu_item = GAIL_MENU_ITEM (data);
  menu_item->action_idle_handler = 0;
  item = gtk_accessible_get_widget (GTK_ACCESSIBLE (menu_item));
  if (item == NULL /* State is defunct */ ||
      !gtk_widget_get_sensitive (item) || !gtk_widget_get_visible (item))
    return FALSE;

  item_parent = gtk_widget_get_parent (item);
  gtk_menu_shell_select_item (GTK_MENU_SHELL (item_parent), item);
  item_mapped = gtk_widget_get_mapped (item);
  /*
   * This is what is called when <Return> is pressed for a menu item
   */
  g_signal_emit_by_name (item_parent, "activate_current",  
                         /*force_hide*/ 1); 
  if (!item_mapped)
    ensure_menus_unposted (menu_item);

  return FALSE;
}

static gint
gail_menu_item_get_n_actions (AtkAction *action)
{
  /*
   * Menu item has 1 action
   */
  return 1;
}

static G_CONST_RETURN gchar*
gail_menu_item_get_description (AtkAction *action,
                                gint      i)
{
  if (i == 0)
    {
      GailMenuItem *item;

      item = GAIL_MENU_ITEM (action);
      return item->click_description;
    }
  else
    return NULL;
}

static G_CONST_RETURN gchar*
gail_menu_item_get_name (AtkAction *action,
                         gint      i)
{
  if (i == 0)
    return "click";
  else
    return NULL;
}

static G_CONST_RETURN gchar*
gail_menu_item_get_keybinding (AtkAction *action,
                               gint      i)
{
  /*
   * This function returns a string of the form A;B;C where
   * A is the keybinding for the widget; B is the keybinding to traverse
   * from the menubar and C is the accelerator.
   * The items in the keybinding to traverse from the menubar are separated
   * by ":".
   */
  GailMenuItem  *gail_menu_item;
  gchar *keybinding = NULL;
  gchar *item_keybinding = NULL;
  gchar *full_keybinding = NULL;
  gchar *accelerator = NULL;

  gail_menu_item = GAIL_MENU_ITEM (action);
  if (i == 0)
    {
      GtkWidget *item;
      GtkWidget *temp_item;
      GtkWidget *child;
      GtkWidget *parent;

      item = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
      if (item == NULL)
        /* State is defunct */
        return NULL;

      temp_item = item;
      while (TRUE)
        {
          GdkModifierType mnemonic_modifier = 0;
          guint key_val;
          gchar *key, *temp_keybinding;

          child = gtk_bin_get_child (GTK_BIN (temp_item));
          if (child == NULL)
            {
              /* Possibly a tear off menu item; it could also be a menu 
               * separator generated by gtk_item_factory_create_items()
               */
              return NULL;
            }
          parent = gtk_widget_get_parent (temp_item);
          if (!parent)
            {
              /*
               * parent can be NULL when activating a window from the panel
               */
              return NULL;
            }
          g_return_val_if_fail (GTK_IS_MENU_SHELL (parent), NULL);
          if (GTK_IS_MENU_BAR (parent))
            {
              GtkWidget *toplevel;

              toplevel = gtk_widget_get_toplevel (parent);
              if (toplevel && GTK_IS_WINDOW (toplevel))
                mnemonic_modifier = gtk_window_get_mnemonic_modifier (
                                       GTK_WINDOW (toplevel));
            }
          if (GTK_IS_LABEL (child))
            {
              key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (child));
              if (key_val != GDK_KEY_VoidSymbol)
                {
                  key = gtk_accelerator_name (key_val, mnemonic_modifier);
                  if (full_keybinding)
                    temp_keybinding = g_strconcat (key, ":", full_keybinding, NULL);
                  else 
                    temp_keybinding = g_strconcat (key, NULL);
                  if (temp_item == item)
                    {
                      item_keybinding = g_strdup (key); 
                    }
                  g_free (key);
                  g_free (full_keybinding);
                  full_keybinding = temp_keybinding;
                }
              else
                {
                  /* No keybinding */
                  g_free (full_keybinding);
                  full_keybinding = NULL;
                  break;
                }        
            }        
          if (GTK_IS_MENU_BAR (parent))
            /* We have reached the menu bar so we are finished */
            break;
          g_return_val_if_fail (GTK_IS_MENU (parent), NULL);
          temp_item = gtk_menu_get_attach_widget (GTK_MENU (parent));
          if (!GTK_IS_MENU_ITEM (temp_item))
            {
              /* 
               * Menu is attached to something other than a menu item;
               * probably an option menu
               */
              g_free (full_keybinding);
              full_keybinding = NULL;
              break;
            }
        }

      parent = gtk_widget_get_parent (item);
      if (GTK_IS_MENU (parent))
        {
          GtkAccelGroup *group; 
          GtkAccelKey *key;

          group = gtk_menu_get_accel_group (GTK_MENU (parent));

          if (group)
            {
              key = gtk_accel_group_find (group, find_accel, item);
            }
          else
            {
              /*
               * If the menu item is created using GtkAction and GtkUIManager
               * we get here.
               */
              key = NULL;
              child = gtk_bin_get_child (GTK_BIN (item));
              if (GTK_IS_ACCEL_LABEL (child))
                {
                  GtkAccelLabel *accel_label;
                  GClosure      *accel_closure;

                  accel_label = GTK_ACCEL_LABEL (child);
                  g_object_get (accel_label,
                                "accel-closure", &accel_closure,
                                NULL);
                  if (accel_closure)
                    {
                      key = gtk_accel_group_find (gtk_accel_group_from_accel_closure (accel_closure),
                                                  find_accel_new,
                                                  accel_closure);
                    }
               
                }
            }

          if (key)
            {           
              accelerator = gtk_accelerator_name (key->accel_key,
                                                  key->accel_mods);
            }
        }
    }
  /*
   * Concatenate the bindings
   */
  if (item_keybinding || full_keybinding || accelerator)
    {
      gchar *temp;
      if (item_keybinding)
        {
          keybinding = g_strconcat (item_keybinding, KEYBINDING_SEPARATOR, NULL);
          g_free (item_keybinding);
        }
      else
        keybinding = g_strconcat (KEYBINDING_SEPARATOR, NULL);

      if (full_keybinding)
        {
          temp = g_strconcat (keybinding, full_keybinding, 
                              KEYBINDING_SEPARATOR, NULL);
          g_free (full_keybinding);
        }
      else
        temp = g_strconcat (keybinding, KEYBINDING_SEPARATOR, NULL);

      g_free (keybinding);
      keybinding = temp;
      if (accelerator)
        {
          temp = g_strconcat (keybinding, accelerator, NULL);
          g_free (accelerator);
          g_free (keybinding);
          keybinding = temp;
      }
    }
  g_free (gail_menu_item->click_keybinding);
  gail_menu_item->click_keybinding = keybinding;
  return keybinding;
}

static gboolean
gail_menu_item_set_description (AtkAction      *action,
                                gint           i,
                                const gchar    *desc)
{
  if (i == 0)
    {
      GailMenuItem *item;

      item = GAIL_MENU_ITEM (action);
      g_free (item->click_description);
      item->click_description = g_strdup (desc);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gail_menu_item_finalize (GObject *object)
{
  GailMenuItem *menu_item = GAIL_MENU_ITEM (object);

  g_free (menu_item->click_keybinding);
  g_free (menu_item->click_description);
  if (menu_item->action_idle_handler)
    {
      g_source_remove (menu_item->action_idle_handler);
      menu_item->action_idle_handler = 0;
    }

  if (menu_item->textutil)
    {
      g_object_unref (menu_item->textutil);
    }
  if (menu_item->text)
    {
      g_free (menu_item->text);
      menu_item->text = NULL;
    }

  G_OBJECT_CLASS (gail_menu_item_parent_class)->finalize (object);
}

static void
menu_item_select (GtkMenuItem *item)
{
  menu_item_selection (item, TRUE);
}

static void
menu_item_deselect (GtkMenuItem *item)
{
  menu_item_selection (item, FALSE);
}

static void
menu_item_selection (GtkMenuItem  *item,
                     gboolean selected)
{
  AtkObject *obj, *parent;
  gint i;

  obj = gtk_widget_get_accessible (GTK_WIDGET (item));
  atk_object_notify_state_change (obj, ATK_STATE_SELECTED, selected);

  for (i = 0; i < atk_object_get_n_accessible_children (obj); i++)
    {
      AtkObject *child;
      child = atk_object_ref_accessible_child (obj, i);
      atk_object_notify_state_change (child, ATK_STATE_SHOWING, selected);
      g_object_unref (child);
    }
  parent = atk_object_get_parent (obj);
  g_signal_emit_by_name (parent, "selection_changed"); 
}

static gboolean
find_accel (GtkAccelKey *key,
            GClosure    *closure,
            gpointer     data)
{
  /*
   * We assume that closure->data points to the widget
   * pending gtk_widget_get_accel_closures being made public
   */
  return data == (gpointer) closure->data;
}

static gboolean
find_accel_new (GtkAccelKey *key,
                GClosure    *closure,
                gpointer     data)
{
  return data == (gpointer) closure;
}
