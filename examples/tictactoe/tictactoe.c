
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
#include "gtk/gtksignal.h"
#include "gtk/gtktable.h"
#include "gtk/gtktogglebutton.h"
#include "tictactoe.h"

enum {
  TICTACTOE_SIGNAL,
  LAST_SIGNAL
};

static void tictactoe_class_init          (TictactoeClass *klass);
static void tictactoe_init                (Tictactoe      *ttt);
static void tictactoe_toggle              (GtkWidget *widget, Tictactoe *ttt);

static gint tictactoe_signals[LAST_SIGNAL] = { 0 };

GType
tictactoe_get_type ()
{
  static GType ttt_type = 0;

  if (!ttt_type)
    {
      static const GTypeInfo ttt_info =
      {
	sizeof (TictactoeClass),
	NULL,
        NULL,
	(GClassInitFunc) tictactoe_class_init,
        NULL,
	NULL,
        sizeof (Tictactoe),
	0,
	(GInstanceInitFunc) tictactoe_init,
      };

      ttt_type = g_type_register_static (GTK_TYPE_VBOX, "Tictactoe", &ttt_info, 0);
    }

  return ttt_type;
}

static void
tictactoe_class_init (TictactoeClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  
  tictactoe_signals[TICTACTOE_SIGNAL] = g_signal_new ("tictactoe",
					 G_TYPE_FROM_CLASS (object_class),
	                                 G_SIGNAL_RUN_FIRST,
	                                 0,
                                         NULL, 
                                         NULL,                
					 g_cclosure_marshal_VOID__VOID,
                                         G_TYPE_NONE, 0, NULL);


  class->tictactoe = NULL;
}

static void
tictactoe_init (Tictactoe *ttt)
{
  GtkWidget *table;
  gint i,j;
  
  table = gtk_table_new (3, 3, TRUE);
  gtk_container_add (GTK_CONTAINER (ttt), table);
  gtk_widget_show (table);

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      {
	ttt->buttons[i][j] = gtk_toggle_button_new ();
	gtk_table_attach_defaults (GTK_TABLE (table), ttt->buttons[i][j], 
				   i, i+1, j, j+1);
	g_signal_connect (G_OBJECT (ttt->buttons[i][j]), "toggled",
			  G_CALLBACK (tictactoe_toggle), ttt);
	gtk_widget_set_size_request (ttt->buttons[i][j], 20, 20);
	gtk_widget_show (ttt->buttons[i][j]);
      }
}

GtkWidget*
tictactoe_new ()
{
  return GTK_WIDGET (g_object_new (tictactoe_get_type (), NULL));
}

void	       
tictactoe_clear (Tictactoe *ttt)
{
  int i,j;

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      {
	g_signal_handlers_block_by_func (G_OBJECT (ttt->buttons[i][j]), 
                                         NULL, ttt);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ttt->buttons[i][j]),
				      FALSE);
	g_signal_handlers_unblock_by_func (G_OBJECT (ttt->buttons[i][j]), 
                                           NULL, ttt);
      }
}

static void
tictactoe_toggle (GtkWidget *widget, Tictactoe *ttt)
{
  int i,k;

  static int rwins[8][3] = { { 0, 0, 0 }, { 1, 1, 1 }, { 2, 2, 2 },
			     { 0, 1, 2 }, { 0, 1, 2 }, { 0, 1, 2 },
			     { 0, 1, 2 }, { 0, 1, 2 } };
  static int cwins[8][3] = { { 0, 1, 2 }, { 0, 1, 2 }, { 0, 1, 2 },
			     { 0, 0, 0 }, { 1, 1, 1 }, { 2, 2, 2 },
			     { 0, 1, 2 }, { 2, 1, 0 } };

  int success, found;

  for (k = 0; k < 8; k++)
    {
      success = TRUE;
      found = FALSE;

      for (i = 0; i < 3; i++)
	{
	  success = success && 
	    GTK_TOGGLE_BUTTON (ttt->buttons[rwins[k][i]][cwins[k][i]])->active;
	  found = found ||
	    ttt->buttons[rwins[k][i]][cwins[k][i]] == widget;
	}
      
      if (success && found)
	{
	  g_signal_emit (G_OBJECT (ttt), 
	                 tictactoe_signals[TICTACTOE_SIGNAL], 0);
	  break;
	}
    }
}

