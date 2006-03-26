/* GtkPrinterOptionWidget
 * Copyright (C) 2006 Alexander Larsson  <alexl@redhat.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gtkintl.h"
#include "gtkalignment.h"
#include "gtkcheckbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcombobox.h"
#include "gtkfilechooserbutton.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtktogglebutton.h"
#include "gtkprivate.h"

#include "gtkprinteroptionwidget.h"
#include "gtkalias.h"

#define GTK_PRINTER_OPTION_WIDGET_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_PRINTER_OPTION_WIDGET, GtkPrinterOptionWidgetPrivate))

static void gtk_printer_option_widget_finalize (GObject *object);

static void deconstruct_widgets (GtkPrinterOptionWidget *widget);
static void construct_widgets (GtkPrinterOptionWidget *widget);
static void update_widgets (GtkPrinterOptionWidget *widget);

struct GtkPrinterOptionWidgetPrivate
{
  GtkPrinterOption *source;
  gulong source_changed_handler;
  
  GtkWidget *check;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *filechooser;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SOURCE,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkPrinterOptionWidget, gtk_printer_option_widget, GTK_TYPE_HBOX);

static void gtk_printer_option_widget_set_property (GObject      *object,
						    guint         prop_id,
						    const GValue *value,
						    GParamSpec   *pspec);
static void gtk_printer_option_widget_get_property (GObject      *object,
						    guint         prop_id,
						    GValue       *value,
						    GParamSpec   *pspec);

static void
gtk_printer_option_widget_class_init (GtkPrinterOptionWidgetClass *class)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GObjectClass *) class;
  widget_class = (GtkWidgetClass *) class;

  object_class->finalize = gtk_printer_option_widget_finalize;
  object_class->set_property = gtk_printer_option_widget_set_property;
  object_class->get_property = gtk_printer_option_widget_get_property;

  g_type_class_add_private (class, sizeof (GtkPrinterOptionWidgetPrivate));  

  signals[CHANGED] =
    g_signal_new ("changed",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrinterOptionWidgetClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_SOURCE,
                                   g_param_spec_object ("source",
							P_("Source option"),
							P_("The PrinterOption backing this widget"),
							GTK_TYPE_PRINTER_OPTION,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
gtk_printer_option_widget_init (GtkPrinterOptionWidget *widget)
{
  widget->priv = GTK_PRINTER_OPTION_WIDGET_GET_PRIVATE (widget); 
}

static void
gtk_printer_option_widget_finalize (GObject *object)
{
  GtkPrinterOptionWidget *widget;
  
  widget = GTK_PRINTER_OPTION_WIDGET (object);

  if (widget->priv->source)
    {
      g_object_unref (widget->priv->source);
      widget->priv->source = NULL;
    }
  
  if (G_OBJECT_CLASS (gtk_printer_option_widget_parent_class)->finalize)
    G_OBJECT_CLASS (gtk_printer_option_widget_parent_class)->finalize (object);
}

static void
gtk_printer_option_widget_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
  GtkPrinterOptionWidget *widget;
  
  widget = GTK_PRINTER_OPTION_WIDGET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      gtk_printer_option_widget_set_source (widget, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_printer_option_widget_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
  GtkPrinterOptionWidget *widget;
  
  widget = GTK_PRINTER_OPTION_WIDGET (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, widget->priv->source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
emit_changed (GtkPrinterOptionWidget *widget)
{
  g_signal_emit (widget, signals[CHANGED], 0);
}

GtkWidget *
gtk_printer_option_widget_new (GtkPrinterOption *source)
{
  return g_object_new (GTK_TYPE_PRINTER_OPTION_WIDGET, "source", source, NULL);
}

static void
source_changed_cb (GtkPrinterOption *source,
		   GtkPrinterOptionWidget  *widget)
{
  update_widgets (widget);
  emit_changed (widget);
}

void
gtk_printer_option_widget_set_source (GtkPrinterOptionWidget  *widget,
				      GtkPrinterOption *source)
{
  if (source)
    g_object_ref (source);
  
  if (widget->priv->source)
    {
      g_signal_handler_disconnect (widget->priv->source,
				   widget->priv->source_changed_handler);
      g_object_unref (widget->priv->source);
    }

  widget->priv->source = source;

  if (source)
    widget->priv->source_changed_handler =
      g_signal_connect (source, "changed", G_CALLBACK (source_changed_cb), widget);

  construct_widgets (widget);
  update_widgets (widget);

  g_object_notify (G_OBJECT (widget), "source");
}

static GtkWidget *
combo_box_new (void)
{
  GtkWidget *combo_box;
  GtkCellRenderer *cell;
  GtkListStore *store;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
                                  "text", 0,
                                  NULL);

  return combo_box;
}
  
static void
combo_box_append (GtkWidget *combo,
		  const char *display_text,
		  const char *value)
{
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreeIter iter;
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  store = GTK_LIST_STORE (model);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      0, display_text,
		      1, value,
		      -1);
}

struct ComboSet {
  GtkComboBox *combo;
  const char *value;
};

static gboolean
set_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  struct ComboSet *set_data = data;
  gboolean found;
  char *value;
  
  gtk_tree_model_get (model, iter, 1, &value, -1);
  found = (strcmp (value, set_data->value) == 0);
  g_free (value);
  
  if (found)
    gtk_combo_box_set_active_iter (set_data->combo, iter);

  return found;
}

static void
combo_box_set (GtkWidget *combo,
	       const char *value)
{
  GtkTreeModel *model;
  GtkListStore *store;
  struct ComboSet set_data;
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  store = GTK_LIST_STORE (model);

  set_data.combo = GTK_COMBO_BOX (combo);
  set_data.value = value;
  gtk_tree_model_foreach (model, set_cb, &set_data);
}

static char *
combo_box_get (GtkWidget *combo)
{
  GtkTreeModel *model;
  char *val;
  GtkTreeIter iter;
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  val = NULL;
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    gtk_tree_model_get (model, &iter,
			1, &val,
			-1);
  return val;
}


static void
deconstruct_widgets (GtkPrinterOptionWidget *widget)
{
  if (widget->priv->check)
    {
      gtk_widget_destroy (widget->priv->check);
      widget->priv->check = NULL;
    }
  
  if (widget->priv->combo)
    {
      gtk_widget_destroy (widget->priv->combo);
      widget->priv->combo = NULL;
    }
  
  if (widget->priv->entry)
    {
      gtk_widget_destroy (widget->priv->entry);
      widget->priv->entry = NULL;
    }

  /* make sure entry and combo are destroyed first */
  /* as we use the two of them to create the filechooser */
  if (widget->priv->filechooser)
    {
      gtk_widget_destroy (widget->priv->filechooser);
      widget->priv->filechooser = NULL;
    }

  if (widget->priv->image)
    {
      gtk_widget_destroy (widget->priv->image);
      widget->priv->image = NULL;
    }

  if (widget->priv->label)
    {
      gtk_widget_destroy (widget->priv->label);
      widget->priv->label = NULL;
    }
}

static void
check_toggled_cb (GtkToggleButton *toggle_button,
		  GtkPrinterOptionWidget *widget)
{
  g_signal_handler_block (widget->priv->source, widget->priv->source_changed_handler);
  gtk_printer_option_set_boolean (widget->priv->source,
				  gtk_toggle_button_get_active (toggle_button));
  g_signal_handler_unblock (widget->priv->source, widget->priv->source_changed_handler);
  emit_changed (widget);
}

static void
filesave_changed_cb (GtkWidget *w,
                     GtkPrinterOptionWidget *widget)
{
  char *value;
  char *directory;
  const char *file;

  /* combine the value of the chooser with the value of the entry */
  g_signal_handler_block (widget->priv->source, widget->priv->source_changed_handler);  
  
  /* TODO: how do we support nonlocal file systems? */
  directory = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (widget->priv->combo));
  file =  gtk_entry_get_text (GTK_ENTRY (widget->priv->entry));

  value = g_build_filename (directory, file, NULL);

  if (value)
    gtk_printer_option_set (widget->priv->source, value);

  g_free (directory);
  g_free (value);

  g_signal_handler_unblock (widget->priv->source, widget->priv->source_changed_handler);
  emit_changed (widget);
}

static void
combo_changed_cb (GtkWidget *combo,
		  GtkPrinterOptionWidget *widget)
{
  char *value;
  
  g_signal_handler_block (widget->priv->source, widget->priv->source_changed_handler);
  value = combo_box_get (combo);
  if (value)
    gtk_printer_option_set (widget->priv->source, value);
  g_free (value);
  g_signal_handler_unblock (widget->priv->source, widget->priv->source_changed_handler);
  emit_changed (widget);
}

static void
entry_changed_cb (GtkWidget *entry,
		  GtkPrinterOptionWidget *widget)
{
  const char *value;
  
  g_signal_handler_block (widget->priv->source, widget->priv->source_changed_handler);
  value = gtk_entry_get_text (GTK_ENTRY (entry));
  if (value)
    gtk_printer_option_set (widget->priv->source, value);
  g_signal_handler_unblock (widget->priv->source, widget->priv->source_changed_handler);
  emit_changed (widget);
}


static void
construct_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOption *source;
  char *text;
  int i;

  source = widget->priv->source;
  
  deconstruct_widgets (widget);
  
  if (source == NULL)
    {
      widget->priv->combo = combo_box_new ();
      combo_box_append (widget->priv->combo,_("Not available"), "None");
      gtk_combo_box_set_active (GTK_COMBO_BOX (widget->priv->combo), 0);
      gtk_widget_set_sensitive (widget->priv->combo, FALSE);
      gtk_widget_show (widget->priv->combo);
      gtk_box_pack_start (GTK_BOX (widget), widget->priv->combo, TRUE, TRUE, 0);
    }
  else switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      widget->priv->check = gtk_check_button_new_with_mnemonic (source->display_text);
      g_signal_connect (widget->priv->check, "toggled", G_CALLBACK (check_toggled_cb), widget);
      gtk_widget_show (widget->priv->check);
      gtk_box_pack_start (GTK_BOX (widget), widget->priv->check, TRUE, TRUE, 0);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
      widget->priv->combo = combo_box_new ();
      for (i = 0; i < source->num_choices; i++)
	  combo_box_append (widget->priv->combo,
			    source->choices_display[i],
			    source->choices[i]);
      gtk_widget_show (widget->priv->combo);
      gtk_box_pack_start (GTK_BOX (widget), widget->priv->combo, TRUE, TRUE, 0);
      g_signal_connect (widget->priv->combo, "changed", G_CALLBACK (combo_changed_cb), widget);

      text = g_strdup_printf ("%s: ", source->display_text);
      widget->priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (widget->priv->label);
      break;
    case GTK_PRINTER_OPTION_TYPE_STRING:
      widget->priv->entry = gtk_entry_new ();
      gtk_widget_show (widget->priv->entry);
      gtk_box_pack_start (GTK_BOX (widget), widget->priv->entry, TRUE, TRUE, 0);
      g_signal_connect (widget->priv->entry, "changed", G_CALLBACK (entry_changed_cb), widget);

      text = g_strdup_printf ("%s: ", source->display_text);
      widget->priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (widget->priv->label);

      break;

    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      {
        GtkWidget *label;
        GtkWidget *align;
        
        widget->priv->filechooser = gtk_table_new (2, 2, FALSE);

        /* TODO: make this a gtkfilechooserentry once we move to GTK */
        widget->priv->entry = gtk_entry_new ();
        widget->priv->combo = gtk_file_chooser_button_new ("Print to PDF", 
                                                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        
        align = gtk_alignment_new (0, 0.5, 0, 0);
        label = gtk_label_new ("Name:");
        gtk_container_add (GTK_CONTAINER (align), label);

        gtk_table_attach (GTK_TABLE (widget->priv->filechooser), align,
                          0, 1, 0, 1, GTK_FILL, 0,
                          0, 0);

        gtk_table_attach (GTK_TABLE (widget->priv->filechooser), widget->priv->entry,
                          1, 2, 0, 1, GTK_FILL, 0,
                          0, 0);

        align = gtk_alignment_new (0, 0.5, 0, 0);
        label = gtk_label_new ("Save in folder:");
        gtk_container_add (GTK_CONTAINER (align), label);

        gtk_table_attach (GTK_TABLE (widget->priv->filechooser), align,
                          0, 1, 1, 2, GTK_FILL, 0,
                          0, 0);

        gtk_table_attach (GTK_TABLE (widget->priv->filechooser), widget->priv->combo,
                          1, 2, 1, 2, GTK_FILL, 0,
                          0, 0);

        gtk_widget_show_all (widget->priv->filechooser);
        gtk_box_pack_start (GTK_BOX (widget), widget->priv->filechooser, TRUE, TRUE, 0);

        g_signal_connect (widget->priv->entry, "changed", G_CALLBACK (filesave_changed_cb), widget);

        g_signal_connect (widget->priv->combo, "current-folder-changed", G_CALLBACK (filesave_changed_cb), widget);
      }
      break;
    default:
      break;
    }

  widget->priv->image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (widget), widget->priv->image, FALSE, FALSE, 0);
}

static void
update_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOption *source;

  source = widget->priv->source;
  
  if (source == NULL)
    {
      gtk_widget_hide (widget->priv->image);
      return;
    }

  switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      if (strcmp (source->value, "True") == 0)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget->priv->check), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget->priv->check), FALSE);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
      combo_box_set (widget->priv->combo, source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_STRING:
      gtk_entry_set_text (GTK_ENTRY (widget->priv->entry), source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      {
	char *basename = g_path_get_basename (source->value);
	char *dirname = g_path_get_dirname (source->value);
	gtk_entry_set_text (GTK_ENTRY (widget->priv->entry), basename);
	if (g_path_is_absolute (dirname))
	  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget->priv->combo),
					       dirname);
	g_free (basename);
	g_free (dirname);
	break;
      }
    default:
      break;
    }

  if (source->has_conflict)
    gtk_widget_show (widget->priv->image);
  else
    gtk_widget_hide (widget->priv->image);
}

gboolean
gtk_printer_option_widget_has_external_label (GtkPrinterOptionWidget  *widget)
{
  return widget->priv->label != NULL;
}

GtkWidget *
gtk_printer_option_widget_get_external_label (GtkPrinterOptionWidget  *widget)
{
  return widget->priv->label;
}

const char *
gtk_printer_option_widget_get_value (GtkPrinterOptionWidget  *widget)
{
  if (widget->priv->source)
    return widget->priv->source->value;
  
  return "";
}

#define __GTK_PRINTER_OPTION_WIDGET_C__
#include "gtkaliasdef.c"
