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
#include <ctype.h>

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
#include "gtkradiobutton.h"
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
  GtkWidget *box;
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

G_DEFINE_TYPE (GtkPrinterOptionWidget, gtk_printer_option_widget, GTK_TYPE_HBOX)

static void gtk_printer_option_widget_set_property (GObject      *object,
						    guint         prop_id,
						    const GValue *value,
						    GParamSpec   *pspec);
static void gtk_printer_option_widget_get_property (GObject      *object,
						    guint         prop_id,
						    GValue       *value,
						    GParamSpec   *pspec);
static gboolean gtk_printer_option_widget_mnemonic_activate (GtkWidget *widget,
							      gboolean  group_cycling);

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

  gtk_box_set_spacing (GTK_BOX (widget), 12);
}

static void
gtk_printer_option_widget_finalize (GObject *object)
{
  GtkPrinterOptionWidget *widget = GTK_PRINTER_OPTION_WIDGET (object);
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  
  if (priv->source)
    {
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

  /* make sure entry and combo are destroyed first */
  /* as we use the two of them to create the filechooser */
  if (priv->filechooser)
    {
      gtk_widget_destroy (priv->filechooser);
      priv->filechooser = NULL;
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
filesave_changed_cb (GtkWidget              *button,
                     GtkPrinterOptionWidget *widget)
{
  GtkPrinterOptionWidgetPrivate *priv = widget->priv;
  gchar *uri, *file;
  gchar *directory;

  file = g_filename_from_utf8 (gtk_entry_get_text (GTK_ENTRY (priv->entry)),
			       -1, NULL, NULL, NULL);
  if (file == NULL)
    return;

  /* combine the value of the chooser with the value of the entry */
  g_signal_handler_block (priv->source, priv->source_changed_handler);  

  directory = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->combo));

  if ((g_uri_parse_scheme (file) == NULL) && (directory != NULL))
    {
      if (g_path_is_absolute (file))
        uri = g_filename_to_uri (file, NULL, NULL);
      else
        {
          gchar *path;

#ifdef G_OS_UNIX
          if (file[0] == '~' && file[1] == '/')
            {
              path = g_build_filename (g_get_home_dir (), file + 2, NULL);
            }
          else
#endif
            {
              path = g_build_filename (directory, file, NULL);
            }

          uri = g_filename_to_uri (path, NULL, NULL);

          g_free (path);
        }
    }
  else
    {
      if (g_uri_parse_scheme (file) != NULL)
        uri = g_strdup (file);
      else
        uri = g_build_path ("/", gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (priv->combo)), file, NULL);
    }
 
  if (uri)
    gtk_printer_option_set (priv->source, uri);

  g_free (uri);
  g_free (file);
  g_free (directory);

  g_signal_handler_unblock (priv->source, priv->source_changed_handler);
  emit_changed (widget);
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
      priv->box = gtk_hbox_new (FALSE, 12);
      gtk_widget_show (priv->box);
      gtk_box_pack_start (GTK_BOX (widget), priv->box, TRUE, TRUE, 0);
      for (i = 0; i < source->num_choices; i++)
	group = alternative_append (priv->box,
				    source->choices_display[i],
				    source->choices[i],
				    widget,
				    group);

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
      {
        GtkWidget *label;
        
        priv->filechooser = gtk_table_new (2, 2, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (priv->filechooser), 6);
        gtk_table_set_col_spacings (GTK_TABLE (priv->filechooser), 12);

        /* TODO: make this a gtkfilechooserentry once we move to GTK */
        priv->entry = gtk_entry_new ();
        priv->combo = gtk_file_chooser_button_new (source->display_text,
                                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

        g_object_set (priv->combo, "local-only", FALSE, NULL);
        gtk_entry_set_activates_default (GTK_ENTRY (priv->entry),
                                         gtk_printer_option_get_activates_default (source));

        label = gtk_label_new_with_mnemonic (_("_Name:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->entry);

        gtk_table_attach (GTK_TABLE (priv->filechooser), label,
                          0, 1, 0, 1, GTK_FILL, 0,
                          0, 0);

        gtk_table_attach (GTK_TABLE (priv->filechooser), priv->entry,
                          1, 2, 0, 1, GTK_FILL, 0,
                          0, 0);

        label = gtk_label_new_with_mnemonic (_("_Save in folder:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->combo);

        gtk_table_attach (GTK_TABLE (priv->filechooser), label,
                          0, 1, 1, 2, GTK_FILL, 0,
                          0, 0);

        gtk_table_attach (GTK_TABLE (priv->filechooser), priv->combo,
                          1, 2, 1, 2, GTK_FILL, 0,
                          0, 0);

        gtk_widget_show_all (priv->filechooser);
        gtk_box_pack_start (GTK_BOX (widget), priv->filechooser, TRUE, TRUE, 0);

        g_signal_connect (priv->entry, "changed", G_CALLBACK (filesave_changed_cb), widget);

        g_signal_connect (priv->combo, "selection-changed", G_CALLBACK (filesave_changed_cb), widget);
      }
      break;
    default:
      break;
    }

  priv->image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_MENU);
  gtk_box_pack_start (GTK_BOX (widget), priv->image, FALSE, FALSE, 0);
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
        gchar *filename = g_filename_from_uri (source->value, NULL, NULL);
        if (filename != NULL)
          {
            gchar *basename, *dirname, *text;

            basename = g_path_get_basename (filename);
            dirname = g_path_get_dirname (filename);
            text = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);

            if (text != NULL)
              gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
            if (g_path_is_absolute (dirname))
              gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (priv->combo),
                                                   dirname);
            g_free (text);
            g_free (basename);
            g_free (dirname);
            g_free (filename);
          }
	else
	  gtk_entry_set_text (GTK_ENTRY (priv->entry), source->value);
	break;
      }
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

#define __GTK_PRINTER_OPTION_WIDGET_C__
#include "gtkaliasdef.c"
