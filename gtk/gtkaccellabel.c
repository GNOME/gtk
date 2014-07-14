/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkAccelLabel: GtkLabel with accelerator monitoring facilities.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <string.h>

#include "gtkaccellabel.h"
#include "gtkaccelmap.h"
#include "gtkmain.h"
#include "gtksizerequest.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkaccellabel
 * @Short_description: A label which displays an accelerator key on the right of the text
 * @Title: GtkAccelLabel
 * @See_also: #GtkAccelGroup
 *
 * The #GtkAccelLabel widget is a subclass of #GtkLabel that also displays an
 * accelerator key on the right of the label text, e.g. “Ctl+S”.
 * It is commonly used in menus to show the keyboard short-cuts for commands.
 *
 * The accelerator key to display is not set explicitly.
 * Instead, the #GtkAccelLabel displays the accelerators which have been added to
 * a particular widget. This widget is set by calling
 * gtk_accel_label_set_accel_widget().
 *
 * For example, a #GtkMenuItem widget may have an accelerator added to emit the
 * “activate” signal when the “Ctl+S” key combination is pressed.
 * A #GtkAccelLabel is created and added to the #GtkMenuItem, and
 * gtk_accel_label_set_accel_widget() is called with the #GtkMenuItem as the
 * second argument. The #GtkAccelLabel will now display “Ctl+S” after its label.
 *
 * Note that creating a #GtkMenuItem with gtk_menu_item_new_with_label() (or
 * one of the similar functions for #GtkCheckMenuItem and #GtkRadioMenuItem)
 * automatically adds a #GtkAccelLabel to the #GtkMenuItem and calls
 * gtk_accel_label_set_accel_widget() to set it up for you.
 *
 * A #GtkAccelLabel will only display accelerators which have %GTK_ACCEL_VISIBLE
 * set (see #GtkAccelFlags).
 * A #GtkAccelLabel can display multiple accelerators and even signal names,
 * though it is almost always used to display just one accelerator key.
 *
 * ## Creating a simple menu item with an accelerator key.
 *
 * |[<!-- language="C" -->
 *   GtkWidget *save_item;
 *   GtkAccelGroup *accel_group;
 *
 *   // Create a GtkAccelGroup and add it to the window.
 *   accel_group = gtk_accel_group_new ();
 *   gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
 *
 *   // Create the menu item using the convenience function.
 *   save_item = gtk_menu_item_new_with_label ("Save");
 *   gtk_widget_show (save_item);
 *   gtk_container_add (GTK_CONTAINER (menu), save_item);
 *
 *   // Now add the accelerator to the GtkMenuItem. Note that since we
 *   // called gtk_menu_item_new_with_label() to create the GtkMenuItem
 *   // the GtkAccelLabel is automatically set up to display the
 *   // GtkMenuItem accelerators. We just need to make sure we use
 *   // GTK_ACCEL_VISIBLE here.
 *   gtk_widget_add_accelerator (save_item, "activate", accel_group,
 *                               GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
 * ]|
 */

enum {
  PROP_0,
  PROP_ACCEL_CLOSURE,
  PROP_ACCEL_WIDGET,
  LAST_PROP
};

struct _GtkAccelLabelPrivate
{
  GtkWidget     *accel_widget;       /* done */
  GClosure      *accel_closure;      /* has set function */
  GtkAccelGroup *accel_group;        /* set by set_accel_closure() */
  gchar         *accel_string;       /* has set function */
  guint          accel_padding;      /* should be style property? */
  guint16        accel_string_width; /* seems to be private */

  guint           accel_key;         /* manual accel key specification if != 0 */
  GdkModifierType accel_mods;
};

GParamSpec *props[LAST_PROP] = { NULL, };

static void         gtk_accel_label_set_property (GObject            *object,
						  guint               prop_id,
						  const GValue       *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_get_property (GObject            *object,
						  guint               prop_id,
						  GValue             *value,
						  GParamSpec         *pspec);
static void         gtk_accel_label_destroy      (GtkWidget          *widget);
static void         gtk_accel_label_finalize     (GObject            *object);
static gboolean     gtk_accel_label_draw         (GtkWidget          *widget,
                                                  cairo_t            *cr);
static const gchar *gtk_accel_label_get_string   (GtkAccelLabel      *accel_label);


static void         gtk_accel_label_get_preferred_width (GtkWidget           *widget,
                                                         gint                *min_width,
                                                         gint                *nat_width);


G_DEFINE_TYPE_WITH_PRIVATE (GtkAccelLabel, gtk_accel_label, GTK_TYPE_LABEL)

static void
gtk_accel_label_class_init (GtkAccelLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  gobject_class->finalize = gtk_accel_label_finalize;
  gobject_class->set_property = gtk_accel_label_set_property;
  gobject_class->get_property = gtk_accel_label_get_property;

  widget_class->draw = gtk_accel_label_draw;
  widget_class->get_preferred_width = gtk_accel_label_get_preferred_width;
  widget_class->destroy = gtk_accel_label_destroy;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_ACCEL_LABEL);

  class->signal_quote1 = g_strdup ("<:");
  class->signal_quote2 = g_strdup (":>");

#ifndef GDK_WINDOWING_QUARTZ
  /* This is the text that should appear next to menu accelerators
   * that use the shift key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_shift = g_strdup (C_("keyboard label", "Shift"));
  /* This is the text that should appear next to menu accelerators
   * that use the control key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_control = g_strdup (C_("keyboard label", "Ctrl"));
  /* This is the text that should appear next to menu accelerators
   * that use the alt key. If the text on this key isn't typically
   * translated on keyboards used for your language, don't translate
   * this.
   */
  class->mod_name_alt = g_strdup (C_("keyboard label", "Alt"));
  class->mod_separator = g_strdup ("+");
#else /* GDK_WINDOWING_QUARTZ */

  /* U+21E7 UPWARDS WHITE ARROW */
  class->mod_name_shift = g_strdup ("\xe2\x87\xa7");
  /* U+2303 UP ARROWHEAD */
  class->mod_name_control = g_strdup ("\xe2\x8c\x83");
  /* U+2325 OPTION KEY */
  class->mod_name_alt = g_strdup ("\xe2\x8c\xa5");
  class->mod_separator = g_strdup ("");

#endif /* GDK_WINDOWING_QUARTZ */

  props[PROP_ACCEL_CLOSURE] =
    g_param_spec_boxed ("accel-closure",
                        P_("Accelerator Closure"),
                        P_("The closure to be monitored for accelerator changes"),
                        G_TYPE_CLOSURE,
                        GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACCEL_WIDGET] =
    g_param_spec_object ("accel-widget",
                         P_("Accelerator Widget"),
                         P_("The widget to be monitored for accelerator changes"),
                         GTK_TYPE_WIDGET,
                         GTK_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, LAST_PROP, props);
}

static void
gtk_accel_label_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      gtk_accel_label_set_accel_closure (accel_label, g_value_get_boxed (value));
      break;
    case PROP_ACCEL_WIDGET:
      gtk_accel_label_set_accel_widget (accel_label, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GtkAccelLabel  *accel_label;

  accel_label = GTK_ACCEL_LABEL (object);

  switch (prop_id)
    {
    case PROP_ACCEL_CLOSURE:
      g_value_set_boxed (value, accel_label->priv->accel_closure);
      break;
    case PROP_ACCEL_WIDGET:
      g_value_set_object (value, accel_label->priv->accel_widget);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_accel_label_init (GtkAccelLabel *accel_label)
{
  GtkAccelLabelPrivate *priv;

  accel_label->priv = gtk_accel_label_get_instance_private (accel_label);
  priv = accel_label->priv;

  priv->accel_padding = 3;
  priv->accel_widget = NULL;
  priv->accel_closure = NULL;
  priv->accel_group = NULL;
  priv->accel_string = NULL;
}

/**
 * gtk_accel_label_new:
 * @string: the label string. Must be non-%NULL.
 *
 * Creates a new #GtkAccelLabel.
 *
 * Returns: a new #GtkAccelLabel.
 */
GtkWidget*
gtk_accel_label_new (const gchar *string)
{
  GtkAccelLabel *accel_label;
  
  g_return_val_if_fail (string != NULL, NULL);
  
  accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
  
  gtk_label_set_text (GTK_LABEL (accel_label), string);
  
  return GTK_WIDGET (accel_label);
}

static void
gtk_accel_label_destroy (GtkWidget *widget)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (widget);

  gtk_accel_label_set_accel_widget (accel_label, NULL);
  gtk_accel_label_set_accel_closure (accel_label, NULL);

  GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->destroy (widget);
}

static void
gtk_accel_label_finalize (GObject *object)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (object);

  g_free (accel_label->priv->accel_string);

  G_OBJECT_CLASS (gtk_accel_label_parent_class)->finalize (object);
}

/**
 * gtk_accel_label_get_accel_widget:
 * @accel_label: a #GtkAccelLabel
 *
 * Fetches the widget monitored by this accelerator label. See
 * gtk_accel_label_set_accel_widget().
 *
 * Returns: (transfer none): the object monitored by the accelerator label, or %NULL.
 **/
GtkWidget*
gtk_accel_label_get_accel_widget (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), NULL);

  return accel_label->priv->accel_widget;
}

/**
 * gtk_accel_label_get_accel_width:
 * @accel_label: a #GtkAccelLabel.
 *
 * Returns the width needed to display the accelerator key(s).
 * This is used by menus to align all of the #GtkMenuItem widgets, and shouldn't
 * be needed by applications.
 *
 * Returns: the width needed to display the accelerator key(s).
 */
guint
gtk_accel_label_get_accel_width (GtkAccelLabel *accel_label)
{
  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), 0);

  return (accel_label->priv->accel_string_width +
	  (accel_label->priv->accel_string_width ? accel_label->priv->accel_padding : 0));
}

static void
gtk_accel_label_get_preferred_width (GtkWidget       *widget,
                                     gint            *min_width,
                                     gint            *nat_width)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (widget);
  PangoLayout   *layout;
  gint           width;

  GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->get_preferred_width (widget, min_width, nat_width);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (widget), 
					   gtk_accel_label_get_string (accel_label));
  pango_layout_get_pixel_size (layout, &width, NULL);
  accel_label->priv->accel_string_width = width;

  g_object_unref (layout);
}

static gint
get_first_baseline (PangoLayout *layout)
{
  PangoLayoutIter *iter;
  gint result;

  iter = pango_layout_get_iter (layout);
  result = pango_layout_iter_get_baseline (iter);
  pango_layout_iter_free (iter);

  return PANGO_PIXELS (result);
}

static gboolean 
gtk_accel_label_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  GtkAccelLabel *accel_label = GTK_ACCEL_LABEL (widget);
  GtkTextDirection direction;
  guint ac_width;
  GtkAllocation allocation;
  GtkRequisition requisition;

  direction = gtk_widget_get_direction (widget);
  ac_width = gtk_accel_label_get_accel_width (accel_label);
  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_get_preferred_size (widget, NULL, &requisition);

  if (allocation.width >= requisition.width + ac_width)
    {
      GtkStyleContext *context;
      PangoLayout *label_layout;
      PangoLayout *accel_layout;
      GtkLabel *label = GTK_LABEL (widget);

      gint x;
      gint y;

      context = gtk_widget_get_style_context (widget);
      label_layout = gtk_label_get_layout (GTK_LABEL (accel_label));

      cairo_save (cr);

      /* XXX: Mad hack: We modify the label's width so it renders
       * properly in its draw function that we chain to. */
      if (direction == GTK_TEXT_DIR_RTL)
        cairo_translate (cr, ac_width, 0);
      if (gtk_label_get_ellipsize (label))
        pango_layout_set_width (label_layout,
                                pango_layout_get_width (label_layout) 
                                - ac_width * PANGO_SCALE);
      
      allocation.width -= ac_width;
      gtk_widget_set_allocation (widget, &allocation);
      GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->draw (widget, cr);
      allocation.width += ac_width;
      gtk_widget_set_allocation (widget, &allocation);
      if (gtk_label_get_ellipsize (label))
        pango_layout_set_width (label_layout,
                                pango_layout_get_width (label_layout) 
                                + ac_width * PANGO_SCALE);

      cairo_restore (cr);

      if (direction == GTK_TEXT_DIR_RTL)
        x = 0;
      else
        x = gtk_widget_get_allocated_width (widget) - ac_width;

      gtk_label_get_layout_offsets (GTK_LABEL (accel_label), NULL, &y);

      accel_layout = gtk_widget_create_pango_layout (widget, gtk_accel_label_get_string (accel_label));

      y += get_first_baseline (label_layout) - get_first_baseline (accel_layout) - allocation.y;

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_ACCELERATOR);
      gtk_render_layout (context, cr, x, y, accel_layout);
      gtk_style_context_restore (context);

      g_object_unref (accel_layout);
    }
  else
    {
      GTK_WIDGET_CLASS (gtk_accel_label_parent_class)->draw (widget, cr);
    }
  
  return FALSE;
}

static void
refetch_widget_accel_closure (GtkAccelLabel *accel_label)
{
  GClosure *closure = NULL;
  GList *clist, *list;
  
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  g_return_if_fail (GTK_IS_WIDGET (accel_label->priv->accel_widget));
  
  clist = gtk_widget_list_accel_closures (accel_label->priv->accel_widget);
  for (list = clist; list; list = list->next)
    {
      /* we just take the first closure used */
      closure = list->data;
      break;
    }
  g_list_free (clist);
  gtk_accel_label_set_accel_closure (accel_label, closure);
}

static void
accel_widget_weak_ref_cb (GtkAccelLabel *accel_label,
                          GtkWidget     *old_accel_widget)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  g_return_if_fail (GTK_IS_WIDGET (accel_label->priv->accel_widget));

  g_signal_handlers_disconnect_by_func (accel_label->priv->accel_widget,
                                        refetch_widget_accel_closure,
                                        accel_label);
  accel_label->priv->accel_widget = NULL;
  g_object_notify_by_pspec (G_OBJECT (accel_label), props[PROP_ACCEL_WIDGET]);
}

/**
 * gtk_accel_label_set_accel_widget:
 * @accel_label: a #GtkAccelLabel
 * @accel_widget: the widget to be monitored.
 *
 * Sets the widget to be monitored by this accelerator label.
 */
void
gtk_accel_label_set_accel_widget (GtkAccelLabel *accel_label,
                                  GtkWidget     *accel_widget)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_widget)
    g_return_if_fail (GTK_IS_WIDGET (accel_widget));

  if (accel_widget != accel_label->priv->accel_widget)
    {
      if (accel_label->priv->accel_widget)
        {
          gtk_accel_label_set_accel_closure (accel_label, NULL);
          g_signal_handlers_disconnect_by_func (accel_label->priv->accel_widget,
                                                refetch_widget_accel_closure,
                                                accel_label);
          g_object_weak_unref (G_OBJECT (accel_label->priv->accel_widget),
                               (GWeakNotify) accel_widget_weak_ref_cb, accel_label);
        }
      accel_label->priv->accel_widget = accel_widget;
      if (accel_label->priv->accel_widget)
        {
          g_object_weak_ref (G_OBJECT (accel_label->priv->accel_widget),
                             (GWeakNotify) accel_widget_weak_ref_cb, accel_label);
          g_signal_connect_object (accel_label->priv->accel_widget, "accel-closures-changed",
                                   G_CALLBACK (refetch_widget_accel_closure),
                                   accel_label, G_CONNECT_SWAPPED);
          refetch_widget_accel_closure (accel_label);
        }
      g_object_notify_by_pspec (G_OBJECT (accel_label), props[PROP_ACCEL_WIDGET]);
    }
}

static void
gtk_accel_label_reset (GtkAccelLabel *accel_label)
{
  g_clear_pointer (&accel_label->priv->accel_string, g_free);
  
  gtk_widget_queue_resize (GTK_WIDGET (accel_label));
}

static void
check_accel_changed (GtkAccelGroup  *accel_group,
		     guint           keyval,
		     GdkModifierType modifier,
		     GClosure       *accel_closure,
		     GtkAccelLabel  *accel_label)
{
  if (accel_closure == accel_label->priv->accel_closure)
    gtk_accel_label_reset (accel_label);
}

/**
 * gtk_accel_label_set_accel_closure:
 * @accel_label: a #GtkAccelLabel
 * @accel_closure: the closure to monitor for accelerator changes.
 *
 * Sets the closure to be monitored by this accelerator label. The closure
 * must be connected to an accelerator group; see gtk_accel_group_connect().
 **/
void
gtk_accel_label_set_accel_closure (GtkAccelLabel *accel_label,
				   GClosure      *accel_closure)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));
  if (accel_closure)
    g_return_if_fail (gtk_accel_group_from_accel_closure (accel_closure) != NULL);

  if (accel_closure != accel_label->priv->accel_closure)
    {
      if (accel_label->priv->accel_closure)
	{
	  g_signal_handlers_disconnect_by_func (accel_label->priv->accel_group,
						check_accel_changed,
						accel_label);
	  accel_label->priv->accel_group = NULL;
	  g_closure_unref (accel_label->priv->accel_closure);
	}
      accel_label->priv->accel_closure = accel_closure;
      if (accel_label->priv->accel_closure)
	{
	  g_closure_ref (accel_label->priv->accel_closure);
	  accel_label->priv->accel_group = gtk_accel_group_from_accel_closure (accel_closure);
	  g_signal_connect_object (accel_label->priv->accel_group, "accel-changed",
				   G_CALLBACK (check_accel_changed),
				   accel_label, 0);
	}
      gtk_accel_label_reset (accel_label);
      g_object_notify_by_pspec (G_OBJECT (accel_label), props[PROP_ACCEL_CLOSURE]);
    }
}

static gboolean
find_accel (GtkAccelKey *key,
	    GClosure    *closure,
	    gpointer     data)
{
  return data == (gpointer) closure;
}

static const gchar *
gtk_accel_label_get_string (GtkAccelLabel *accel_label)
{
  if (!accel_label->priv->accel_string)
    gtk_accel_label_refetch (accel_label);
  
  return accel_label->priv->accel_string;
}

/* Underscores in key names are better displayed as spaces
 * E.g., Page_Up should be “Page Up”.
 *
 * Some keynames also have prefixes that are not suitable
 * for display, e.g XF86AudioMute, so strip those out, too.
 *
 * This function is only called on untranslated keynames,
 * so no need to be UTF-8 safe.
 */
static void
append_without_underscores (GString *s,
                            gchar   *str)
{
  gchar *p;

  if (g_str_has_prefix (str, "XF86"))
    p = str + 4;
  else if (g_str_has_prefix (str, "ISO_"))
    p = str + 4;
  else
    p = str;

  for ( ; *p; p++)
    {
      if (*p == '_')
        g_string_append_c (s, ' ');
      else
        g_string_append_c (s, *p);
    }
}

/* On Mac, if the key has symbolic representation (e.g. arrow keys),
 * append it to gstring and return TRUE; otherwise return FALSE.
 * See http://docs.info.apple.com/article.html?path=Mac/10.5/en/cdb_symbs.html 
 * for the list of special keys. */
static gboolean
append_keyval_symbol (guint    accelerator_key,
                      GString *gstring)
{
#ifdef GDK_WINDOWING_QUARTZ
  switch (accelerator_key)
  {
  case GDK_KEY_Return:
    /* U+21A9 LEFTWARDS ARROW WITH HOOK */
    g_string_append (gstring, "\xe2\x86\xa9");
    return TRUE;

  case GDK_KEY_ISO_Enter:
    /* U+2324 UP ARROWHEAD BETWEEN TWO HORIZONTAL BARS */
    g_string_append (gstring, "\xe2\x8c\xa4");
    return TRUE;

  case GDK_KEY_Left:
    /* U+2190 LEFTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x90");
    return TRUE;

  case GDK_KEY_Up:
    /* U+2191 UPWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x91");
    return TRUE;

  case GDK_KEY_Right:
    /* U+2192 RIGHTWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x92");
    return TRUE;

  case GDK_KEY_Down:
    /* U+2193 DOWNWARDS ARROW */
    g_string_append (gstring, "\xe2\x86\x93");
    return TRUE;

  case GDK_KEY_Page_Up:
    /* U+21DE UPWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9e");
    return TRUE;

  case GDK_KEY_Page_Down:
    /* U+21DF DOWNWARDS ARROW WITH DOUBLE STROKE */
    g_string_append (gstring, "\xe2\x87\x9f");
    return TRUE;

  case GDK_KEY_Home:
    /* U+2196 NORTH WEST ARROW */
    g_string_append (gstring, "\xe2\x86\x96");
    return TRUE;

  case GDK_KEY_End:
    /* U+2198 SOUTH EAST ARROW */
    g_string_append (gstring, "\xe2\x86\x98");
    return TRUE;

  case GDK_KEY_Escape:
    /* U+238B BROKEN CIRCLE WITH NORTHWEST ARROW */
    g_string_append (gstring, "\xe2\x8e\x8b");
    return TRUE;

  case GDK_KEY_BackSpace:
    /* U+232B ERASE TO THE LEFT */
    g_string_append (gstring, "\xe2\x8c\xab");
    return TRUE;

  case GDK_KEY_Delete:
    /* U+2326 ERASE TO THE RIGHT */
    g_string_append (gstring, "\xe2\x8c\xa6");
    return TRUE;

  default:
    return FALSE;
  }
#else /* !GDK_WINDOWING_QUARTZ */
  return FALSE;
#endif
}

gchar *
_gtk_accel_label_class_get_accelerator_label (GtkAccelLabelClass *klass,
					      guint               accelerator_key,
					      GdkModifierType     accelerator_mods)
{
  GString *gstring;
  gboolean seen_mod = FALSE;
  gunichar ch;
  
  gstring = g_string_new ("");
  
  if (accelerator_mods & GDK_SHIFT_MASK)
    {
      g_string_append (gstring, klass->mod_name_shift);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_CONTROL_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);
      g_string_append (gstring, klass->mod_name_control);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD1_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);
      g_string_append (gstring, klass->mod_name_alt);
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD2_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod2");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD3_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod3");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD4_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod4");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_MOD5_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      g_string_append (gstring, "Mod5");
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_SUPER_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      /* This is the text that should appear next to menu accelerators
       * that use the super key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Super"));
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_HYPER_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

      /* This is the text that should appear next to menu accelerators
       * that use the hyper key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Hyper"));
      seen_mod = TRUE;
    }
  if (accelerator_mods & GDK_META_MASK)
    {
      if (seen_mod)
	g_string_append (gstring, klass->mod_separator);

#ifndef GDK_WINDOWING_QUARTZ
      /* This is the text that should appear next to menu accelerators
       * that use the meta key. If the text on this key isn't typically
       * translated on keyboards used for your language, don't translate
       * this.
       */
      g_string_append (gstring, C_("keyboard label", "Meta"));
#else
      /* Command key symbol U+2318 PLACE OF INTEREST SIGN */
      g_string_append (gstring, "\xe2\x8c\x98");
#endif
      seen_mod = TRUE;
    }
  
  ch = gdk_keyval_to_unicode (accelerator_key);
  if (ch && ch < 0x80 && (g_unichar_isgraph (ch) || ch == ' '))
    {
      if (seen_mod)
        g_string_append (gstring, klass->mod_separator);

      switch (ch)
	{
	case ' ':
	  g_string_append (gstring, C_("keyboard label", "Space"));
	  break;
	case '\\':
	  g_string_append (gstring, C_("keyboard label", "Backslash"));
	  break;
	default:
	  g_string_append_unichar (gstring, g_unichar_toupper (ch));
	  break;
	}
    }
  else if (!append_keyval_symbol (accelerator_key, gstring))
    {
      gchar *tmp;

      tmp = gdk_keyval_name (gdk_keyval_to_lower (accelerator_key));
      if (tmp != NULL)
	{
          if (seen_mod)
            g_string_append (gstring, klass->mod_separator);

	  if (tmp[0] != 0 && tmp[1] == 0)
	    g_string_append_c (gstring, g_ascii_toupper (tmp[0]));
	  else
	    {
	      const gchar *str;
              str = g_dpgettext2 (GETTEXT_PACKAGE, "keyboard label", tmp);
	      if (str == tmp)
                append_without_underscores (gstring, tmp);
	      else
		g_string_append (gstring, str);
	    }
	}
    }

  return g_string_free (gstring, FALSE);
}

/**
 * gtk_accel_label_refetch:
 * @accel_label: a #GtkAccelLabel.
 *
 * Recreates the string representing the accelerator keys.
 * This should not be needed since the string is automatically updated whenever
 * accelerators are added or removed from the associated widget.
 *
 * Returns: always returns %FALSE.
 */
gboolean
gtk_accel_label_refetch (GtkAccelLabel *accel_label)
{
  gboolean enable_accels;

  g_return_val_if_fail (GTK_IS_ACCEL_LABEL (accel_label), FALSE);

  g_clear_pointer (&accel_label->priv->accel_string, g_free);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (accel_label)),
                "gtk-enable-accels", &enable_accels,
                NULL);

  if (enable_accels && (accel_label->priv->accel_closure || accel_label->priv->accel_key))
    {
      gboolean have_accel = FALSE;
      guint accel_key;
      GdkModifierType accel_mods;

      /* First check for a manual accel set with _set_accel() */
      if (accel_label->priv->accel_key)
        {
          accel_mods = accel_label->priv->accel_mods;
          accel_key = accel_label->priv->accel_key;
          have_accel = TRUE;
        }

      /* If we don't have a hardcoded value, check the accel group */
      if (!have_accel)
        {
          GtkAccelKey *key;

          key = gtk_accel_group_find (accel_label->priv->accel_group, find_accel, accel_label->priv->accel_closure);

          if (key && key->accel_flags & GTK_ACCEL_VISIBLE)
            {
              accel_key = key->accel_key;
              accel_mods = key->accel_mods;
              have_accel = TRUE;
            }
        }

      /* If we found a key using either method, set it */
      if (have_accel)
	{
	  GtkAccelLabelClass *klass;
	  gchar *tmp;

	  klass = GTK_ACCEL_LABEL_GET_CLASS (accel_label);
	  tmp = _gtk_accel_label_class_get_accelerator_label (klass, accel_key, accel_mods);
	  accel_label->priv->accel_string = g_strconcat ("   ", tmp, NULL);
	  g_free (tmp);
	}

      else
        /* Otherwise we have a closure with no key.  Show "-/-". */
        accel_label->priv->accel_string = g_strdup ("-/-");
    }

  if (!accel_label->priv->accel_string)
    accel_label->priv->accel_string = g_strdup ("");

  gtk_widget_queue_resize (GTK_WIDGET (accel_label));

  return FALSE;
}

/**
 * gtk_accel_label_set_accel:
 * @accel_label: a #GtkAccelLabel
 * @accelerator_key: a keyval, or 0
 * @accelerator_mods: the modifier mask for the accel
 *
 * Manually sets a keyval and modifier mask as the accelerator rendered
 * by @accel_label.
 *
 * If a keyval and modifier are explicitly set then these values are
 * used regardless of any associated accel closure or widget.
 *
 * Providing an @accelerator_key of 0 removes the manual setting.
 *
 * Since: 3.6
 */
void
gtk_accel_label_set_accel (GtkAccelLabel   *accel_label,
                           guint            accelerator_key,
                           GdkModifierType  accelerator_mods)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  accel_label->priv->accel_key = accelerator_key;
  accel_label->priv->accel_mods = accelerator_mods;

  gtk_accel_label_reset (accel_label);
}

/**
 * gtk_accel_label_get_accel:
 * @accel_label: a #GtkAccelLabel
 * @accelerator_key: (out): return location for the keyval
 * @accelerator_mods: (out): return location for the modifier mask
 *
 * Gets the keyval and modifier mask set with
 * gtk_accel_label_set_accel().
 *
 * Since: 3.12
 */
void
gtk_accel_label_get_accel (GtkAccelLabel   *accel_label,
                           guint           *accelerator_key,
                           GdkModifierType *accelerator_mods)
{
  g_return_if_fail (GTK_IS_ACCEL_LABEL (accel_label));

  *accelerator_key = accel_label->priv->accel_key;
  *accelerator_mods = accel_label->priv->accel_mods;
}
