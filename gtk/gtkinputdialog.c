/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * gtkinputdialog.c
 *
 * Copyright 1997 Owen Taylor <owt1@cornell.edu>
 *
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "gdk/gdkkeysyms.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkhbox.h"
#include "gtkhseparator.h"
#include "gtkinputdialog.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtknotebook.h"
#include "gtkoptionmenu.h"
#include "gtkscrolledwindow.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtkvbox.h"

#include "gtkintl.h"

typedef struct {
  gint       index;
  GtkWidget *entry;
  GtkInputDialog *inputd;
} GtkInputKeyInfo;

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

static void gtk_input_dialog_class_init (GtkInputDialogClass *klass);
static void gtk_input_dialog_init (GtkInputDialog *inputd);
static GdkDeviceInfo *gtk_input_dialog_get_device_info(guint32 deviceid);
static void gtk_input_dialog_set_device(GtkWidget *widget, gpointer data);
static void gtk_input_dialog_finalize (GtkObject *object);
static void gtk_input_dialog_set_mapping_mode(GtkWidget *w,
					      gpointer data);
static void gtk_input_dialog_set_axis(GtkWidget *widget, gpointer data);
static void gtk_input_dialog_fill_axes (GtkInputDialog *inputd,
					GdkDeviceInfo *info);
static void gtk_input_dialog_set_key (GtkInputKeyInfo *key,
				      guint keyval, 
				      GdkModifierType modifiers);
static gint gtk_input_dialog_key_press (GtkWidget *widget, 
					GdkEventKey *event,
					GtkInputKeyInfo *key);
static void gtk_input_dialog_clear_key (GtkWidget *widget, 
					GtkInputKeyInfo *key);
static void gtk_input_dialog_destroy_key (GtkWidget *widget, 
					  GtkInputKeyInfo *key);
static void gtk_input_dialog_fill_keys (GtkInputDialog *inputd,
					GdkDeviceInfo *info);

static GtkObjectClass *parent_class = NULL;
static guint input_dialog_signals[LAST_SIGNAL] = { 0 };

static GdkDeviceInfo *
gtk_input_dialog_get_device_info(guint32 deviceid)
{
  GList *tmp_list = gdk_input_list_devices();
  while (tmp_list)
    {
      if (((GdkDeviceInfo *)tmp_list->data)->deviceid == deviceid)
	return (GdkDeviceInfo *)tmp_list->data;
      tmp_list = tmp_list->next;
    }

  return NULL;
}

GtkType
gtk_input_dialog_get_type (void)
{
  static GtkType input_dialog_type = 0;

  if (!input_dialog_type)
    {
      static const GtkTypeInfo input_dialog_info =
      {
	"GtkInputDialog",
	sizeof (GtkInputDialog),
	sizeof (GtkInputDialogClass),
	(GtkClassInitFunc) gtk_input_dialog_class_init,
	(GtkObjectInitFunc) gtk_input_dialog_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      input_dialog_type = gtk_type_unique (GTK_TYPE_DIALOG,
					   &input_dialog_info);
    }

  return input_dialog_type;
}

static void
gtk_input_dialog_class_init (GtkInputDialogClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_DIALOG);

  input_dialog_signals[ENABLE_DEVICE] =
    gtk_signal_new ("enable_device",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkInputDialogClass, enable_device),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  input_dialog_signals[DISABLE_DEVICE] =
    gtk_signal_new ("disable_device",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkInputDialogClass, disable_device),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, input_dialog_signals,
				LAST_SIGNAL);


  object_class->finalize = gtk_input_dialog_finalize;
  klass->enable_device = NULL;
  klass->disable_device = NULL;
}

static void
gtk_input_dialog_init (GtkInputDialog *inputd)
{
  GtkWidget *vbox;
  GtkWidget *util_box;
  GtkWidget *label;
  GtkWidget *device_menu;
  GtkWidget *mapping_menu;
  GtkWidget *menuitem;
  GtkWidget *optionmenu;
  GtkWidget *separator;
  GtkWidget *notebook;

  GList *tmp_list;
  GList *device_info;

  device_info = gdk_input_list_devices();

  /* shell and main vbox */

  gtk_window_set_title (GTK_WINDOW (inputd), _("Input"));

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (inputd)->vbox), vbox, TRUE, TRUE, 0);

  if (g_list_length(device_info) <= 1) /* only core device */
    {
      label = gtk_label_new (_("No input devices"));
      gtk_container_add (GTK_CONTAINER (vbox), label);

      gtk_widget_show (label);
    }
  else
    {
      /* menu for selecting device */

      device_menu = gtk_menu_new ();

      for (tmp_list = device_info; tmp_list; tmp_list = tmp_list->next) {
	GdkDeviceInfo *info = (GdkDeviceInfo *)(tmp_list->data);
	if (info->deviceid != GDK_CORE_POINTER)
	  {
	    menuitem = gtk_menu_item_new_with_label(info->name);

	    gtk_menu_append(GTK_MENU(device_menu),menuitem);
	    gtk_widget_show(menuitem);
	    gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
	    gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				(GtkSignalFunc) gtk_input_dialog_set_device,
				GUINT_TO_POINTER(info->deviceid));
	  }
      }

      util_box = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

      label = gtk_label_new(_("Device:"));
      gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);

      optionmenu = gtk_option_menu_new ();
      gtk_box_pack_start (GTK_BOX (util_box), optionmenu, TRUE, TRUE, 2);
      gtk_widget_show (optionmenu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), device_menu);

      gtk_widget_show (label);

      /* Device options */

      /* mapping mode option menu */

      mapping_menu = gtk_menu_new ();

      menuitem = gtk_menu_item_new_with_label(_("Disabled"));
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  GINT_TO_POINTER (GDK_MODE_DISABLED));

      menuitem = gtk_menu_item_new_with_label(_("Screen"));
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  GINT_TO_POINTER (GDK_MODE_SCREEN));

      menuitem = gtk_menu_item_new_with_label(_("Window"));
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  GINT_TO_POINTER (GDK_MODE_WINDOW));

      label = gtk_label_new(_("Mode: "));
      gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);

      inputd->mode_optionmenu = gtk_option_menu_new ();
      gtk_box_pack_start (GTK_BOX (util_box), inputd->mode_optionmenu, FALSE, FALSE, 2);
      gtk_widget_show (inputd->mode_optionmenu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (inputd->mode_optionmenu), mapping_menu);

      gtk_widget_show(label);

      gtk_widget_show (util_box);

      util_box = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX(vbox), util_box, FALSE, FALSE, 0);

      gtk_widget_show (label);
      gtk_widget_show (util_box);

      separator = gtk_hseparator_new();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      /* Notebook */

      notebook = gtk_notebook_new ();
      gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
      gtk_widget_show (notebook);
      
      /*  The axis listbox  */

      label = gtk_label_new (_("Axes"));

      inputd->axis_listbox = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(inputd->axis_listbox),
				      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      
      gtk_widget_set_usize (inputd->axis_listbox, AXIS_LIST_WIDTH, AXIS_LIST_HEIGHT);
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
				inputd->axis_listbox, label);

      gtk_widget_show (inputd->axis_listbox);

      inputd->axis_list = 0;

      /* Keys listbox */

      label = gtk_label_new (_("Keys"));

      inputd->keys_listbox = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_set_usize (inputd->keys_listbox, KEYS_LIST_WIDTH, KEYS_LIST_HEIGHT);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(inputd->keys_listbox),
				      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_notebook_append_page (GTK_NOTEBOOK(notebook), 
				inputd->keys_listbox, label);

      gtk_widget_show (inputd->keys_listbox);

      inputd->keys_list = 0;

      /* ...set_device expects to get input dialog from widget user data */
      gtk_object_set_user_data (GTK_OBJECT (inputd), inputd);
      gtk_input_dialog_set_device(GTK_WIDGET(inputd), 
          GUINT_TO_POINTER (((GdkDeviceInfo *)device_info->data)->deviceid));

    }

  /* We create the save button in any case, so that clients can 
     connect to it, without paying attention to whether it exits */
  inputd->save_button = gtk_button_new_with_label (_("Save"));
  GTK_WIDGET_SET_FLAGS (inputd->save_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(inputd)->action_area),
		      inputd->save_button, TRUE, TRUE, 0);
  gtk_widget_show (inputd->save_button);

  if (g_list_length(device_info) <= 1) /* only core device */
    gtk_widget_set_sensitive(inputd->save_button, FALSE);

  inputd->close_button = gtk_button_new_with_label (_("Close"));
  GTK_WIDGET_SET_FLAGS (inputd->close_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(inputd)->action_area),
		      inputd->close_button, TRUE, TRUE, 0);

  gtk_widget_show (inputd->close_button);
  gtk_widget_grab_default (inputd->close_button);

  gtk_widget_show (vbox);
}


GtkWidget*
gtk_input_dialog_new (void)
{
  GtkInputDialog *inputd;

  inputd = gtk_type_new (GTK_TYPE_INPUT_DIALOG);

  return GTK_WIDGET (inputd);
}

static void
gtk_input_dialog_set_device(GtkWidget *widget, gpointer data)
{
  guint32 deviceid = GPOINTER_TO_UINT(data);
  GdkDeviceInfo *info;

  GtkInputDialog *inputd = GTK_INPUT_DIALOG(
                gtk_object_get_user_data(GTK_OBJECT(widget)));

  inputd->current_device = deviceid;
  info = gtk_input_dialog_get_device_info (deviceid);

  gtk_input_dialog_fill_axes(inputd, info);
  gtk_input_dialog_fill_keys(inputd, info);

  gtk_option_menu_set_history(GTK_OPTION_MENU(inputd->mode_optionmenu),
			      info->mode);
}

static void
gtk_input_dialog_finalize (GtkObject *object)
{
  /*  GtkInputDialog *inputd = GTK_INPUT_DIALOG (object); */

  /* Clean up ? */

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_input_dialog_set_mapping_mode(GtkWidget *w,
				  gpointer data)
{
  GtkInputDialog *inputd = GTK_INPUT_DIALOG(
                gtk_object_get_user_data(GTK_OBJECT(w)));
  GdkDeviceInfo *info = gtk_input_dialog_get_device_info (inputd->current_device);
  GdkInputMode old_mode = info->mode;
  GdkInputMode mode = GPOINTER_TO_INT (data);

  if (mode != old_mode)
    {
      if (gdk_input_set_mode(inputd->current_device, mode))
	{
	  if (mode == GDK_MODE_DISABLED)
	    gtk_signal_emit (GTK_OBJECT (inputd),
			     input_dialog_signals[DISABLE_DEVICE],
			     info->deviceid);
	  else
	    gtk_signal_emit (GTK_OBJECT (inputd),
			     input_dialog_signals[ENABLE_DEVICE],
			     info->deviceid);
	}
      else
	gtk_option_menu_set_history (GTK_OPTION_MENU (inputd->mode_optionmenu),
				     old_mode);

      /* FIXME: error dialog ? */
    }
}

static void
gtk_input_dialog_set_axis(GtkWidget *widget, gpointer data)
{
  GdkAxisUse use = GPOINTER_TO_INT(data) & 0xFFFF;
  GdkAxisUse old_use;
  GdkAxisUse *new_axes;
  GtkInputDialog *inputd = GTK_INPUT_DIALOG (gtk_object_get_user_data (GTK_OBJECT (widget)));
  GdkDeviceInfo *info = gtk_input_dialog_get_device_info (inputd->current_device);

  gint axis = (GPOINTER_TO_INT(data) >> 16) - 1;
  gint old_axis;
  int i;

  new_axes = g_new (GdkAxisUse, info->num_axes);
  old_axis = -1;
  for (i=0;i<info->num_axes;i++)
    {
      new_axes[i] = info->axes[i];
      if (info->axes[i] == use)
	old_axis = i;
    }

  if (axis != -1)
    old_use = info->axes[axis];
  else
    old_use = GDK_AXIS_IGNORE;

  if (axis == old_axis)
    return;

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
	new_axes[axis] = use;

      if (old_axis != -1)
	new_axes[old_axis] = old_use;

      if (old_use != GDK_AXIS_IGNORE)
	{
	  gtk_option_menu_set_history (
		GTK_OPTION_MENU (inputd->axis_items[old_use]),
		old_axis + 1);
	}
      gdk_input_set_axes (info->deviceid, new_axes);
    }

  g_free (new_axes);
}

static void
gtk_input_dialog_fill_axes(GtkInputDialog *inputd, GdkDeviceInfo *info)
{
  static const char *axis_use_strings[GDK_AXIS_LAST] =
  {
    "",
    N_("X"),
    N_("Y"),
    N_("Pressure"),
    N_("X Tilt"),
    N_("Y Tilt")
  };

  int i,j;
  GtkWidget *menu;
  GtkWidget *option_menu;
  GtkWidget *label;

  /* remove all the old items */
  if (inputd->axis_list)
    {
      gtk_widget_hide (inputd->axis_list);	/* suppress resizes (or get warnings) */
      gtk_widget_destroy (inputd->axis_list);
    }
  inputd->axis_list = gtk_table_new (GDK_AXIS_LAST, 2, 0);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (inputd->axis_listbox), 
					 inputd->axis_list);
  gtk_widget_show (inputd->axis_list);

  gtk_widget_realize (inputd->axis_list);
  gdk_window_set_background (inputd->axis_list->window,
			     &inputd->axis_list->style->base[GTK_STATE_NORMAL]);

  for (i=GDK_AXIS_X;i<GDK_AXIS_LAST;i++)
    {
      /* create the label */

      label = gtk_label_new (_(axis_use_strings[i]));
      gtk_table_attach (GTK_TABLE (inputd->axis_list), label, 0, 1, i, i+1, 
			0, 0, 2, 2);

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
	      sprintf (buffer,"%d",j+1);
	      menu_item = gtk_menu_item_new_with_label (buffer);
	    }
	  gtk_object_set_user_data (GTK_OBJECT (menu_item), inputd);
	  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			      (GtkSignalFunc) gtk_input_dialog_set_axis,
			      GINT_TO_POINTER (0x10000 * (j + 1) + i));
	  gtk_widget_show (menu_item);
	  gtk_menu_append (GTK_MENU (menu), menu_item);
	}

      inputd->axis_items[i] = option_menu = gtk_option_menu_new ();
      gtk_table_attach (GTK_TABLE (inputd->axis_list), option_menu, 
			1, 2, i, i+1, 0, 0, 2, 2);

      gtk_widget_show (option_menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
      for (j = 0; j < info->num_axes; j++)
	if (info->axes[j] == (GdkAxisUse) i)
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
  gtk_entry_set_text (GTK_ENTRY(key->entry), _("(disabled)"));
  gdk_input_set_key (key->inputd->current_device, key->index, 0, 0);
}

static void 
gtk_input_dialog_set_key (GtkInputKeyInfo *key,
			  guint keyval, GdkModifierType modifiers)
{
  GString *str;
  gchar chars[2];

  if (keyval)
    {
      str = g_string_new("");
      
      if (modifiers & GDK_SHIFT_MASK)
	g_string_append (str, "Shft+");
      if (modifiers & GDK_CONTROL_MASK)
	g_string_append (str, "Ctl+");
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

static gint
gtk_input_dialog_key_press (GtkWidget *widget, 
			    GdkEventKey *event,
			    GtkInputKeyInfo *key)
{
  gtk_input_dialog_set_key (key, event->keyval, event->state & 0xFF);
  gdk_input_set_key (key->inputd->current_device, key->index, 
		     event->keyval, event->state & 0xFF);

  gtk_signal_emit_stop_by_name (GTK_OBJECT(widget), "key_press_event");
  
  return TRUE;
}

static void 
gtk_input_dialog_destroy_key (GtkWidget *widget, GtkInputKeyInfo *key)
{
  g_free (key);
}

static void
gtk_input_dialog_fill_keys(GtkInputDialog *inputd, GdkDeviceInfo *info)
{
  int i;
  GtkWidget *label;
  GtkWidget *button;

  char buffer[32];
  
  /* remove all the old items */
  if (inputd->keys_list)
    {
      gtk_widget_hide (inputd->keys_list);	/* suppress resizes (or get warnings) */
      gtk_widget_destroy (inputd->keys_list);
    }

  inputd->keys_list = gtk_table_new (info->num_keys, 3, FALSE);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (inputd->keys_listbox), 
					 inputd->keys_list);
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

      sprintf(buffer, "%d", i+1);
      label = gtk_label_new(buffer);
      gtk_table_attach (GTK_TABLE (inputd->keys_list), label, 0, 1, i, i+1, 
			0, 0, 2, 2);
      gtk_widget_show (label);

      /* the entry */

      key->entry = gtk_entry_new ();
      gtk_table_attach (GTK_TABLE (inputd->keys_list), key->entry, 1, 2, i, i+1, 
			GTK_EXPAND | GTK_FILL , 0, 2, 2);
      gtk_widget_show (key->entry);

      gtk_signal_connect (GTK_OBJECT(key->entry), "key_press_event",
			  GTK_SIGNAL_FUNC (gtk_input_dialog_key_press), key);
      gtk_signal_connect (GTK_OBJECT(key->entry), "destroy",
			  GTK_SIGNAL_FUNC (gtk_input_dialog_destroy_key), 
			  key);
      
      /* and clear button */

      button = gtk_button_new_with_label (_("clear"));
      gtk_table_attach (GTK_TABLE (inputd->keys_list), button, 2, 3, i, i+1, 
			0, 0, 2, 2);
      gtk_widget_show (button);

      gtk_signal_connect (GTK_OBJECT(button), "clicked",
			  GTK_SIGNAL_FUNC (gtk_input_dialog_clear_key), key);

      gtk_input_dialog_set_key (key, info->keys[i].keyval,
				info->keys[i].modifiers);
    }
}
