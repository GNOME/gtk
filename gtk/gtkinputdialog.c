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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * gtkinputdialog.c
 *
 * Copyright 1997 Owen Taylor <owt1@cornell.edu>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "gdk/gdkkeysyms.h"
#include "gtkbutton.h"
#include "gtkhbox.h"
#include "gtkhseparator.h"
#include "gtkinputdialog.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkmain.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkoptionmenu.h"
#include "gtkscrolledwindow.h"
#include "gtksignal.h"
#include "gtkvbox.h"

typedef void (*GtkInputDialogSignal1) (GtkObject *object,
				  int   arg1,
				  gpointer   data);

enum
{
  ENABLE_DEVICE,
  DISABLE_DEVICE,
  LAST_SIGNAL
};


#define AXIS_LIST_WIDTH 160
#define AXIS_LIST_HEIGHT 175

/* Forward declarations */

static void gtk_input_dialog_marshal_signal1 (GtkObject      *object,
					      GtkSignalFunc   func,
					      gpointer        func_data,
					      GtkArg *args);
static void gtk_input_dialog_class_init (GtkInputDialogClass *klass);
static void gtk_input_dialog_init (GtkInputDialog *inputd);
static GdkDeviceInfo *gtk_input_dialog_get_device_info(guint32 deviceid);
static void gtk_input_dialog_set_device(GtkWidget *widget, gpointer data);
static void gtk_input_dialog_destroy (GtkObject *object);
static void gtk_input_dialog_set_mapping_mode(GtkWidget *w,
					      gpointer data);
static void gtk_input_dialog_set_axis(GtkWidget *widget, gpointer data);
static void gtk_input_dialog_fill_axes (GtkInputDialog *inputd,
					GdkDeviceInfo *info);

static GtkObjectClass *parent_class = NULL;
static gint input_dialog_signals[LAST_SIGNAL] = { 0 };

static void
gtk_input_dialog_marshal_signal1 (GtkObject      *object,
				  GtkSignalFunc   func,
				  gpointer        func_data,
				  GtkArg *args)
{
  GtkInputDialogSignal1 rfunc;

  rfunc = (GtkInputDialogSignal1) func;
  (* rfunc) (object, GTK_VALUE_INT(args[0]), func_data);
}

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

guint
gtk_input_dialog_get_type ()
{
  static guint input_dialog_type = 0;

  if (!input_dialog_type)
    {
      GtkTypeInfo input_dialog_info =
      {
	"GtkInputDialog",
	sizeof (GtkInputDialog),
	sizeof (GtkInputDialogClass),
	(GtkClassInitFunc) gtk_input_dialog_class_init,
	(GtkObjectInitFunc) gtk_input_dialog_init,
	(GtkArgFunc) NULL,
      };

      input_dialog_type = gtk_type_unique (gtk_dialog_get_type (),
					   &input_dialog_info);
    }

  return input_dialog_type;
}

static void
gtk_input_dialog_class_init (GtkInputDialogClass *klass)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) klass;

  parent_class = gtk_type_class (gtk_dialog_get_type ());

  input_dialog_signals[ENABLE_DEVICE] =
    gtk_signal_new ("enable_device",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkInputDialogClass, enable_device),
		    gtk_input_dialog_marshal_signal1,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  input_dialog_signals[DISABLE_DEVICE] =
    gtk_signal_new ("disable_device",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkInputDialogClass, disable_device),
		    gtk_input_dialog_marshal_signal1,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);

  gtk_object_class_add_signals (object_class, input_dialog_signals,
				LAST_SIGNAL);


  object_class->destroy = gtk_input_dialog_destroy;
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

  GList *tmp_list;
  GList *device_info;

  device_info = gdk_input_list_devices();

  /* shell and main vbox */

  gtk_window_set_title (GTK_WINDOW (inputd), "Input");

  vbox = gtk_vbox_new (FALSE, 4);
  gtk_container_border_width(GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (inputd)->vbox), vbox, TRUE, TRUE, 0);

  if (g_list_length(device_info) <= 1) /* only core device */
    {
      label = gtk_label_new ("No input devices");
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
				(gpointer)((long)info->deviceid));
	  }
      }

      util_box = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

      label = gtk_label_new("Device:");
      gtk_box_pack_start (GTK_BOX (util_box), label, FALSE, FALSE, 2);

      optionmenu = gtk_option_menu_new ();
      gtk_box_pack_start (GTK_BOX (util_box), optionmenu, TRUE, TRUE, 2);
      gtk_widget_show (optionmenu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (optionmenu), device_menu);

      gtk_widget_show (label);
      gtk_widget_show (util_box);

      /* Device options */

      separator = gtk_hseparator_new();
      gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);

      util_box = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX (vbox), util_box, FALSE, FALSE, 0);

      /* mapping mode option menu */

      mapping_menu = gtk_menu_new ();

      menuitem = gtk_menu_item_new_with_label("Disabled");
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  (gpointer)((long)GDK_MODE_DISABLED));

      menuitem = gtk_menu_item_new_with_label("Screen");
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  (gpointer)((long)GDK_MODE_SCREEN));

      menuitem = gtk_menu_item_new_with_label("Window");
      gtk_menu_append(GTK_MENU(mapping_menu),menuitem);
      gtk_object_set_user_data (GTK_OBJECT (menuitem), inputd);
      gtk_widget_show(menuitem);
      gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			  (GtkSignalFunc) gtk_input_dialog_set_mapping_mode,
			  (gpointer)((long)GDK_MODE_WINDOW));

      label = gtk_label_new("Mode: ");
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

      /*  The axis listbox  */

      label = gtk_label_new ("Axes");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      inputd->axis_listbox = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_set_usize (inputd->axis_listbox, AXIS_LIST_WIDTH, AXIS_LIST_HEIGHT);
      gtk_box_pack_start (GTK_BOX (vbox), inputd->axis_listbox, TRUE, TRUE, 0);
      gtk_widget_show (inputd->axis_listbox);

      inputd->axis_list = 0;

      gtk_widget_show(label);

      /* ...set_device expects to get input dialog from widget user data */
      gtk_object_set_user_data (GTK_OBJECT (inputd), inputd);
      gtk_input_dialog_set_device(GTK_WIDGET(inputd), (gpointer)((long)
			      ((GdkDeviceInfo *)device_info->data)->deviceid));
    }

  /* buttons */

  inputd->save_button = gtk_button_new_with_label ("Save");
  GTK_WIDGET_SET_FLAGS (inputd->save_button, GTK_CAN_DEFAULT);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(inputd)->action_area),
		      inputd->save_button, TRUE, TRUE, 0);
  gtk_widget_show (inputd->save_button);

  inputd->close_button = gtk_button_new_with_label ("Close");
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

  inputd = gtk_type_new (gtk_input_dialog_get_type ());

  return GTK_WIDGET (inputd);
}

static void
gtk_input_dialog_set_device(GtkWidget *widget, gpointer data)
{
  guint32 deviceid = (guint32)data;
  GdkDeviceInfo *info;

  GtkInputDialog *inputd = GTK_INPUT_DIALOG(
                gtk_object_get_user_data(GTK_OBJECT(widget)));

  inputd->current_device = deviceid;
  info = gtk_input_dialog_get_device_info((guint32)data);

  gtk_input_dialog_fill_axes(inputd, info);

  gtk_option_menu_set_history(GTK_OPTION_MENU(inputd->mode_optionmenu),
			      info->mode);
}

static void
gtk_input_dialog_destroy (GtkObject *object)
{
  /*  GtkInputDialog *inputd = GTK_INPUT_DIALOG (object); */

  /* Clean up ? */

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_input_dialog_set_mapping_mode(GtkWidget *w,
				  gpointer data)
{
  GtkInputDialog *inputd = GTK_INPUT_DIALOG(
                gtk_object_get_user_data(GTK_OBJECT(w)));
  GdkDeviceInfo *info = gtk_input_dialog_get_device_info (inputd->current_device);
  GdkInputMode old_mode = info->mode;
  GdkInputMode mode = (GdkInputMode)data;

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
  GdkAxisUse use = (GdkAxisUse)data & 0xFFFF;
  GdkAxisUse old_use;
  GdkAxisUse *new_axes;
  GtkInputDialog *inputd = GTK_INPUT_DIALOG (gtk_object_get_user_data (GTK_OBJECT (widget)));
  GdkDeviceInfo *info = gtk_input_dialog_get_device_info (inputd->current_device);

  gint axis = ((gint)data >> 16) - 1;
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
  static char *axis_use_strings[GDK_AXIS_LAST] =
  {
    "",
    "X",
    "Y",
    "Pressure",
    "X Tilt",
    "Y Tilt"
  };

  int i,j;
  GtkWidget *list_item;
  GtkWidget *menu;
  GtkWidget *option_menu;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;

  /* remove all the old items */
  if (inputd->axis_list)
    {
      gtk_widget_hide (inputd->axis_list);	/* suppress resizes (or get warnings) */
      gtk_widget_destroy (inputd->axis_list);
    }
  inputd->axis_list = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (inputd->axis_listbox), inputd->axis_list);
  gtk_widget_show (inputd->axis_list);

  gtk_widget_realize (inputd->axis_list);
  gdk_window_set_background (inputd->axis_list->window,
			     &inputd->axis_list->style->white);

  for (i=GDK_AXIS_X;i<GDK_AXIS_LAST;i++)
    {
      list_item = gtk_list_item_new();

      gtk_box_pack_start(GTK_BOX(inputd->axis_list),list_item,FALSE,FALSE,0);
      gtk_widget_show (list_item);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add(GTK_CONTAINER (list_item), vbox);

      hbox = gtk_hbox_new (FALSE, 2);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 1);

      /* create the label */

      label = gtk_label_new(axis_use_strings[i]);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 2);

      /* and the use option menu */
      menu = gtk_menu_new();

      for (j = -1; j < info->num_axes; j++)
	{
	  char buffer[16];
	  GtkWidget *menu_item;

	  if (j == -1)
	    menu_item = gtk_menu_item_new_with_label ("none");
	  else
	    {
	      sprintf (buffer,"%d",j+1);
	      menu_item = gtk_menu_item_new_with_label (buffer);
	    }
	  gtk_object_set_user_data (GTK_OBJECT (menu_item), inputd);
	  gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			      (GtkSignalFunc) gtk_input_dialog_set_axis,
			      (gpointer) ((long) (0x10000 * (j + 1) + i)));
	  gtk_widget_show (menu_item);
	  gtk_menu_append (GTK_MENU (menu), menu_item);
	}

      inputd->axis_items[i] = option_menu = gtk_option_menu_new ();
      gtk_box_pack_start (GTK_BOX (hbox), option_menu, FALSE, FALSE, 2);

      gtk_widget_show (option_menu);
      gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
      for (j = 0; j < info->num_axes; j++)
	if (info->axes[j] == (GdkAxisUse) i)
	  {
	    gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), j+1);
	    break;
	  }

      gtk_widget_show (label);

      gtk_widget_show (hbox);
      gtk_widget_show (vbox);
    }
}
