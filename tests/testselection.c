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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef GTK_DISABLE_DEPRECATED

#include "config.h"
#include <stdio.h>
#include <string.h>
#define GTK_ENABLE_BROKEN
#include "gtk/gtk.h"

typedef enum {
  SEL_TYPE_NONE,
  APPLE_PICT,
  ATOM,
  ATOM_PAIR,
  BITMAP,
  C_STRING,
  COLORMAP,
  COMPOUND_TEXT,
  DRAWABLE,
  INTEGER,
  PIXEL,
  PIXMAP,
  SPAN,
  STRING,
  TEXT,
  WINDOW,
  LAST_SEL_TYPE
} SelType;

GdkAtom seltypes[LAST_SEL_TYPE];

typedef struct _Target {
  gchar *target_name;
  SelType type;
  GdkAtom target;
  gint format;
} Target;

/* The following is a list of all the selection targets defined
   in the ICCCM */

static Target targets[] = {
  { "ADOBE_PORTABLE_DOCUMENT_FORMAT",	    STRING, 	   NULL, 8 },
  { "APPLE_PICT", 			    APPLE_PICT,    NULL, 8 },
  { "BACKGROUND",			    PIXEL,         NULL, 32 },
  { "BITMAP", 				    BITMAP,        NULL, 32 },
  { "CHARACTER_POSITION",                   SPAN, 	   NULL, 32 },
  { "CLASS", 				    TEXT, 	   NULL, 8 },
  { "CLIENT_WINDOW", 			    WINDOW, 	   NULL, 32 },
  { "COLORMAP", 			    COLORMAP,      NULL, 32 },
  { "COLUMN_NUMBER", 			    SPAN, 	   NULL, 32 },
  { "COMPOUND_TEXT", 			    COMPOUND_TEXT, NULL, 8 },
  /*  { "DELETE", "NULL", 0, ? }, */
  { "DRAWABLE", 			    DRAWABLE,      NULL, 32 },
  { "ENCAPSULATED_POSTSCRIPT", 		    STRING, 	   NULL, 8 },
  { "ENCAPSULATED_POSTSCRIPT_INTERCHANGE",  STRING, 	   NULL, 8 },
  { "FILE_NAME", 			    TEXT, 	   NULL, 8 },
  { "FOREGROUND", 			    PIXEL, 	   NULL, 32 },
  { "HOST_NAME", 			    TEXT, 	   NULL, 8 },
  /*  { "INSERT_PROPERTY", "NULL", 0, ? NULL }, */
  /*  { "INSERT_SELECTION", "NULL", 0, ? NULL }, */
  { "LENGTH", 				    INTEGER, 	   NULL, 32 },
  { "LINE_NUMBER", 			    SPAN, 	   NULL, 32 },
  { "LIST_LENGTH", 			    INTEGER,       NULL, 32 },
  { "MODULE", 				    TEXT, 	   NULL, 8 },
  /*  { "MULTIPLE", "ATOM_PAIR", 0, 32 }, */
  { "NAME", 				    TEXT, 	   NULL, 8 },
  { "ODIF", 				    TEXT,          NULL, 8 },
  { "OWNER_OS", 			    TEXT, 	   NULL, 8 },
  { "PIXMAP", 				    PIXMAP,        NULL, 32 },
  { "POSTSCRIPT", 			    STRING,        NULL, 8 },
  { "PROCEDURE", 			    TEXT,          NULL, 8 },
  { "PROCESS",				    INTEGER,       NULL, 32 },
  { "STRING", 				    STRING,        NULL, 8 },
  { "TARGETS", 				    ATOM, 	   NULL, 32 },
  { "TASK", 				    INTEGER,       NULL, 32 },
  { "TEXT", 				    TEXT,          NULL, 8  },
  { "TIMESTAMP", 			    INTEGER,       NULL, 32 },
  { "USER", 				    TEXT, 	   NULL, 8 },
};

static int num_targets = sizeof(targets)/sizeof(Target);

static int have_selection = FALSE;

GtkWidget *selection_widget;
GtkWidget *selection_text;
GtkWidget *selection_button;
GString *selection_string = NULL;

static void
init_atoms (void)
{
  int i;

  seltypes[SEL_TYPE_NONE] = GDK_NONE;
  seltypes[APPLE_PICT] = gdk_atom_intern ("APPLE_PICT",FALSE);
  seltypes[ATOM]       = gdk_atom_intern ("ATOM",FALSE);
  seltypes[ATOM_PAIR]  = gdk_atom_intern ("ATOM_PAIR",FALSE);
  seltypes[BITMAP]     = gdk_atom_intern ("BITMAP",FALSE);
  seltypes[C_STRING]   = gdk_atom_intern ("C_STRING",FALSE);
  seltypes[COLORMAP]   = gdk_atom_intern ("COLORMAP",FALSE);
  seltypes[COMPOUND_TEXT] = gdk_atom_intern ("COMPOUND_TEXT",FALSE);
  seltypes[DRAWABLE]   = gdk_atom_intern ("DRAWABLE",FALSE);
  seltypes[INTEGER]    = gdk_atom_intern ("INTEGER",FALSE);
  seltypes[PIXEL]      = gdk_atom_intern ("PIXEL",FALSE);
  seltypes[PIXMAP]     = gdk_atom_intern ("PIXMAP",FALSE);
  seltypes[SPAN]       = gdk_atom_intern ("SPAN",FALSE);
  seltypes[STRING]     = gdk_atom_intern ("STRING",FALSE);
  seltypes[TEXT]       = gdk_atom_intern ("TEXT",FALSE);
  seltypes[WINDOW]     = gdk_atom_intern ("WINDOW",FALSE);

  for (i=0; i<num_targets; i++)
    targets[i].target = gdk_atom_intern (targets[i].target_name, FALSE);
}

void
selection_toggled (GtkWidget *widget)
{
  if (GTK_TOGGLE_BUTTON(widget)->active)
    {
      have_selection = gtk_selection_owner_set (selection_widget,
						GDK_SELECTION_PRIMARY,
						GDK_CURRENT_TIME);
      if (!have_selection)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), FALSE);
    }
  else
    {
      if (have_selection)
	{
	  if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == widget->window)
	    gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				     GDK_CURRENT_TIME);
	  have_selection = FALSE;
	}
    }
}

void
selection_get (GtkWidget *widget, 
	       GtkSelectionData *selection_data,
	       guint      info,
	       guint      time,
	       gpointer   data)
{
  guchar *buffer;
  gint len;
  GdkAtom type = GDK_NONE;

  if (!selection_string)
    {
      buffer = NULL;
      len = 0;
    }      
  else
    {
      buffer = (guchar *)selection_string->str;
      len = selection_string->len;
    }

  switch (info)
    {
    case COMPOUND_TEXT:
    case TEXT:
      type = seltypes[COMPOUND_TEXT];
    case STRING:
      type = seltypes[STRING];
    }
  
  gtk_selection_data_set (selection_data, type, 8, buffer, len);
}

gint
selection_clear (GtkWidget *widget, GdkEventSelection *event)
{
  have_selection = FALSE;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(selection_button), FALSE);

  return TRUE;
}

gchar *
stringify_atom (guchar *data, gint *position)
{
  gchar *str = gdk_atom_name (*(GdkAtom *)(data+*position));
  *position += sizeof(GdkAtom);
    
  return str;
}

gchar *
stringify_text (guchar *data, gint *position)
{
  gchar *str = g_strdup ((gchar *)(data+*position));
  *position += strlen (str) + 1;
    
  return str;
}

gchar *
stringify_xid (guchar *data, gint *position)
{
  gchar buffer[20];
  gchar *str;

  sprintf(buffer,"0x%x",*(guint32 *)(data+*position));
  str = g_strdup (buffer);

  *position += sizeof(guint32);
    
  return str;
}

gchar *
stringify_integer (guchar *data, gint *position)
{
  gchar buffer[20];
  gchar *str;

  sprintf(buffer,"%d",*(int *)(data+*position));
  str = g_strdup (buffer);

  *position += sizeof(int);
    
  return str;
}

gchar *
stringify_span (guchar *data, gint *position)
{
  gchar buffer[42];
  gchar *str;

  sprintf(buffer,"%d - %d",((int *)(data+*position))[0],
	  ((int *)(data+*position))[1]);
  str = g_strdup (buffer);

  *position += 2*sizeof(int);
    
  return str;
}

void
selection_received (GtkWidget *widget, GtkSelectionData *data)
{
  int position;
  int i;
  SelType seltype;
  char *str;
  
  if (data->length < 0)
    {
      g_print("Error retrieving selection\n");
      return;
    }

  seltype = SEL_TYPE_NONE;
  for (i=0; i<LAST_SEL_TYPE; i++)
    {
      if (seltypes[i] == data->type)
	{
	  seltype = i;
	  break;
	}
    }

  if (seltype == SEL_TYPE_NONE)
    {
      char *name = gdk_atom_name (data->type);
      g_print("Don't know how to handle type: %s\n",
	      name?name:"<unknown>");
      return;
    }

  if (selection_string != NULL)
    g_string_free (selection_string, TRUE);

  selection_string = g_string_new (NULL);

  gtk_text_freeze (GTK_TEXT (selection_text));
  gtk_text_set_point (GTK_TEXT (selection_text), 0);
  gtk_text_forward_delete (GTK_TEXT (selection_text), 
			   gtk_text_get_length (GTK_TEXT (selection_text)));

  position = 0;
  while (position < data->length)
    {
      switch (seltype)
	{
	case ATOM:
	  str = stringify_atom (data->data, &position);
	  break;
	case COMPOUND_TEXT:
	case STRING:
	case TEXT:
	  str = stringify_text (data->data, &position);
	  break;
	case BITMAP:
	case DRAWABLE:
	case PIXMAP:
	case WINDOW:
	case COLORMAP:
	  str = stringify_xid (data->data, &position);
	  break;
	case INTEGER:
	case PIXEL:
	  str = stringify_integer (data->data, &position);
	  break;
	case SPAN:
	  str = stringify_span (data->data, &position);
	  break;
	default:
	  {
	    char *name = gdk_atom_name (data->type);
	    g_print("Can't convert type %s to string\n",
		    name?name:"<unknown>");
	    position = data->length;
	    continue;
	  }
	}
      gtk_text_insert (GTK_TEXT (selection_text), NULL, 
		       &selection_text->style->black, 
		       NULL, str, -1);
      gtk_text_insert (GTK_TEXT (selection_text), NULL, 
		       &selection_text->style->black, 
		       NULL, "\n", -1);
      g_string_append (selection_string, str);
      g_free (str);
    }
  gtk_text_thaw (GTK_TEXT (selection_text));
}

void
paste (GtkWidget *widget, GtkWidget *entry)
{
  const char *name;
  GdkAtom atom;

  name = gtk_entry_get_text (GTK_ENTRY(entry));
  atom = gdk_atom_intern (name, FALSE);

  if (atom == GDK_NONE)
    {
      g_print("Could not create atom: \"%s\"\n",name);
      return;
    }

  gtk_selection_convert (selection_widget, GDK_SELECTION_PRIMARY, atom, 
			 GDK_CURRENT_TIME);
}

void
quit (void)
{
  gtk_exit (0);
}

int
main (int argc, char *argv[])
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *hbox;

  static GtkTargetEntry targetlist[] = {
    { "STRING",        0, STRING },
    { "TEXT",          0, TEXT },
    { "COMPOUND_TEXT", 0, COMPOUND_TEXT }
  };
  static gint ntargets = sizeof(targetlist) / sizeof(targetlist[0]);
  
  gtk_init (&argc, &argv);

  init_atoms();

  selection_widget = gtk_invisible_new ();

  dialog = gtk_dialog_new ();
  gtk_widget_set_name (dialog, "Test Input");
  gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);

  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (quit), NULL);

  table = gtk_table_new (4, 2, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER(table), 10);

  gtk_table_set_row_spacing (GTK_TABLE (table), 0, 5);
  gtk_table_set_row_spacing (GTK_TABLE (table), 1, 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 2, 2);
  gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), 
		      table, TRUE, TRUE, 0);
  gtk_widget_show (table);
  
  selection_button = gtk_toggle_button_new_with_label ("Claim Selection");
  gtk_table_attach (GTK_TABLE (table), selection_button, 0, 2, 0, 1,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (selection_button);

  g_signal_connect (selection_button, "toggled",
		    G_CALLBACK (selection_toggled), NULL);
  g_signal_connect (selection_widget, "selection_clear_event",
		    G_CALLBACK (selection_clear), NULL);
  g_signal_connect (selection_widget, "selection_received",
		    G_CALLBACK (selection_received), NULL);

  gtk_selection_add_targets (selection_widget, GDK_SELECTION_PRIMARY,
			     targetlist, ntargets);

  g_signal_connect (selection_widget, "selection_get",
		    G_CALLBACK (selection_get), NULL);

  selection_text = gtk_text_new (NULL, NULL);
  gtk_table_attach_defaults (GTK_TABLE (table), selection_text, 0, 1, 1, 2);
  gtk_widget_show (selection_text);
  
  hscrollbar = gtk_hscrollbar_new (GTK_TEXT (selection_text)->hadj);
  gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 2, 3,
		    GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (hscrollbar);
  
  vscrollbar = gtk_vscrollbar_new (GTK_TEXT (selection_text)->vadj);
  gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 1, 2,
		    GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (vscrollbar);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 2, 3, 4,
		    GTK_EXPAND | GTK_FILL, 0, 0, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new ("Target:");
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(hbox), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  /* .. And create some buttons */
  button = gtk_button_new_with_label ("Paste");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), 
		      button, TRUE, TRUE, 0);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (paste), entry);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("Quit");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->action_area), 
		      button, TRUE, TRUE, 0);

  g_signal_connect_swapped (button, "clicked",
			    G_CALLBACK (gtk_widget_destroy), dialog);
  gtk_widget_show (button);

  gtk_widget_show (dialog);

  gtk_main ();

  return 0;
}
