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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "gtkintl.h"
#include "gtkcheckbutton.h"
#include "gtkcelllayout.h"
#include "gtkcellrenderertext.h"
#include "gtkcombobox.h"
#include "gtkfilechooserdialog.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkliststore.h"
#include "gtkradiobutton.h"
#include "gtkgrid.h"
#include "gtktogglebutton.h"
#include "gtkorientable.h"
#include "gtkprivate.h"

#include "gtkprinteroptionwidget.h"

/* This defines the max file length that the file chooser
 * button should display. The total length will be
 * FILENAME_LENGTH_MAX+3 because the truncated name is prefixed
 * with “...”.
 */
#define FILENAME_LENGTH_MAX 27

static void gtk_printer_option_widget_finalize (GObject *object);

static void deconstruct_widgets (GtkPrinterOptionWidget *widget);
static void construct_widgets   (GtkPrinterOptionWidget *widget);
static void update_widgets      (GtkPrinterOptionWidget *widget);

static gchar *trim_long_filename (const gchar *filename);

struct GtkPrinterOptionWidgetPrivate
{
  GtkPrinterOption *source;
  gulong source_changed_handler;

  GtkWidget *check;
  GtkWidget *combo;
  GtkWidget *entry;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *info_label;
  GtkWidget *box;
  GtkWidget *button;

  /* the last location for save to file, that the user selected */
  gchar *last_location;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SOURCE
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkPrinterOptionWidget, gtk_printer_option_widget, GTK_TYPE_BOX)

static void gtk_printer_option_widget_set_property (GObject      *object,
						    guint         prop_id,
						    const GValue *value,
						    GParamSpec   *pspec);
static void gtk_printer_option_widget_get_property (GObject      *object,
						    guint         prop_id,
						    GValue       *value,
						    GParamSpec   *pspec);
static gboolean gtk_printer_option_widget_mnemonic_activate (GtkWidget *widget,
							     gboolean   group_cycling);

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

  widget_class->mnemonic_activate = gtk_printer_option_widget_mnemonic_activate;

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
  widget->priv = gtk_printer_option_widget_get_instance_private (widget);

  gtk_box_set_spacing (GTK_BOX (widget), 12);
}

static void
gtk_printer_option_widget_finalize (GObject *object)
{
  GtkPrinterOptionWidget *widget = GTK_PRINTER_OPTION_WIDGET (object);
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  
  if (priv->source)
    {
      g_signal_handler_disconnect (priv->source,
				   priv->source_changed_handler);
      g_object_unref (priv->source);
      priv->source = NULL;
    }
  
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
  GtkPrinterOptionWidget *widget = GTK_PRINTER_OPTION_WIDGET (object);
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_printer_option_widget_mnemonic_activate (GtkWidget *widget,
					     gboolean   group_cycling)
{
  GtkPrinterOptionWidget *powidget = GTK_PRINTER_OPTION_WIDGET (widget);
  GtkPrinterOptionWidgetPrivate *priv = powidget->priv;

  if (priv->check)
    return gtk_widget_mnemonic_activate (priv->check, group_cycling);
  if (priv->combo)
    return gtk_widget_mnemonic_activate (priv->combo, group_cycling);
  if (priv->entry)
    return gtk_widget_mnemonic_activate (priv->entry, group_cycling);
  if (priv->button)
    return gtk_widget_mnemonic_activate (priv->button, group_cycling);

  return FALSE;
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
gtk_printer_option_widget_set_source (GtkPrinterOptionWidget *widget,
				      GtkPrinterOption       *source)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (source)
    g_object_ref (source);
  
  if (priv->source)
    {
      g_signal_handler_disconnect (priv->source,
				   priv->source_changed_handler);
      g_object_unref (priv->source);
    }

  priv->source = source;

  if (source)
    priv->source_changed_handler =
      g_signal_connect (source, "changed", G_CALLBACK (source_changed_cb), widget);

  construct_widgets (widget);
  update_widgets (widget);

  g_object_notify (G_OBJECT (widget), "source");
}

enum {
  NAME_COLUMN,
  VALUE_COLUMN,
  N_COLUMNS
};

static void
combo_box_set_model (GtkWidget *combo_box)
{
  GtkListStore *store;

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
  g_object_unref (store);
}

static void
combo_box_set_view (GtkWidget *combo_box)
{
  GtkCellRenderer *cell;

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
                                  "text", NAME_COLUMN,
                                   NULL);
}

static GtkWidget *
combo_box_entry_new (void)
{
  GtkWidget *combo_box;
  combo_box = g_object_new (GTK_TYPE_COMBO_BOX, "has-entry", TRUE, NULL);

  combo_box_set_model (combo_box);

  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (combo_box), NAME_COLUMN);

  return combo_box;
}

static GtkWidget *
combo_box_new (void)
{
  GtkWidget *combo_box;
  combo_box = gtk_combo_box_new ();

  combo_box_set_model (combo_box);
  combo_box_set_view (combo_box);

  return combo_box;
}
  
static void
combo_box_append (GtkWidget   *combo,
		  const gchar *display_text,
		  const gchar *value)
{
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreeIter iter;
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  store = GTK_LIST_STORE (model);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
		      NAME_COLUMN, display_text,
		      VALUE_COLUMN, value,
		      -1);
}

struct ComboSet {
  GtkComboBox *combo;
  const gchar *value;
};

static gboolean
set_cb (GtkTreeModel *model, 
	GtkTreePath  *path, 
	GtkTreeIter  *iter, 
	gpointer      data)
{
  struct ComboSet *set_data = data;
  gboolean found;
  char *value;
  
  gtk_tree_model_get (model, iter, VALUE_COLUMN, &value, -1);
  found = (strcmp (value, set_data->value) == 0);
  g_free (value);
  
  if (found)
    gtk_combo_box_set_active_iter (set_data->combo, iter);

  return found;
}

static void
combo_box_set (GtkWidget   *combo,
	       const gchar *value)
{
  GtkTreeModel *model;
  struct ComboSet set_data;
  
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  set_data.combo = GTK_COMBO_BOX (combo);
  set_data.value = value;
  gtk_tree_model_foreach (model, set_cb, &set_data);
}

static gchar *
combo_box_get (GtkWidget *combo, gboolean *custom)
{
  GtkTreeModel *model;
  gchar *value;
  GtkTreeIter iter;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  value = NULL;
  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
    {
      gtk_tree_model_get (model, &iter, VALUE_COLUMN, &value, -1);
      *custom = FALSE;
    }
  else
    {
      if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (combo)))
        {
          value = g_strdup (gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo)))));
          *custom = TRUE;
        }

      if (!value || !gtk_tree_model_get_iter_first (model, &iter))
        return value;

      /* If the user entered an item from the dropdown list manually, return
       * the non-custom option instead. */
      do
        {
          gchar *val, *name;
          gtk_tree_model_get (model, &iter, VALUE_COLUMN, &val,
                                            NAME_COLUMN, &name, -1);
          if (g_str_equal (value, name))
            {
              *custom = FALSE;
              g_free (name);
              g_free (value);
              return val;
            }

          g_free (val);
          g_free (name);
        }
      while (gtk_tree_model_iter_next (model, &iter));
    }

  return value;
}


static void
deconstruct_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (priv->check)
    {
      gtk_widget_destroy (priv->check);
      priv->check = NULL;
    }
  
  if (priv->combo)
    {
      gtk_widget_destroy (priv->combo);
      priv->combo = NULL;
    }
  
  if (priv->entry)
    {
      gtk_widget_destroy (priv->entry);
      priv->entry = NULL;
    }

  if (priv->image)
    {
      gtk_widget_destroy (priv->image);
      priv->image = NULL;
    }

  if (priv->label)
    {
      gtk_widget_destroy (priv->label);
      priv->label = NULL;
    }
  if (priv->info_label)
    {
      gtk_widget_destroy (priv->info_label);
      priv->info_label = NULL;
    }
}

static void
check_toggled_cb (GtkToggleButton        *toggle_button,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  g_signal_handler_block (priv->source, priv->source_changed_handler);
  gtk_printer_option_set_boolean (priv->source,
				  gtk_toggle_button_get_active (toggle_button));
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
dialog_response_callback (GtkDialog              *dialog,
                          gint                    response_id,
                          GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  gchar *uri = NULL;
  gchar *new_location = NULL;

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      gchar *filename;
      gchar *filename_utf8;
      gchar *filename_short;

      new_location = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      filename_short = trim_long_filename (filename_utf8);
      gtk_button_set_label (GTK_BUTTON (priv->button), filename_short);
      g_free (filename_short);
      g_free (filename_utf8);
      g_free (filename);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (new_location)
    uri = new_location;
  else
    uri = priv->last_location;

  if (uri)
    {
      gtk_printer_option_set (priv->source, uri);
      emit_changed (widget);
    }

  g_free (new_location);
  g_free (priv->last_location);
  priv->last_location = NULL;

  /* unblock the handler which was blocked in the filesave_choose_cb function */
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
}

static void
filesave_choose_cb (GtkWidget              *button,
                    GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  gchar *last_location = NULL;
  GtkWidget *dialog;
  GtkWindow *toplevel;

  /* this will be unblocked in the dialog_response_callback function */
  g_signal_handler_block (priv->source, priv->source_changed_handler);

  toplevel = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
  dialog = gtk_file_chooser_dialog_new (_("Select a filename"),
                                        toplevel,
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Select"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  /* The confirmation dialog will appear, when the user clicks print */
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), FALSE);

  /* select the current filename in the dialog */
  if (priv->source != NULL)
    {
      priv->last_location = last_location = g_strdup (priv->source->value);
      if (last_location)
        {
          GFile *file;
          gchar *basename;
          gchar *basename_utf8;

          gtk_file_chooser_select_uri (GTK_FILE_CHOOSER (dialog), last_location);
          file = g_file_new_for_uri (last_location);
          basename = g_file_get_basename (file);
          basename_utf8 = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
          gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), basename_utf8);
          g_free (basename_utf8);
          g_free (basename);
          g_object_unref (file);
        }
    }

  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_response_callback), widget);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_present (GTK_WINDOW (dialog));
}

static gchar *
filter_numeric (const gchar *val,
                gboolean     allow_neg,
		gboolean     allow_dec,
                gboolean    *changed_out)
{
  gchar *filtered_val;
  int i, j;
  int len = strlen (val);
  gboolean dec_set = FALSE;

  filtered_val = g_malloc (len + 1);

  for (i = 0, j = 0; i < len; i++)
    {
      if (isdigit (val[i]))
        {
          filtered_val[j] = val[i];
	  j++;
	}
      else if (allow_dec && !dec_set && 
               (val[i] == '.' || val[i] == ','))
        {
	  /* allow one period or comma
	   * we should be checking locals
	   * but this is good enough for now
	   */
          filtered_val[j] = val[i];
	  dec_set = TRUE;
	  j++;
	}
      else if (allow_neg && i == 0 && val[0] == '-')
        {
          filtered_val[0] = val[0];
	  j++;
	}
    }

  filtered_val[j] = '\0';
  *changed_out = !(i == j);

  return filtered_val;
}

static void
combo_changed_cb (GtkWidget              *combo,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  gchar *value;
  gchar *filtered_val = NULL;
  gboolean changed;
  gboolean custom = TRUE;

  g_signal_handler_block (priv->source, priv->source_changed_handler);
  
  value = combo_box_get (combo, &custom);

  /* Handle constraints if the user entered a custom value. */
  if (custom)
    {
      switch (priv->source->type)
        {
        case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
          filtered_val = filter_numeric (value, FALSE, FALSE, &changed);
          break;
        case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
          filtered_val = filter_numeric (value, TRUE, FALSE, &changed);
          break;
        case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
          filtered_val = filter_numeric (value, TRUE, TRUE, &changed);
          break;
        default:
          break;
        }
    }

  if (filtered_val)
    {
      g_free (value);

      if (changed)
        {
          GtkEntry *entry;
	  
	  entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo)));

          gtk_entry_set_text (entry, filtered_val);
	}
      value = filtered_val;
    }

  if (value)
    gtk_printer_option_set (priv->source, value);
  g_free (value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
entry_changed_cb (GtkWidget              *entry,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  const gchar *value;
  
  g_signal_handler_block (priv->source, priv->source_changed_handler);
  value = gtk_entry_get_text (GTK_ENTRY (entry));
  if (value)
    gtk_printer_option_set (priv->source, value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}


static void
radio_changed_cb (GtkWidget              *button,
		  GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  gchar *value;
  
  g_signal_handler_block (priv->source, priv->source_changed_handler);
  value = g_object_get_data (G_OBJECT (button), "value");
  if (value)
    gtk_printer_option_set (priv->source, value);
  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
}

static void
select_maybe (GtkWidget   *widget, 
	      const gchar *value)
{
  gchar *v = g_object_get_data (G_OBJECT (widget), "value");
      
  if (strcmp (value, v) == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
alternative_set (GtkWidget   *box,
		 const gchar *value)
{
  gtk_container_foreach (GTK_CONTAINER (box), 
			 (GtkCallback) select_maybe,
			 (gpointer) value);
}

static GSList *
alternative_append (GtkWidget              *box,
		    const gchar            *label,
                    const gchar            *value,
		    GtkPrinterOptionWidget *widget,
		    GSList                 *group)
{
  GtkWidget *button;

  button = gtk_radio_button_new_with_label (group, label);
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

  g_object_set_data (G_OBJECT (button), "value", (gpointer)value);
  g_signal_connect (button, "toggled", 
		    G_CALLBACK (radio_changed_cb), widget);

  return gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
}

static void
construct_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GtkPrinterOption *source;
  char *text;
  int i;
  GSList *group;

  source = priv->source;
  
  deconstruct_widgets (widget);
  
  gtk_widget_set_sensitive (GTK_WIDGET (widget), TRUE);

  if (source == NULL)
    {
      priv->combo = combo_box_new ();
      combo_box_append (priv->combo,_("Not available"), "None");
      gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo), 0);
      gtk_widget_set_sensitive (GTK_WIDGET (widget), FALSE);
      gtk_widget_show (priv->combo);
      gtk_box_pack_start (GTK_BOX (widget), priv->combo, TRUE, TRUE, 0);
    }
  else switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      priv->check = gtk_check_button_new_with_mnemonic (source->display_text);
      g_signal_connect (priv->check, "toggled", G_CALLBACK (check_toggled_cb), widget);
      gtk_widget_show (priv->check);
      gtk_box_pack_start (GTK_BOX (widget), priv->check, TRUE, TRUE, 0);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_STRING:
      if (source->type == GTK_PRINTER_OPTION_TYPE_PICKONE)
        {
          priv->combo = combo_box_new ();
	}
      else
        {
          priv->combo = combo_box_entry_new ();

          if (source->type == GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD ||
	      source->type == GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE)
	    {
              GtkEntry *entry;

	      entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->combo)));

              gtk_entry_set_visibility (entry, FALSE); 
	    }
        }

      for (i = 0; i < source->num_choices; i++)
        combo_box_append (priv->combo,
                          source->choices_display[i],
                          source->choices[i]);
      gtk_widget_show (priv->combo);
      gtk_box_pack_start (GTK_BOX (widget), priv->combo, TRUE, TRUE, 0);
      g_signal_connect (priv->combo, "changed", G_CALLBACK (combo_changed_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (priv->label);
      break;

    case GTK_PRINTER_OPTION_TYPE_ALTERNATIVE:
      group = NULL;
      priv->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_widget_show (priv->box);
      gtk_box_pack_start (GTK_BOX (widget), priv->box, TRUE, TRUE, 0);
      for (i = 0; i < source->num_choices; i++)
        {
	  group = alternative_append (priv->box,
                                      source->choices_display[i],
                                      source->choices[i],
                                      widget,
                                      group);
          /* for mnemonic activation */
          if (i == 0)
            priv->button = group->data;
        }

      if (source->display_text)
	{
	  text = g_strdup_printf ("%s:", source->display_text);
	  priv->label = gtk_label_new_with_mnemonic (text);
	  g_free (text);
	  gtk_widget_show (priv->label);
	}
      break;

    case GTK_PRINTER_OPTION_TYPE_STRING:
      priv->entry = gtk_entry_new ();
      gtk_entry_set_activates_default (GTK_ENTRY (priv->entry),
                                       gtk_printer_option_get_activates_default (source));
      gtk_widget_show (priv->entry);
      gtk_box_pack_start (GTK_BOX (widget), priv->entry, TRUE, TRUE, 0);
      g_signal_connect (priv->entry, "changed", G_CALLBACK (entry_changed_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (priv->label);

      break;

    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      priv->button = gtk_button_new ();
      gtk_widget_show (priv->button);
      gtk_box_pack_start (GTK_BOX (widget), priv->button, TRUE, TRUE, 0);
      g_signal_connect (priv->button, "clicked", G_CALLBACK (filesave_choose_cb), widget);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (priv->label);

      break;

    case GTK_PRINTER_OPTION_TYPE_INFO:
      priv->info_label = gtk_label_new (NULL);
      gtk_label_set_selectable (GTK_LABEL (priv->info_label), TRUE);
      gtk_widget_show (priv->info_label);
      gtk_box_pack_start (GTK_BOX (widget), priv->info_label, FALSE, TRUE, 0);

      text = g_strdup_printf ("%s:", source->display_text);
      priv->label = gtk_label_new_with_mnemonic (text);
      g_free (text);
      gtk_widget_show (priv->label);

      break;

    default:
      break;
    }

  priv->image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (widget), priv->image, FALSE, FALSE, 0);
}

/*
 * If the filename exceeds FILENAME_LENGTH_MAX, then trim it and replace
 * the first three letters with three dots.
 */
static gchar *
trim_long_filename (const gchar *filename)
{
  const gchar *home;
  gint len, offset;
  gchar *result;

  home = g_get_home_dir ();
  if (g_str_has_prefix (filename, home))
    {
      gchar *homeless_filename;

      offset = g_utf8_strlen (home, -1);
      len = g_utf8_strlen (filename, -1);
      homeless_filename = g_utf8_substring (filename, offset, len);
      result = g_strconcat ("~", homeless_filename, NULL);
      g_free (homeless_filename);
    }
  else
    result = g_strdup (filename);

  len = g_utf8_strlen (result, -1);
  if (len > FILENAME_LENGTH_MAX)
    {
      gchar *suffix;

      suffix = g_utf8_substring (result, len - FILENAME_LENGTH_MAX, len);
      g_free (result);
      result = g_strconcat ("...", suffix, NULL);
      g_free (suffix);
    }

  return result;
}

static void
update_widgets (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  GtkPrinterOption *source;

  source = priv->source;
  
  if (source == NULL)
    {
      gtk_widget_hide (priv->image);
      return;
    }

  switch (source->type)
    {
    case GTK_PRINTER_OPTION_TYPE_BOOLEAN:
      if (g_ascii_strcasecmp (source->value, "True") == 0)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check), TRUE);
      else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check), FALSE);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE:
      combo_box_set (priv->combo, source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_ALTERNATIVE:
      alternative_set (priv->box, source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_STRING:
      gtk_entry_set_text (GTK_ENTRY (priv->entry), source->value);
      break;
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_REAL:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_INT:
    case GTK_PRINTER_OPTION_TYPE_PICKONE_STRING:
      {
        GtkEntry *entry;

        entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->combo)));
        if (gtk_printer_option_has_choice (source, source->value))
          combo_box_set (priv->combo, source->value);
        else
          gtk_entry_set_text (entry, source->value);

        break;
      }
    case GTK_PRINTER_OPTION_TYPE_FILESAVE:
      {
        gchar *text;
        gchar *filename;

        filename = g_filename_from_uri (source->value, NULL, NULL);
        if (filename != NULL)
          {
            text = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
            if (text != NULL)
              {
                gchar *short_filename;

                short_filename = trim_long_filename (text);
                gtk_button_set_label (GTK_BUTTON (priv->button), short_filename);
                g_free (short_filename);
              }

            g_free (text);
            g_free (filename);
          }
        else
          gtk_button_set_label (GTK_BUTTON (priv->button), source->value);
        break;
      }
    case GTK_PRINTER_OPTION_TYPE_INFO:
      gtk_label_set_text (GTK_LABEL (priv->info_label), source->value);
      break;
    default:
      break;
    }

  if (source->has_conflict)
    gtk_widget_show (priv->image);
  else
    gtk_widget_hide (priv->image);
}

gboolean
gtk_printer_option_widget_has_external_label (GtkPrinterOptionWidget *widget)
{
  return widget->priv->label != NULL;
}

GtkWidget *
gtk_printer_option_widget_get_external_label (GtkPrinterOptionWidget  *widget)
{
  return widget->priv->label;
}

const gchar *
gtk_printer_option_widget_get_value (GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;

  if (priv->source)
    return priv->source->value;

  return "";
}
