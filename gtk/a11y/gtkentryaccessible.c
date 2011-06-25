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

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gtkentryaccessible.h"
#include "gailcombobox.h"
#include <libgail-util/gailmisc.h>

static void       gtk_entry_accessible_class_init            (GtkEntryAccessibleClass       *klass);
static void       gtk_entry_accessible_init                  (GtkEntryAccessible            *entry);
static void	  gtk_entry_accessible_real_initialize       (AtkObject            *obj,
                                                    gpointer             data);
static void       text_setup                       (GtkEntryAccessible            *entry,
                                                    GtkEntry             *gtk_entry);
static void	  gtk_entry_accessible_real_notify_gtk	   (GObject		 *obj,
                                                    GParamSpec		 *pspec);
static void       gtk_entry_accessible_finalize              (GObject              *object);

static gint       gtk_entry_accessible_get_index_in_parent   (AtkObject            *accessible);

/* atkobject.h */

static AtkStateSet*     gtk_entry_accessible_ref_state_set   (AtkObject            *accessible);
static AtkAttributeSet* gtk_entry_accessible_get_attributes  (AtkObject            *accessible);

/* atktext.h */

static void       atk_text_interface_init          (AtkTextIface         *iface);

static gchar*     gtk_entry_accessible_get_text              (AtkText              *text,
                                                    gint                 start_pos,
                                                    gint                 end_pos);
static gunichar	  gtk_entry_accessible_get_character_at_offset
						   (AtkText		 *text,
						    gint		 offset);
static gchar*	  gtk_entry_accessible_get_text_before_offset(AtkText		 *text,
						    gint		 offset,
						    AtkTextBoundary	 boundary_type,
						    gint		 *start_offset,
						    gint		 *end_offset);
static gchar*	  gtk_entry_accessible_get_text_at_offset	   (AtkText		 *text,
						    gint		 offset,
						    AtkTextBoundary	 boundary_type,
						    gint		 *start_offset,
						    gint		 *end_offset);
static gchar*	  gtk_entry_accessible_get_text_after_offset (AtkText		 *text,
						    gint		 offset,
						    AtkTextBoundary	 boundary_type,
						    gint		 *start_offset,
						    gint		 *end_offset);
static gint       gtk_entry_accessible_get_caret_offset      (AtkText              *text);
static gboolean   gtk_entry_accessible_set_caret_offset      (AtkText              *text,
						    gint                 offset);
static gint	  gtk_entry_accessible_get_n_selections	   (AtkText		 *text);
static gchar*	  gtk_entry_accessible_get_selection	   (AtkText		 *text,
						    gint		 selection_num,
						    gint		 *start_offset,
						    gint		 *end_offset);
static gboolean	  gtk_entry_accessible_add_selection	   (AtkText		 *text,
						    gint		 start_offset,
						    gint		 end_offset);
static gboolean	  gtk_entry_accessible_remove_selection	   (AtkText		 *text,
						    gint		 selection_num);
static gboolean	  gtk_entry_accessible_set_selection	   (AtkText		 *text,
						    gint		 selection_num,
						    gint		 start_offset,
						    gint		 end_offset);
static gint	  gtk_entry_accessible_get_character_count   (AtkText		 *text);
static AtkAttributeSet *  gtk_entry_accessible_get_run_attributes 
                                                   (AtkText              *text,
						    gint		 offset,
        					    gint		 *start_offset,
					       	    gint 		 *end_offset);
static AtkAttributeSet *  gtk_entry_accessible_get_default_attributes 
                                                   (AtkText              *text);
static void gtk_entry_accessible_get_character_extents       (AtkText	         *text,
						    gint 	         offset,
		                                    gint 	         *x,
                    		   	            gint 	         *y,
                                		    gint 	         *width,
                                     		    gint 	         *height,
			        		    AtkCoordType         coords);
static gint gtk_entry_accessible_get_offset_at_point         (AtkText              *text,
                                                    gint                 x,
                                                    gint                 y,
			                            AtkCoordType         coords);
/* atkeditabletext.h */

static void       atk_editable_text_interface_init (AtkEditableTextIface *iface);
static void       gtk_entry_accessible_set_text_contents     (AtkEditableText      *text,
                                                    const gchar          *string);
static void       gtk_entry_accessible_insert_text           (AtkEditableText      *text,
                                                    const gchar          *string,
                                                    gint                 length,
                                                    gint                 *position);
static void       gtk_entry_accessible_copy_text             (AtkEditableText      *text,
                                                    gint                 start_pos,
                                                    gint                 end_pos);
static void       gtk_entry_accessible_cut_text              (AtkEditableText      *text,
                                                    gint                 start_pos,
                                                    gint                 end_pos);
static void       gtk_entry_accessible_delete_text           (AtkEditableText      *text,
                                                    gint                 start_pos,
                                                    gint                 end_pos);
static void       gtk_entry_accessible_paste_text            (AtkEditableText      *text,
                                                    gint                 position);
static void       gtk_entry_accessible_paste_received	   (GtkClipboard *clipboard,
						    const gchar  *text,
						    gpointer     data);


/* Callbacks */

static gboolean   gtk_entry_accessible_idle_notify_insert    (gpointer data);
static void       gtk_entry_accessible_notify_insert         (GtkEntryAccessible            *entry);
static void       gtk_entry_accessible_notify_delete         (GtkEntryAccessible            *entry);
static void	  _gtk_entry_accessible_insert_text_cb	   (GtkEntry     	 *entry,
                                                    gchar		 *arg1,
                                                    gint		 arg2,
                                                    gpointer		 arg3);
static void	  _gtk_entry_accessible_delete_text_cb	   (GtkEntry		 *entry,
                                                    gint		 arg1,
                                                    gint		 arg2);
static void	  _gtk_entry_accessible_changed_cb           (GtkEntry		 *entry);
static gboolean   check_for_selection_change       (GtkEntryAccessible            *entry,
                                                    GtkEntry             *gtk_entry);

static void                  atk_action_interface_init          (AtkActionIface  *iface);

static gboolean              gtk_entry_accessible_do_action               (AtkAction       *action,
                                                                 gint            i);
static gboolean              idle_do_action                     (gpointer        data);
static gint                  gtk_entry_accessible_get_n_actions           (AtkAction       *action);
static const gchar* gtk_entry_accessible_get_keybinding          (AtkAction       *action,
                                                                 gint            i);
static const gchar* gtk_entry_accessible_action_get_name         (AtkAction       *action,
                                                                 gint            i);

typedef struct _GtkEntryAccessiblePaste			GtkEntryAccessiblePaste;

struct _GtkEntryAccessiblePaste
{
  GtkEntry* entry;
  gint position;
};

G_DEFINE_TYPE_WITH_CODE (GtkEntryAccessible, gtk_entry_accessible, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_EDITABLE_TEXT, atk_editable_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TEXT, atk_text_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void
gtk_entry_accessible_class_init (GtkEntryAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  gobject_class->finalize = gtk_entry_accessible_finalize;

  class->ref_state_set = gtk_entry_accessible_ref_state_set;
  class->get_index_in_parent = gtk_entry_accessible_get_index_in_parent;
  class->initialize = gtk_entry_accessible_real_initialize;
  class->get_attributes = gtk_entry_accessible_get_attributes;

  widget_class->notify_gtk = gtk_entry_accessible_real_notify_gtk;
}

static void
gtk_entry_accessible_init (GtkEntryAccessible *entry)
{
  entry->textutil = NULL;
  entry->signal_name_insert = NULL;
  entry->signal_name_delete = NULL;
  entry->cursor_position = 0;
  entry->selection_bound = 0;
  entry->activate_keybinding = NULL;
}

static void
gtk_entry_accessible_real_initialize (AtkObject *obj, 
                            gpointer  data)
{
  GtkEntry *entry;
  GtkEntryAccessible *gtk_entry_accessible;
  gint start_pos, end_pos;

  ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->initialize (obj, data);

  gtk_entry_accessible = GTK_ENTRY_ACCESSIBLE (obj);
  gtk_entry_accessible->textutil = gail_text_util_new ();
  
  g_assert (GTK_IS_ENTRY (data));

  entry = GTK_ENTRY (data);
  text_setup (gtk_entry_accessible, entry);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                     &start_pos, &end_pos);
  gtk_entry_accessible->cursor_position = end_pos;
  gtk_entry_accessible->selection_bound = start_pos;

  /* Set up signal callbacks */
  g_signal_connect (data, "insert-text",
	G_CALLBACK (_gtk_entry_accessible_insert_text_cb), NULL);
  g_signal_connect (data, "delete-text",
	G_CALLBACK (_gtk_entry_accessible_delete_text_cb), NULL);
  g_signal_connect (data, "changed",
	G_CALLBACK (_gtk_entry_accessible_changed_cb), NULL);

  if (gtk_entry_get_visibility (entry))
    obj->role = ATK_ROLE_TEXT;
  else
    obj->role = ATK_ROLE_PASSWORD_TEXT;
}

static void
gtk_entry_accessible_real_notify_gtk (GObject		*obj,
                            GParamSpec		*pspec)
{
  GtkWidget *widget;
  AtkObject* atk_obj;
  GtkEntry* gtk_entry;
  GtkEntryAccessible* entry;

  widget = GTK_WIDGET (obj);
  atk_obj = gtk_widget_get_accessible (widget);
  gtk_entry = GTK_ENTRY (widget);
  entry = GTK_ENTRY_ACCESSIBLE (atk_obj);

  if (strcmp (pspec->name, "cursor-position") == 0)
    {
      if (entry->insert_idle_handler == 0)
        entry->insert_idle_handler = gdk_threads_add_idle (gtk_entry_accessible_idle_notify_insert, entry);

      if (check_for_selection_change (entry, gtk_entry))
        g_signal_emit_by_name (atk_obj, "text_selection_changed");
      /*
       * The entry cursor position has moved so generate the signal.
       */
      g_signal_emit_by_name (atk_obj, "text_caret_moved", 
                             entry->cursor_position);
    }
  else if (strcmp (pspec->name, "selection-bound") == 0)
    {
      if (entry->insert_idle_handler == 0)
        entry->insert_idle_handler = gdk_threads_add_idle (gtk_entry_accessible_idle_notify_insert, entry);

      if (check_for_selection_change (entry, gtk_entry))
        g_signal_emit_by_name (atk_obj, "text_selection_changed");
    }
  else if (strcmp (pspec->name, "editable") == 0)
    {
      gboolean value;

      g_object_get (obj, "editable", &value, NULL);
      atk_object_notify_state_change (atk_obj, ATK_STATE_EDITABLE,
                                               value);
    }
  else if (strcmp (pspec->name, "visibility") == 0)
    {
      gboolean visibility;
      AtkRole new_role;

      text_setup (entry, gtk_entry);
      visibility = gtk_entry_get_visibility (gtk_entry);
      new_role = visibility ? ATK_ROLE_TEXT : ATK_ROLE_PASSWORD_TEXT;
      atk_object_set_role (atk_obj, new_role);
    }
  else if (strcmp (pspec->name, "invisible-char") == 0)
    {
      text_setup (entry, gtk_entry);
    }
  else if (strcmp (pspec->name, "editing-canceled") == 0)
    {
      if (entry->insert_idle_handler)
        {
          g_source_remove (entry->insert_idle_handler);
          entry->insert_idle_handler = 0;
        }
    }
  else
    GAIL_WIDGET_CLASS (gtk_entry_accessible_parent_class)->notify_gtk (obj, pspec);
}

static void
text_setup (GtkEntryAccessible *entry,
            GtkEntry  *gtk_entry)
{
  if (gtk_entry_get_visibility (gtk_entry))
    {
      gail_text_util_text_setup (entry->textutil, gtk_entry_get_text (gtk_entry));
    }
  else
    {
      gunichar invisible_char;
      GString *tmp_string = g_string_new (NULL);
      gint ch_len; 
      gchar buf[7];
      guint length;
      gint i;

      invisible_char = gtk_entry_get_invisible_char (gtk_entry);
      if (invisible_char == 0)
        invisible_char = ' ';

      ch_len = g_unichar_to_utf8 (invisible_char, buf);
      length = gtk_entry_get_text_length (gtk_entry);
      for (i = 0; i < length; i++)
        {
          g_string_append_len (tmp_string, buf, ch_len);
        }

      gail_text_util_text_setup (entry->textutil, tmp_string->str);
      g_string_free (tmp_string, TRUE);

    } 
}

static void
gtk_entry_accessible_finalize (GObject            *object)
{
  GtkEntryAccessible *entry = GTK_ENTRY_ACCESSIBLE (object);

  g_object_unref (entry->textutil);
  g_free (entry->activate_keybinding);
  if (entry->action_idle_handler)
    {
      g_source_remove (entry->action_idle_handler);
      entry->action_idle_handler = 0;
    }
  if (entry->insert_idle_handler)
    {
      g_source_remove (entry->insert_idle_handler);
      entry->insert_idle_handler = 0;
    }
  G_OBJECT_CLASS (gtk_entry_accessible_parent_class)->finalize (object);
}

static gint
gtk_entry_accessible_get_index_in_parent (AtkObject *accessible)
{
  /*
   * If the parent widget is a combo box then the index is 1
   * otherwise do the normal thing.
   */
  if (accessible->accessible_parent)
    if (GAIL_IS_COMBO_BOX (accessible->accessible_parent))
      return 1;

  return ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->get_index_in_parent (accessible);
}

/* atkobject.h */

static AtkStateSet*
gtk_entry_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkEntry *entry;
  gboolean value;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->ref_state_set (accessible);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));

  if (widget == NULL)
    return state_set;

  entry = GTK_ENTRY (widget);

  g_object_get (G_OBJECT (entry), "editable", &value, NULL);
  if (value)
    atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

  return state_set;
}

static AtkAttributeSet *
gtk_entry_accessible_get_attributes (AtkObject *accessible)
{
  GtkWidget *widget;
  AtkAttributeSet *attributes;
  AtkAttribute *placeholder_text;
  const gchar *text;

  attributes = ATK_OBJECT_CLASS (gtk_entry_accessible_parent_class)->get_attributes (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return attributes;

  text = gtk_entry_get_placeholder_text (GTK_ENTRY (widget));
  if (text == NULL)
    return attributes;

  placeholder_text = g_malloc (sizeof (AtkAttribute));
  placeholder_text->name = g_strdup ("placeholder-text");
  placeholder_text->value = g_strdup (text);

  attributes = g_slist_append (attributes, placeholder_text);

  return attributes;
}

/* atktext.h */

static void
atk_text_interface_init (AtkTextIface *iface)
{
  iface->get_text = gtk_entry_accessible_get_text;
  iface->get_character_at_offset = gtk_entry_accessible_get_character_at_offset;
  iface->get_text_before_offset = gtk_entry_accessible_get_text_before_offset;
  iface->get_text_at_offset = gtk_entry_accessible_get_text_at_offset;
  iface->get_text_after_offset = gtk_entry_accessible_get_text_after_offset;
  iface->get_caret_offset = gtk_entry_accessible_get_caret_offset;
  iface->set_caret_offset = gtk_entry_accessible_set_caret_offset;
  iface->get_character_count = gtk_entry_accessible_get_character_count;
  iface->get_n_selections = gtk_entry_accessible_get_n_selections;
  iface->get_selection = gtk_entry_accessible_get_selection;
  iface->add_selection = gtk_entry_accessible_add_selection;
  iface->remove_selection = gtk_entry_accessible_remove_selection;
  iface->set_selection = gtk_entry_accessible_set_selection;
  iface->get_run_attributes = gtk_entry_accessible_get_run_attributes;
  iface->get_default_attributes = gtk_entry_accessible_get_default_attributes;
  iface->get_character_extents = gtk_entry_accessible_get_character_extents;
  iface->get_offset_at_point = gtk_entry_accessible_get_offset_at_point;
}

static gchar*
gtk_entry_accessible_get_text (AtkText *text,
                     gint    start_pos,
                     gint    end_pos)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  return gail_text_util_get_substring (GTK_ENTRY_ACCESSIBLE (text)->textutil, start_pos, end_pos);
}

static gchar*
gtk_entry_accessible_get_text_before_offset (AtkText	    *text,
				   gint		    offset,
				   AtkTextBoundary  boundary_type,
				   gint		    *start_offset,
				   gint		    *end_offset)
{
  GtkWidget *widget;
  GtkEntry *entry;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  /* Get Entry */
  entry = GTK_ENTRY (widget);

  return gail_text_util_get_text (GTK_ENTRY_ACCESSIBLE (text)->textutil,
                          gtk_entry_get_layout (entry), GAIL_BEFORE_OFFSET, 
                          boundary_type, offset, start_offset, end_offset);
}

static gchar*
gtk_entry_accessible_get_text_at_offset (AtkText          *text,
                               gint             offset,
                               AtkTextBoundary  boundary_type,
                               gint             *start_offset,
                               gint             *end_offset)
{
  GtkWidget *widget;
  GtkEntry *entry;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  /* Get Entry */
  entry = GTK_ENTRY (widget);

  return gail_text_util_get_text (GTK_ENTRY_ACCESSIBLE (text)->textutil,
                            gtk_entry_get_layout (entry), GAIL_AT_OFFSET, 
                            boundary_type, offset, start_offset, end_offset);
}

static gchar*
gtk_entry_accessible_get_text_after_offset  (AtkText	    *text,
				   gint		    offset,
				   AtkTextBoundary  boundary_type,
				   gint		    *start_offset,
				   gint		    *end_offset)
{
  GtkWidget *widget;
  GtkEntry *entry;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  /* Get Entry */
  entry = GTK_ENTRY (widget);

  return gail_text_util_get_text (GTK_ENTRY_ACCESSIBLE (text)->textutil,
                           gtk_entry_get_layout (entry), GAIL_AFTER_OFFSET, 
                           boundary_type, offset, start_offset, end_offset);
}

static gint
gtk_entry_accessible_get_character_count (AtkText *text)
{
  GtkEntry *entry;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return 0;

  entry = GTK_ENTRY (widget);
  return g_utf8_strlen (gtk_entry_get_text (entry), -1);
}

static gint
gtk_entry_accessible_get_caret_offset (AtkText *text)
{
  GtkEntry *entry;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return 0;

  entry = GTK_ENTRY (widget);

  return gtk_editable_get_position (GTK_EDITABLE (entry));
}

static gboolean
gtk_entry_accessible_set_caret_offset (AtkText *text, gint offset)
{
  GtkEntry *entry;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  entry = GTK_ENTRY (widget);

  gtk_editable_set_position (GTK_EDITABLE (entry), offset);
  return TRUE;
}

static AtkAttributeSet*
gtk_entry_accessible_get_run_attributes (AtkText *text,
			       gint    offset,
                               gint    *start_offset,
                               gint    *end_offset)
{
  GtkWidget *widget;
  GtkEntry *entry;
  AtkAttributeSet *at_set = NULL;
  GtkTextDirection dir;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  entry = GTK_ENTRY (widget);
 
  dir = gtk_widget_get_direction (widget);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set,
                                        ATK_TEXT_ATTR_DIRECTION,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                gtk_entry_get_layout (entry),
                                                (gchar*)gtk_entry_get_text (entry),
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
gtk_entry_accessible_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  GtkEntry *entry;
  AtkAttributeSet *at_set = NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  entry = GTK_ENTRY (widget);

  at_set = gail_misc_get_default_attributes (at_set,
                                             gtk_entry_get_layout (entry),
                                             widget);
  return at_set;
}
  
static void
gtk_entry_accessible_get_character_extents (AtkText *text,
				  gint    offset,
		                  gint    *x,
                    		  gint 	  *y,
                                  gint 	  *width,
                                  gint 	  *height,
			          AtkCoordType coords)
{
  GtkWidget *widget;
  GtkEntry *entry;
  PangoRectangle char_rect;
  gint index, x_layout, y_layout;
  const gchar *entry_text;
  gint start_pos, end_pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);

  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                     &start_pos, &end_pos);
  gtk_entry_get_layout_offsets (entry, &x_layout, &y_layout);
  entry_text = gtk_entry_get_text (entry);

  index = g_utf8_offset_to_pointer (entry_text, offset) - entry_text;

  /* FIXME: entry->preedit cannot be accessed directly
  cursor_index = g_utf8_offset_to_pointer (entry_text, end_pos) - entry_text;
  if (index > cursor_index)
    index += entry->preedit_length;
  */
  pango_layout_index_to_pos (gtk_entry_get_layout(entry), index, &char_rect);
 
  gail_misc_get_extents_from_pango_rectangle (widget, &char_rect, 
                        x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
gtk_entry_accessible_get_offset_at_point (AtkText *text,
                                gint x,
                                gint y,
			        AtkCoordType coords)
{ 
  GtkWidget *widget;
  GtkEntry *entry;
  gint index, x_layout, y_layout;
  const gchar *entry_text;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return -1;

  entry = GTK_ENTRY (widget);
  
  gtk_entry_get_layout_offsets (entry, &x_layout, &y_layout);
  entry_text = gtk_entry_get_text (entry);
  
  index = gail_misc_get_index_at_point_in_layout (widget, 
               gtk_entry_get_layout(entry), x_layout, y_layout, x, y, coords);
  if (index == -1)
    {
      if (coords == ATK_XY_SCREEN || coords == ATK_XY_WINDOW)
        return g_utf8_strlen (entry_text, -1);

      return index;  
    }
  else
    {
      gint start_pos, end_pos;

      gtk_editable_get_selection_bounds (GTK_EDITABLE (entry),
                                         &start_pos, &end_pos);
      /* FIXME: entry->preedit_length cannot be accessed directly
      cursor_index = g_utf8_offset_to_pointer (entry_text, end_pos) - entry_text;
      if (index >= cursor_index && entry->preedit_length)
        {
          if (index >= cursor_index + entry->preedit_length)
            index -= entry->preedit_length;
          else
            index = cursor_index;
        }
      */
      return g_utf8_pointer_to_offset (entry_text, entry_text + index);
    }
}

static gint
gtk_entry_accessible_get_n_selections (AtkText              *text)
{
  GtkEntry *entry;
  GtkWidget *widget;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return -1;

  entry = GTK_ENTRY (widget);

  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    return 1;
  else
    return 0;
}

static gchar*
gtk_entry_accessible_get_selection (AtkText *text,
			  gint    selection_num,
                          gint    *start_pos,
                          gint    *end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

 /* Only let the user get the selection if one is set, and if the
  * selection_num is 0.
  */
  if (selection_num != 0)
     return NULL;

  entry = GTK_ENTRY (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), start_pos, end_pos);

  if (*start_pos != *end_pos)
     return gtk_editable_get_chars (GTK_EDITABLE (entry), *start_pos, *end_pos);
  else
     return NULL;
}

static gboolean
gtk_entry_accessible_add_selection (AtkText *text,
                          gint    start_pos,
                          gint    end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  entry = GTK_ENTRY (widget);

  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &select_start, 
                                     &select_end);

 /* If there is already a selection, then don't allow another to be added,
  * since GtkEntry only supports one selected region.
  */
  if (select_start == select_end)
    {
       gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
       return TRUE;
    }
  else
   return FALSE;
}

static gboolean
gtk_entry_accessible_remove_selection (AtkText *text,
                             gint    selection_num)
{
  GtkEntry *entry;
  GtkWidget *widget;
  gint select_start, select_end, caret_pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  entry = GTK_ENTRY (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    {
     /* Setting the start & end of the selected region to the caret position
      * turns off the selection.
      */
      caret_pos = gtk_editable_get_position (GTK_EDITABLE (entry));
      gtk_editable_select_region (GTK_EDITABLE (entry), caret_pos, caret_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_entry_accessible_set_selection (AtkText *text,
			  gint	  selection_num,
                          gint    start_pos,
                          gint    end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

 /* Only let the user move the selection if one is set, and if the
  * selection_num is 0
  */
  if (selection_num != 0)
     return FALSE;

  entry = GTK_ENTRY (widget);

  gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    {
      gtk_editable_select_region (GTK_EDITABLE (entry), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  iface->set_text_contents = gtk_entry_accessible_set_text_contents;
  iface->insert_text = gtk_entry_accessible_insert_text;
  iface->copy_text = gtk_entry_accessible_copy_text;
  iface->cut_text = gtk_entry_accessible_cut_text;
  iface->delete_text = gtk_entry_accessible_delete_text;
  iface->paste_text = gtk_entry_accessible_paste_text;
  iface->set_run_attributes = NULL;
}

static void
gtk_entry_accessible_set_text_contents (AtkEditableText *text,
                              const gchar     *string)
{
  GtkEntry *entry;
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (entry);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_entry_set_text (entry, string);
}

static void
gtk_entry_accessible_insert_text (AtkEditableText *text,
                        const gchar     *string,
                        gint            length,
                        gint            *position)
{
  GtkEntry *entry;
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (entry);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_insert_text (editable, string, length, position);
  gtk_editable_set_position (editable, *position);
}

static void
gtk_entry_accessible_copy_text   (AtkEditableText *text,
                        gint            start_pos,
                        gint            end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (entry);
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
}

static void
gtk_entry_accessible_cut_text (AtkEditableText *text,
                     gint            start_pos,
                     gint            end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  GtkEditable *editable;
  gchar *str;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (entry);
  if (!gtk_editable_get_editable (editable))
    return;
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, str, -1);
  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
gtk_entry_accessible_delete_text (AtkEditableText *text,
                        gint            start_pos,
                        gint            end_pos)
{
  GtkEntry *entry;
  GtkWidget *widget;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  entry = GTK_ENTRY (widget);
  editable = GTK_EDITABLE (entry);
  if (!gtk_editable_get_editable (editable))
    return;

  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
gtk_entry_accessible_paste_text (AtkEditableText *text,
                       gint            position)
{
  GtkWidget *widget;
  GtkEditable *editable;
  GtkEntryAccessiblePaste paste_struct;
  GtkClipboard *clipboard;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;
  paste_struct.entry = GTK_ENTRY (widget);
  paste_struct.position = position;

  g_object_ref (paste_struct.entry);
  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_request_text (clipboard,
    gtk_entry_accessible_paste_received, &paste_struct);
}

static void
gtk_entry_accessible_paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer     data)
{
  GtkEntryAccessiblePaste* paste_struct = (GtkEntryAccessiblePaste *)data;

  if (text)
    gtk_editable_insert_text (GTK_EDITABLE (paste_struct->entry), text, -1,
       &(paste_struct->position));

  g_object_unref (paste_struct->entry);
}

/* Callbacks */

static gboolean
gtk_entry_accessible_idle_notify_insert (gpointer data)
{
  GtkEntryAccessible *entry;

  entry = GTK_ENTRY_ACCESSIBLE (data);
  entry->insert_idle_handler = 0;
  gtk_entry_accessible_notify_insert (entry);

  return FALSE;
}

static void
gtk_entry_accessible_notify_insert (GtkEntryAccessible *entry)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (entry));
  if (gtk_entry_get_text_length (GTK_ENTRY (widget)) == 0)
    return;

  if (entry->signal_name_insert)
    {
      g_signal_emit_by_name (entry, 
                             entry->signal_name_insert,
                             entry->position_insert,
                             entry->length_insert);
      entry->signal_name_insert = NULL;
    }
}

/* Note arg1 returns the character at the start of the insert.
 * arg2 returns the number of characters inserted.
 */
static void 
_gtk_entry_accessible_insert_text_cb (GtkEntry *entry, 
                            gchar    *arg1, 
                            gint     arg2,
                            gpointer arg3)
{
  AtkObject *accessible;
  GtkEntryAccessible *entry_accessible;
  gint *position = (gint *) arg3;

  if (arg2 == 0)
    return;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (entry));
  entry_accessible = GTK_ENTRY_ACCESSIBLE (accessible);
  if (!entry_accessible->signal_name_insert)
    {
      entry_accessible->signal_name_insert = "text_changed::insert";
      entry_accessible->position_insert = *position;
      entry_accessible->length_insert = g_utf8_strlen(arg1, arg2);
    }
  /*
   * The signal will be emitted when the cursor position is updated.
   * or in an idle handler if it not updated.
   */
   if (entry_accessible->insert_idle_handler == 0)
     entry_accessible->insert_idle_handler = gdk_threads_add_idle (gtk_entry_accessible_idle_notify_insert, entry_accessible);
}

static gunichar 
gtk_entry_accessible_get_character_at_offset (AtkText *text,
                                    gint     offset)
{
  GtkWidget *widget;
  GtkEntryAccessible *entry;
  gchar *string;
  gchar *index;
  gunichar unichar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  entry = GTK_ENTRY_ACCESSIBLE (text);
  string = gail_text_util_get_substring (entry->textutil, 0, -1);
  if (offset >= g_utf8_strlen (string, -1))
    {
      unichar = '\0';
    }
  else
    {
      index = g_utf8_offset_to_pointer (string, offset);

      unichar = g_utf8_get_char(index);
    }

  g_free(string);
  return unichar;
}

static void
gtk_entry_accessible_notify_delete (GtkEntryAccessible *entry)
{
  if (entry->signal_name_delete)
    {
      g_signal_emit_by_name (entry, 
                             entry->signal_name_delete,
                             entry->position_delete,
                             entry->length_delete);
      entry->signal_name_delete = NULL;
    }
}

/* Note arg1 returns the start of the delete range, arg2 returns the
 * end of the delete range if multiple characters are deleted.	
 */
static void 
_gtk_entry_accessible_delete_text_cb (GtkEntry *entry, 
                            gint      arg1, 
                            gint      arg2)
{
  AtkObject *accessible;
  GtkEntryAccessible *entry_accessible;

  /*
   * Zero length text deleted so ignore
   */
  if (arg2 - arg1 == 0)
    return;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (entry));
  entry_accessible = GTK_ENTRY_ACCESSIBLE (accessible);
  if (!entry_accessible->signal_name_delete)
    {
      entry_accessible->signal_name_delete = "text_changed::delete";
      entry_accessible->position_delete = arg1;
      entry_accessible->length_delete = arg2 - arg1;
    }
  gtk_entry_accessible_notify_delete (entry_accessible);
}

static void
_gtk_entry_accessible_changed_cb (GtkEntry *entry)
{
  AtkObject *accessible;
  GtkEntryAccessible *gtk_entry_accessible;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (entry));

  gtk_entry_accessible = GTK_ENTRY_ACCESSIBLE (accessible);

  text_setup (gtk_entry_accessible, entry);
}

static gboolean 
check_for_selection_change (GtkEntryAccessible   *entry,
                            GtkEntry    *gtk_entry)
{
  gboolean selected, ret_val = FALSE;
  gint start_pos, end_pos;

  selected = gtk_editable_get_selection_bounds (GTK_EDITABLE (gtk_entry),
                                                &start_pos, &end_pos);
  if (selected)
    {
      if (end_pos != entry->cursor_position ||
          start_pos != entry->selection_bound)
        /*
         * This check is here as this function can be called
         * for notification of selection_bound and current_pos.
         * The values of current_pos and selection_bound may be the same 
         * for both notifications and we only want to generate one
         * text_selection_changed signal.
         */
        ret_val = TRUE;
    }
  else 
    {
      /* We had a selection */
      ret_val = (entry->cursor_position != entry->selection_bound);
    }
  entry->cursor_position = end_pos;
  entry->selection_bound = start_pos;

  return ret_val;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_entry_accessible_do_action;
  iface->get_n_actions = gtk_entry_accessible_get_n_actions;
  iface->get_keybinding = gtk_entry_accessible_get_keybinding;
  iface->get_name = gtk_entry_accessible_action_get_name;
}

static gboolean
gtk_entry_accessible_do_action (AtkAction *action,
                      gint      i)
{
  GtkEntryAccessible *entry;
  GtkWidget *widget;
  gboolean return_value = TRUE;

  entry = GTK_ENTRY_ACCESSIBLE (action);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    /*
     * State is defunct
     */
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  switch (i)
    {
    case 0:
      if (entry->action_idle_handler)
        return_value = FALSE;
      else
        entry->action_idle_handler = gdk_threads_add_idle (idle_do_action, entry);
      break;
    default:
      return_value = FALSE;
      break;
    }
  return return_value; 
}

static gboolean
idle_do_action (gpointer data)
{
  GtkEntryAccessible *entry;
  GtkWidget *widget;

  entry = GTK_ENTRY_ACCESSIBLE (data);
  entry->action_idle_handler = 0;
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (entry));
  if (widget == NULL /* State is defunct */ ||
      !gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_activate (widget);

  return FALSE;
}

static gint
gtk_entry_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar*
gtk_entry_accessible_get_keybinding (AtkAction *action,
                           gint      i)
{
  GtkEntryAccessible *entry;
  gchar *return_value = NULL;

  entry = GTK_ENTRY_ACCESSIBLE (action);
  switch (i)
    {
    case 0:
      {
        /*
         * We look for a mnemonic on the label
         */
        GtkWidget *widget;
        GtkWidget *label;
        AtkRelationSet *set;
        AtkRelation *relation;
        GPtrArray *target;
        gpointer target_object;
        guint key_val; 

        entry = GTK_ENTRY_ACCESSIBLE (action);
        widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (entry));
        if (widget == NULL)
          /*
           * State is defunct
           */
          return NULL;

        /* Find labelled-by relation */

        set = atk_object_ref_relation_set (ATK_OBJECT (action));
        if (!set)
          return NULL;
        label = NULL;
        relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
        if (relation)
          {              
            target = atk_relation_get_target (relation);
          
            target_object = g_ptr_array_index (target, 0);
            label = gtk_accessible_get_widget (GTK_ACCESSIBLE (target_object));
          }

        g_object_unref (set);

        if (GTK_IS_LABEL (label))
          {
            key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label)); 
            if (key_val != GDK_KEY_VoidSymbol)
              return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
          }
        g_free (entry->activate_keybinding);
        entry->activate_keybinding = return_value;
        break;
      }
    default:
      break;
    }
  return return_value; 
}

static const gchar*
gtk_entry_accessible_action_get_name (AtkAction *action,
                            gint      i)
{
  const gchar *return_value;

  switch (i)
    {
    case 0:
      return_value = "activate";
      break;
    default:
      return_value = NULL;
      break;
  }
  return return_value; 
}
