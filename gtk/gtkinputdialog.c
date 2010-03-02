/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * gtkinputdialog.c
 *
 * Copyright 1997 Owen Taylor <owt1@cornell.edu>
 *
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdlib.h>

#include "gdk/gdkkeysyms.h"

#undef GTK_DISABLE_DEPRECATED /* GtkOptionMenu */

#include "gtkinputdialog.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtknotebook.h"
#include "gtkoptionmenu.h"
#include "gtkscrolledwindow.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtkvbox.h"

#include "gtkintl.h"
#include "gtkalias.h"

typedef struct _GtkInputDialogPrivate GtkInputDialogPrivate;
typedef struct _GtkInputKeyInfo       GtkInputKeyInfo;

struct _GtkInputDialogPrivate
{
  GtkWidget *device_menu;
  GtkWidget *device_optionmenu;
  GtkWidget *no_devices_label;
  GtkWidget *main_vbox;
};

struct _GtkInputKeyInfo
{
  gint       index;
  GtkWidget *entry;
  GtkInputDialog *inputd;
};

enum
{
  ENABLE_DEVICE,
  DISABLE_DEVICE,
  LAST_SIGNAL
};


#define AXIS_LIST_WIDTH 160
#define AXIS_LIST_HEIGHT 175

#define KEYS_LIST_WIDTH 200
#define KEYS_LIST_HEIGHT 175

/* Forward declarations */

static void gtk_input_dialog_screen_changed   (GtkWidget           *widget,
					       GdkScreen           *previous_screen);
static void gtk_input_dialog_set_device       (GtkWidget           *widget,
					       gpointer             data);
static void gtk_input_dialog_set_mapping_mode (GtkWidget           *w,
					       gpointer             data);
static void gtk_input_dialog_set_axis         (GtkWidget           *widget,
					       gpointer             data);
static void gtk_input_dialog_fill_axes        (GtkInputDialog      *inputd,
					       GdkDevice           *info);
static void gtk_input_dialog_set_key          (GtkInputKeyInfo     *key,
					       guint                keyval,
					       GdkModifierType      modifiers);
static gboolean gtk_input_dialog_key_press    (GtkWidget           *widget,
					       GdkEventKey         *event,
					       GtkInputKeyInfo     *key);
static void gtk_input_dialog_clear_key        (GtkWidget           *widget,
					       GtkInputKeyInfo     *key);
static void gtk_input_dialog_destroy_key      (GtkWidget           *widget,
					       GtkInputKeyInfo     *key);
static void gtk_input_dialog_fill_keys        (GtkInputDialog      *inputd,
					       GdkDevice           *info);

static guint input_dialog_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkInputDialog, gtk_input_dialog, GTK_TYPE_DIALOG)

static GtkInputDialogPrivate *
gtk_input_dialog_get_private (GtkInputDialog *input_dialog)
{
  return G_TYPE_INSTANCE_GET_PRIVATE (input_dialog, 
				      GTK_TYPE_INPUT_DIALOG, 
				      GtkInputDialogPrivate);
}

static GtkInputDialog *
input_dialog_from_widget (GtkWidget *widget)
{
  GtkWidget *toplevel;
  
  if (GTK_IS_MENU_ITEM (widget))
    {
      GtkMenu *menu = GTK_MENU (widget->parent);
      widget = gtk_menu_get_attach_widget (menu);
    }

  toplevel = gtk_widget_get_toplevel (widget);
  return GTK_INPUT_DIALOG (toplevel);
}

static void
gtk_input_dialog_class_init (GtkInputDialogClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
  
  widget_class->screen_changed = gtk_input_dialog_screen_changed;
  
  klass->enable_device = NULL;
  klass->disable_device = NULL;

  input_dialog_signals[ENABLE_DEVICE] =
    g_signal_new (I_("enable-device"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkInputDialogClass, enable_device),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DEVICE);

  input_dialog_signals[DISABLE_DEVICE] =
    g_signal_new (I_("disable-device"),
		  G_OBJECT_CLASS_TYPE (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkInputDialogClass, disable_device),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DEVICE);

  g_type_class_add_private (object_class, sizeof (GtkInputDialogPrivate));
}

static void
gtk_input_dialog_init (GtkInputDialog *inputd)
{
  GtkInputDialogPrivate *private = gtk_input_dialog_get_private (inputd);
  GtkDialog *dialog = GTK_DIALOG (inputd);
  GtkWidget *util_box;
  GtkWidget *label;
  GtkWidget *mapping_menu;
  GtkWidget *menuitem;
  GtkWidget *notebook;

  gtk_widget_push_composite_child ();

  gtk_window_set_title (GTK_WINDOW (inputd), _("Input"));

  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);

  /* main vbox */

  private->main_vbox = gtk_vbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (private->main_vbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (inputd)->vbox), private->main_vbox,
		      TRUE, TRUE, 0);

  private->no_devices_label = gtk_label_new (_("No extended input devices"));
  gtk_container_set_border_width (GTK_CONTAINER (private->main_vbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (inputd)->vbox),
		      private->no_devices_label,
		      TRUE, TRUE, 0);

  /* menu for selecting device */

  private->device_menu = gtk_menu_new ();

  util_box = gtk_hbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (private->main_vbox), util_box, FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic (_("_Device:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 0);

  private->device_optionmenu = gtk_option_menu_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), private->device_optionmenu);
  gtk_box_pack_start (GTK_BOX (util_box), private->device_optionmenu, TRUE, TRUE, 0);
  gtk_widget_show (private->device_optionmenu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (private->device_optionmenu), private->device_menu);

  gtk_widget_show (label);

  /* Device options */

  /* mapping mode option menu */

  mapping_menu = gtk_menu_new ();

  menuitem = gtk_menu_item_new_with_label(_("Disabled"));
  gtk_menu_shell_append (GTK_MENU_SHELL (mapping_menu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (gtk_input_dialog_set_mapping_mode),
		    GINT_TO_POINTER (GDK_MODE_DISABLED));

  menuitem = gtk_menu_item_new_with_label(_("Screen"));
  gtk_menu_shell_append (GTK_MENU_SHELL (mapping_menu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (gtk_input_dialog_set_mapping_mode),
		    GINT_TO_POINTER (GDK_MODE_SCREEN));

  menuitem = gtk_menu_item_new_with_label(_("Window"));
  gtk_menu_shell_append (GTK_MENU_SHELL (mapping_menu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (gtk_input_dialog_set_mapping_mode),
		    GINT_TO_POINTER (GDK_MODE_WINDOW));

  label = gtk_label_new_with_mnemonic (_("_Mode:"));
  gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 0);
  
  inputd->mode_optionmenu = gtk_option_menu_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), inputd->mode_optionmenu);
  gtk_box_pack_start (GTK_BOX (util_box), inputd->mode_optionmenu, FALSE, FALSE, 0);
  gtk_widget_show (inputd->mode_optionmenu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (inputd->mode_optionmenu), mapping_menu);

  gtk_widget_show(label);

  gtk_widget_show (util_box);

  /* Notebook */

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (private->main_vbox), notebook, TRUE, TRUE, 0);
  gtk_widget_show (notebook);
      
  /*  The axis listbox  */

  label = gtk_label_new (_("Axes"));

  inputd->axis_listbox = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (inputd->axis_listbox), 12);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(inputd->axis_listbox),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      
  gtk_widget_set_size_request (inputd->axis_listbox,
			       AXIS_LIST_WIDTH, AXIS_LIST_HEIGHT);
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
			    inputd->axis_listbox, label);

  gtk_widget_show (inputd->axis_listbox);

  inputd->axis_list = NULL;

  /* Keys listbox */

  label = gtk_label_new (_("Keys"));

  inputd->keys_listbox = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (inputd->keys_listbox), 12);
  gtk_widget_set_size_request (inputd->keys_listbox,
			       KEYS_LIST_WIDTH, KEYS_LIST_HEIGHT);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (inputd->keys_listbox),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), 
			    inputd->keys_listbox, label);

  gtk_widget_show (inputd->keys_listbox);

  inputd->keys_list = NULL;

  inputd->save_button = gtk_button_new_from_stock (GTK_STOCK_SAVE);
  gtk_widget_set_can_default (inputd->save_button, TRUE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(inputd)->action_area),
		      inputd->save_button, TRUE, TRUE, 0);
  gtk_widget_show (inputd->save_button);

  inputd->close_button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_widget_set_can_default (inputd->close_button, TRUE);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(inputd)->action_area),
		      inputd->close_button, TRUE, TRUE, 0);

  gtk_widget_show (inputd->close_button);
  gtk_widget_grab_default (inputd->close_button);

  gtk_widget_pop_composite_child ();

  gtk_input_dialog_screen_changed (GTK_WIDGET (inputd), NULL);

  _gtk_dialog_set_ignore_separator (dialog, TRUE);
}

static void
gtk_input_dialog_screen_changed (GtkWidget *widget,
				 GdkScreen *previous_screen)
{
  GtkInputDialog *inputd = GTK_INPUT_DIALOG (widget);
  GtkInputDialogPrivate *private = gtk_input_dialog_get_private (inputd);
  
  GList *device_info = NULL;
  GdkDevice *core_pointer = NULL;
  GList *tmp_list;

  if (gtk_widget_has_screen (widget))
    {
      GdkDisplay *display;
      
      display = gtk_widget_get_display (widget);
      device_info = gdk_display_list_devices (display);
      core_pointer = gdk_display_get_core_pointer (display);
    }

  inputd->current_device = NULL;
  gtk_container_foreach (GTK_CONTAINER (private->device_menu),
			 (GtkCallback)gtk_widget_destroy, NULL);
  
  if (g_list_length(device_info) <= 1) /* only core device */
    {
      gtk_widget_hide (private->main_vbox);
      gtk_widget_show (private->no_devices_label);
      gtk_widget_set_sensitive(inputd->save_button, FALSE);
    }
  else
    {
      gtk_widget_show (private->main_vbox);
      gtk_widget_hide (private->no_devices_label);
      gtk_widget_set_sensitive(inputd->save_button, TRUE);

      for (tmp_list = device_info; tmp_list; tmp_list = tmp_list->next)
	{
	  GdkDevice *info = tmp_list->data;
	  if (info != core_pointer)
	    {
	      GtkWidget *menuitem;
	      
	      menuitem = gtk_menu_item_new_with_label (info->name);
	      
	      gtk_menu_shell_append (GTK_MENU_SHELL (private->device_menu),
				     menuitem);
	      gtk_widget_show (menuitem);
	      g_signal_connect (menuitem, "activate",
				G_CALLBACK (gtk_input_dialog_set_device),
				info);
	    }
	}
      
      gtk_input_dialog_set_device (widget, device_info->data);
      gtk_option_menu_set_history (GTK_OPTION_MENU (private->device_optionmenu), 0);
    }
}
     
GtkWidget*
gtk_input_dialog_new (void)
{
  GtkInputDialog *inputd;

  inputd = g_object_new (GTK_TYPE_INPUT_DIALOG, NULL);

  return GTK_WIDGET (inputd);
}

static void
gtk_input_dialog_set_device (GtkWidget *w,
			     gpointer   data)
{
  GdkDevice *device = data;
  GtkInputDialog *inputd = input_dialog_from_widget (w);

  inputd->current_device = device;

  gtk_input_dialog_fill_axes (inputd, device);
  gtk_input_dialog_fill_keys (inputd, device);

  gtk_option_menu_set_history (GTK_OPTION_MENU (inputd->mode_optionmenu),
			       device->mode);
}

static void
gtk_input_dialog_set_mapping_mode (GtkWidget *w,
				   gpointer   data)
{
  GtkInputDialog *inputd = input_dialog_from_widget (w);
  GdkDevice *info = inputd->current_device;
  GdkInputMode old_mode;
  GdkInputMode mode = GPOINTER_TO_INT (data);

  if (!info)
    return;
  
  old_mode = info->mode;

  if (mode != old_mode)
    {
      if (gdk_device_set_mode (info, mode))
	{
	  if (mode == GDK_MODE_DISABLED)
	    g_signal_emit (inputd,
			   input_dialog_signals[DISABLE_DEVICE],
			   0,
			   info);
	  else
	    g_signal_emit (inputd,
			   input_dialog_signals[ENABLE_DEVICE],
			   0,
			   info);
	}
      else
	gtk_option_menu_set_history (GTK_OPTION_MENU (inputd->mode_optionmenu),
				     old_mode);

      /* FIXME: error dialog ? */
    }
}

static void
gtk_input_dialog_set_axis (GtkWidget *w,
			   gpointer   data)
{
  GdkAxisUse use = GPOINTER_TO_INT(data) & 0xFFFF;
  GdkAxisUse old_use;
  GdkAxisUse *new_axes;
  GtkInputDialog *inputd = input_dialog_from_widget (w);
  GdkDevice *info = inputd->current_device;

  gint axis = (GPOINTER_TO_INT(data) >> 16) - 1;
  gint old_axis;
  int i;

  if (!info)
    return;

  new_axes = g_new (GdkAxisUse, info->num_axes);
  old_axis = -1;
  for (i=0;i<info->num_axes;i++)
    {
      new_axes[i] = info->axes[i].use;
      if (info->axes[i].use == use)
	old_axis = i;
    }

  if (axis != -1)
    old_use = info->axes[axis].use;
  else
    old_use = GDK_AXIS_IGNORE;

  if (axis == old_axis) {
    g_free (new_axes);
    return;
  }

  /* we must always have an x and a y axis */
  if ((axis == -1 && (use == GDK_AXIS_X || use == GDK_AXIS_Y)) ||
      (old_axis == -1 && (old_use == GDK_AXIS_X || old_use == GDK_AXIS_Y)))
    {
      gtk_option_menu_set_history (
	        GTK_OPTION_MENU (inputd->axis_items[use]),
		old_axis + 1);
    }
  else
    {
      if (axis != -1)
	gdk_device_set_axis_use (info, axis, use);

      if (old_axis != -1)
	gdk_device_set_axis_use (info, old_axis, old_use);

      if (old_use != GDK_AXIS_IGNORE)
	{
	  gtk_option_menu_set_history (
		GTK_OPTION_MENU (inputd->axis_items[old_use]),
		old_axis + 1);
	}
    }

  g_free (new_axes);
}

static void
gtk_input_dialog_fill_axes(GtkInputDialog *inputd, GdkDevice *info)
{
  static const char *const axis_use_strings[GDK_AXIS_LAST] =
  {
    "",
    N_("_X:"),
    N_("_Y:"),
    N_("_Pressure:"),
    N_("X _tilt:"),
    N_("Y t_ilt:"),
    N_("_Wheel:")
  };

  int i,j;
  GtkWidget *menu;
  GtkWidget *option_menu;
  GtkWidget *label;
  GtkWidget *viewport;
  GtkWidget *old_child;

  /* remove all the old items */
  if (inputd->axis_list)
    {
      gtk_widget_hide (inputd->axis_list);	/* suppress resizes (or get warnings) */
      gtk_widget_destroy (inputd->axis_list);
    }
  inputd->axis_list = gtk_table_new (GDK_AXIS_LAST, 2, 0);
  gtk_table_set_row_spacings (GTK_TABLE (inputd->axis_list), 6);
  gtk_table_set_col_spacings (GTK_TABLE (inputd->axis_list), 12);
  
  viewport = gtk_viewport_new (NULL, NULL);
  old_child = gtk_bin_get_child (GTK_BIN (inputd->axis_listbox));
  if (old_child != NULL)
    gtk_widget_destroy (old_child);
  gtk_container_add (GTK_CONTAINER (inputd->axis_listbox), viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), inputd->axis_list);
  gtk_widget_show (inputd->axis_list);

  gtk_widget_realize (inputd->axis_list);
  gdk_window_set_background (inputd->axis_list->window,
			     &inputd->axis_list->style->base[GTK_STATE_NORMAL]);

  for (i=GDK_AXIS_X;i<GDK_AXIS_LAST;i++)
    {
      /* create the label */

      label = gtk_label_new_with_mnemonic (_(axis_use_strings[i]));
      gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
      gtk_table_attach (GTK_TABLE (inputd->axis_list), label, 0, 1, i, i+1, 
                        GTK_FILL, 0, 2, 2);

      /* and the use option menu */
      menu = gtk_menu_new();

      for (j = -1; j < info->num_axes; j++)
	{
	  char buffer[16];
	  GtkWidget *menu_item;

	  if (j == -1)
	    menu_item = gtk_menu_item_new_with_label (_("none"));
	  else
	    {
	      g_snprintf (buffer, sizeof (buffer), "%d", j+1);
	      menu_item = gtk_menu_item_new_with_label (buffer);
	    }
	  g_signal_connect (menu_item, "activate",
			    G_CALLBACK (gtk_input_dialog_set_axis),
			    GINT_TO_POINTER (0x10000 * (j + 1) + i));
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	}

      inputd->axis_items[i] = option_menu = gtk_option_menu_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
      gtk_table_attach (GTK_TABLE (inputd->axis_list), option_menu, 
			1, 2, i, i+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);

      gtk_widget_show (option_menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
      for (j = 0; j < info->num_axes; j++)
	if (info->axes[j].use == (GdkAxisUse) i)
	  {
	    gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), j+1);
	    break;
	  }

      gtk_widget_show (label);
    }
}

static void 
gtk_input_dialog_clear_key (GtkWidget *widget, GtkInputKeyInfo *key)
{
  if (!key->inputd->current_device)
    return;
  
  gtk_entry_set_text (GTK_ENTRY(key->entry), _("(disabled)"));
  gdk_device_set_key (key->inputd->current_device, key->index, 0, 0);
}

static void 
gtk_input_dialog_set_key (GtkInputKeyInfo *key,
			  guint keyval, GdkModifierType modifiers)
{
  GString *str;
  gchar chars[2];

  if (keyval)
    {
      str = g_string_new (NULL);
      
      if (modifiers & GDK_SHIFT_MASK)
	g_string_append (str, "Shift+");
      if (modifiers & GDK_CONTROL_MASK)
	g_string_append (str, "Ctrl+");
      if (modifiers & GDK_MOD1_MASK)
	g_string_append (str, "Alt+");
      
      if ((keyval >= 0x20) && (keyval <= 0xFF))
	{
	  chars[0] = keyval;
	  chars[1] = 0;
	  g_string_append (str, chars);
	}
      else
	g_string_append (str, _("(unknown)"));
      gtk_entry_set_text (GTK_ENTRY(key->entry), str->str);

      g_string_free (str, TRUE);
    }
  else
    {
      gtk_entry_set_text (GTK_ENTRY(key->entry), _("(disabled)"));
    }
}

static gboolean
gtk_input_dialog_key_press (GtkWidget *widget, 
			    GdkEventKey *event,
			    GtkInputKeyInfo *key)
{
  if (!key->inputd->current_device)
    return FALSE;
  
  gtk_input_dialog_set_key (key, event->keyval, event->state & 0xFF);
  gdk_device_set_key (key->inputd->current_device, key->index, 
		      event->keyval, event->state & 0xFF);

  g_signal_stop_emission_by_name (widget, "key-press-event");
  
  return TRUE;
}

static void 
gtk_input_dialog_destroy_key (GtkWidget *widget, GtkInputKeyInfo *key)
{
  g_free (key);
}

static void
gtk_input_dialog_fill_keys(GtkInputDialog *inputd, GdkDevice *info)
{
  int i;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *viewport;
  GtkWidget *old_child;

  char buffer[32];
  
  /* remove all the old items */
  if (inputd->keys_list)
    {
      gtk_widget_hide (inputd->keys_list);	/* suppress resizes (or get warnings) */
      gtk_widget_destroy (inputd->keys_list);
    }

  inputd->keys_list = gtk_table_new (info->num_keys, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (inputd->keys_list), 6);
  gtk_table_set_col_spacings (GTK_TABLE (inputd->keys_list), 12);
  
  viewport = gtk_viewport_new (NULL, NULL);
  old_child = gtk_bin_get_child (GTK_BIN (inputd->keys_listbox));
  if (old_child != NULL)
    gtk_widget_destroy (old_child);
  gtk_container_add (GTK_CONTAINER (inputd->keys_listbox), viewport);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_widget_show (viewport);
  gtk_container_add (GTK_CONTAINER (viewport), inputd->keys_list);
  gtk_widget_show (inputd->keys_list);

  gtk_widget_realize (inputd->keys_list);
  gdk_window_set_background (inputd->keys_list->window,
			     &inputd->keys_list->style->base[GTK_STATE_NORMAL]);

  for (i=0;i<info->num_keys;i++)
    {
      GtkInputKeyInfo *key = g_new (GtkInputKeyInfo, 1);
      key->index = i;
      key->inputd = inputd;

      /* create the label */

      g_snprintf (buffer, sizeof (buffer), "_%d:", i+1);
      label = gtk_label_new_with_mnemonic (buffer);
      gtk_table_attach (GTK_TABLE (inputd->keys_list), label, 0, 1, i, i+1, 
			GTK_FILL, 0, 2, 2);
      gtk_widget_show (label);

      /* the entry */

      hbox = gtk_hbox_new (FALSE, 6);
      gtk_table_attach (GTK_TABLE (inputd->keys_list), hbox, 1, 2, i, i+1, 
                        GTK_EXPAND | GTK_FILL, 0, 2, 2);
      gtk_widget_show (hbox);

      key->entry = gtk_entry_new ();
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), key->entry);
      gtk_box_pack_start (GTK_BOX (hbox), key->entry, TRUE, TRUE, 0);
      gtk_widget_show (key->entry);

      g_signal_connect (key->entry, "key-press-event",
			G_CALLBACK (gtk_input_dialog_key_press), key);
      g_signal_connect (key->entry, "destroy",
			G_CALLBACK (gtk_input_dialog_destroy_key), key);
      
      /* and clear button */

      button = gtk_button_new_with_mnemonic (_("Cl_ear"));
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
      gtk_widget_show (button);

      g_signal_connect (button, "clicked",
			G_CALLBACK (gtk_input_dialog_clear_key), key);

      gtk_input_dialog_set_key (key, info->keys[i].keyval,
				info->keys[i].modifiers);
    }
}

#define __GTK_INPUTDIALOG_C__
#include "gtkaliasdef.c"
