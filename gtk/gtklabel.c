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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>
#include <string.h>
#include "gtklabel.h"
#include "gdk/gdkkeysyms.h"
#include "gtkclipboard.h"
#include "gdk/gdki18n.h"
#include <pango/pango.h>
#include "gtkintl.h"

struct _GtkLabelSelectionInfo
{
  GdkWindow *window;
  gint selection_anchor;
  gint selection_end;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ATTRIBUTES,
  PROP_USE_MARKUP,
  PROP_USE_UNDERLINE,
  PROP_JUSTIFY,
  PROP_PATTERN,
  PROP_WRAP,
  PROP_SELECTABLE,
  PROP_ACCEL_KEYVAL
};

static void gtk_label_class_init        (GtkLabelClass    *klass);
static void gtk_label_init              (GtkLabel         *label);
static void gtk_label_set_property      (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec,
					 const gchar      *trailer);
static void gtk_label_get_property      (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec,
					 const gchar      *trailer);
static void gtk_label_finalize          (GObject          *object);
static void gtk_label_size_request      (GtkWidget        *widget,
					 GtkRequisition   *requisition);
static void gtk_label_size_allocate     (GtkWidget        *widget,
                                         GtkAllocation    *allocation);
static void gtk_label_style_set         (GtkWidget        *widget,
					 GtkStyle         *previous_style);
static void gtk_label_direction_changed (GtkWidget        *widget,
					 GtkTextDirection  previous_dir);
static gint gtk_label_expose            (GtkWidget        *widget,
					 GdkEventExpose   *event);

static void gtk_label_realize           (GtkWidget        *widget);
static void gtk_label_unrealize         (GtkWidget        *widget);
static void gtk_label_map               (GtkWidget        *widget);
static void gtk_label_unmap             (GtkWidget        *widget);
static gint gtk_label_button_press      (GtkWidget        *widget,
                                         GdkEventButton   *event);
static gint gtk_label_button_release    (GtkWidget        *widget,
                                         GdkEventButton   *event);
static gint gtk_label_motion            (GtkWidget        *widget,
                                         GdkEventMotion   *event);


static void gtk_label_set_text_internal          (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_label_internal         (GtkLabel      *label,
						  gchar         *str);
static void gtk_label_set_use_markup_internal    (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_use_underline_internal (GtkLabel      *label,
						  gboolean       val);
static void gtk_label_set_attributes_internal    (GtkLabel      *label,
						  PangoAttrList *attrs);
static void gtk_label_set_uline_text_internal    (GtkLabel      *label,
						  const gchar   *str);
static void gtk_label_set_pattern_internal       (GtkLabel      *label,
				                  const gchar   *pattern);
static void set_markup                           (GtkLabel      *label,
						  const gchar   *str,
						  gboolean       with_uline);
static void gtk_label_recalculate                (GtkLabel      *label);

static void gtk_label_create_window       (GtkLabel *label);
static void gtk_label_destroy_window      (GtkLabel *label);
static void gtk_label_clear_layout        (GtkLabel *label);
static void gtk_label_ensure_layout       (GtkLabel *label,
                                           gint     *widthp,
                                           gint     *heightp);
static void gtk_label_select_region_index (GtkLabel *label,
                                           gint      anchor_index,
                                           gint      end_index);


static GtkMiscClass *parent_class = NULL;


GtkType
gtk_label_get_type (void)
{
  static GtkType label_type = 0;
  
  if (!label_type)
    {
      static const GTypeInfo label_info =
      {
	sizeof (GtkLabelClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) gtk_label_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (GtkLabel),
	32,             /* n_preallocs */
	(GInstanceInitFunc) gtk_label_init,
      };

      label_type = g_type_register_static (GTK_TYPE_MISC, "GtkLabel", &label_info, 0);
    }
  
  return label_type;
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_MISC);
  
  gobject_class->set_property = gtk_label_set_property;
  gobject_class->get_property = gtk_label_get_property;
  gobject_class->finalize = gtk_label_finalize;
  
  widget_class->size_request = gtk_label_size_request;
  widget_class->size_allocate = gtk_label_size_allocate;
  widget_class->style_set = gtk_label_style_set;
  widget_class->direction_changed = gtk_label_direction_changed;
  widget_class->expose_event = gtk_label_expose;
  widget_class->realize = gtk_label_realize;
  widget_class->unrealize = gtk_label_unrealize;
  widget_class->map = gtk_label_map;
  widget_class->unmap = gtk_label_unmap;
  widget_class->button_press_event = gtk_label_button_press;
  widget_class->button_release_event = gtk_label_button_release;
  widget_class->motion_notify_event = gtk_label_motion;

  g_object_class_install_property (G_OBJECT_CLASS(object_class),
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        _("Label"),
                                                        _("The text of the label."),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       _("Attributes"),
						       _("A list of style attributes to apply to the text of the label."),
						       PANGO_TYPE_ATTR_LIST,
						       G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_MARKUP,
                                   g_param_spec_boolean ("use_markup",
							 _("Use markup"),
							 _("The text of the label includes XML markup. See pango_parse_markup()."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use_underline",
							 _("Use underline"),
							 _("If set, an underline in the text indicates the next character should be used for the accelerator key"),
                                                        FALSE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_JUSTIFY,
                                   g_param_spec_enum ("justify",
                                                      _("Justification"),
                                                      _("The alignment of the lines in the text of the label relative to each other. This does NOT affect the alignment of the label within its allocation. See GtkMisc::xalign for that."),
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PATTERN,
                                   g_param_spec_string ("pattern",
                                                        _("Pattern"),
                                                        _("A string with _ characters in positions correspond to characters in
  the text to underline."),
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                        _("Line wrap"),
                                                        _("If set, wrap lines if the text becomes too wide."),
                                                        TRUE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTABLE,
                                   g_param_spec_boolean ("selectable",
                                                        _("Selectable"),
                                                        _("Whether the label text can be selected with the mouse."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_ACCEL_KEYVAL,
                                   g_param_spec_uint ("accel_keyval",
						      _("Accelerator key value"),
						      _("The accelerator key for this label."),
						      0,
						      G_MAXUINT,
						      GDK_VoidSymbol,
						      G_PARAM_READABLE));
}

static void 
gtk_label_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec,
			const gchar  *trailer)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label_internal (label,
				    g_strdup (g_value_get_string (value)));
      gtk_label_recalculate (label);
      break;
    case PROP_ATTRIBUTES:
      gtk_label_set_attributes (label, g_value_get_boxed (value));
      break;
    case PROP_USE_MARKUP:
      gtk_label_set_use_markup_internal (label, g_value_get_boolean (value));
      gtk_label_recalculate (label);
      break;
    case PROP_USE_UNDERLINE:
      gtk_label_set_use_underline_internal (label, g_value_get_boolean (value));
      gtk_label_recalculate (label);
      break;
    case PROP_JUSTIFY:
      gtk_label_set_justify (label, g_value_get_enum (value));
      break;
    case PROP_PATTERN:
      gtk_label_set_pattern (label, g_value_get_string (value));
      break;
    case PROP_WRAP:
      gtk_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    case PROP_SELECTABLE:
      gtk_label_set_selectable (label, g_value_get_boolean (value));
      break;	  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_label_get_property (GObject     *object,
			guint        prop_id,
			GValue      *value,
			GParamSpec  *pspec,
			const gchar *trailer)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, g_strdup (label->label));
      break;
    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, label->attrs);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, label->use_markup);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, label->use_underline);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, label->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, label->wrap);
      break;
    case PROP_SELECTABLE:
      g_value_set_boolean (value, gtk_label_get_selectable (label));
      break;
    case PROP_ACCEL_KEYVAL:
      g_value_set_uint (value, label->accel_keyval);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);
  
  label->label = NULL;

  label->jtype = GTK_JUSTIFY_CENTER;
  label->wrap = FALSE;

  label->use_underline = FALSE;
  label->use_markup = FALSE;
  
  label->accel_keyval = GDK_VoidSymbol;
  label->layout = NULL;
  label->text = NULL;
  label->attrs = NULL;
  
  gtk_label_set_text (label, "");
}

GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = gtk_type_new (GTK_TYPE_LABEL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

/**
 * gtk_label_get_accel_keyval:
 * @label: a #GtkLabel
 * @Returns: GDK keyval usable for accelerators, or GDK_VoidSymbol
 *
 * If the label text was set using gtk_label_set_markup_with_accel,
 * gtk_label_parse_uline, or using the use_underline property this function
 * returns the keyval for the first underlined accelerator.
 **/
guint
gtk_label_get_accel_keyval (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);

  return label->accel_keyval;
}

static void
gtk_label_set_text_internal (GtkLabel *label,
			     gchar    *str)
{
  g_free (label->text);
  
  label->text = str;

  gtk_label_select_region_index (label, 0, 0);
}

static void
gtk_label_set_label_internal (GtkLabel *label,
			      gchar    *str)
{
  g_free (label->label);
  
  label->label = str;

  g_object_notify (G_OBJECT (label), "label");
}

static void
gtk_label_set_use_markup_internal (GtkLabel *label,
				   gboolean val)
{
  val = val != FALSE;
  if (label->use_markup != val)
    g_object_notify (G_OBJECT (label), "use_markup");
  label->use_markup = val;
}

static void
gtk_label_set_use_underline_internal (GtkLabel *label,
				      gboolean val)
{
  val = val != FALSE;
  if (label->use_underline != val)
    g_object_notify (G_OBJECT (label), "use_underline");
  label->use_underline = val;
}

static void
gtk_label_set_attributes_internal (GtkLabel         *label,
				   PangoAttrList    *attrs)
{
  if (attrs)
    pango_attr_list_ref (attrs);
  
  if (label->attrs)
    pango_attr_list_unref (label->attrs);

  label->attrs = attrs;
  g_object_notify (G_OBJECT (label), "attributes");
}


/* Calculates text, attrs and accel_keyval from
 * label, use_underline and use_markup */
static void
gtk_label_recalculate (GtkLabel *label)
{
  if (label->use_markup)
      set_markup (label, label->label, label->use_underline);
  else
    {
      if (label->use_underline)
	gtk_label_set_uline_text_internal (label, label->label);
      else
	{
	  gtk_label_set_text_internal (label, g_strdup (label->label));
	  gtk_label_set_attributes_internal (label, NULL);
	}
    }

  if (!label->use_underline)
    label->accel_keyval = GDK_VoidSymbol;

  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);
}

/**
 * gtk_label_set_attributes:
 * @label: a #GtkLabel
 * @attrs: a #PangoAttrList
 * 
 * Sets a #PangoAttrList; the attributes in the list are applied to the
 * label text.
 **/
void
gtk_label_set_attributes (GtkLabel         *label,
                          PangoAttrList    *attrs)
{
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_attributes_internal (label, attrs);
  
  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

static void
set_markup (GtkLabel    *label,
            const gchar *str,
            gboolean     with_uline)
{
  gchar *text = NULL;
  GError *error = NULL;
  PangoAttrList *attrs = NULL;
  gunichar accel_char = 0;

  if (!pango_parse_markup (str,
                           -1,
                           with_uline ? '_' : 0,
                           &attrs,
                           &text,
                           with_uline ? &accel_char : NULL,
                           &error))
    {
      g_warning ("Failed to set label from markup due to error parsing markup: %s",
                 error->message);
      g_error_free (error);
      return;
    }

  if (text)
    gtk_label_set_text_internal (label, text);

  if (attrs)
    {
      gtk_label_set_attributes_internal (label, attrs);
      pango_attr_list_unref (attrs);
    }

  if (accel_char != 0)
    label->accel_keyval = gdk_keyval_to_lower (gdk_unicode_to_keyval (accel_char));
  else
    label->accel_keyval = GDK_VoidSymbol;
}

/**
 * gtk_label_set_markup:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the Pango text markup language,
 * setting the label's text and attribute list based on the parse results.
 **/
void
gtk_label_set_markup (GtkLabel    *label,
                      const gchar *str)
{  
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, FALSE);
  
  gtk_label_recalculate (label);
}

/**
 * gtk_label_set_markup_with_accel:
 * @label: a #GtkLabel
 * @str: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 * 
 * Parses @str which is marked up with the Pango text markup language,
 * setting the label's text and attribute list based on the parse results.
 * If characters in @str are preceded by an underscore, they are underlined
 * indicating that they represent a keyboard accelerator, and the GDK
 * keyval for the first underlined accelerator is returned. If there are
 * no underlines in the text, GDK_VoidSymbol will be returned.
 *
 * Return value: GDK keyval for accelerator
 **/
guint
gtk_label_set_markup_with_accel (GtkLabel    *label,
                                 const gchar *str)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, TRUE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);
  return label->accel_keyval;
}

/**
 * gtk_label_get_text:
 * @label: a #GtkLabel
 * 
 * Fetches the text from a label widget
 * 
 * Return value: the text in the label widget. This is the internal
 * string used by the label, and must not be modified.
 **/
G_CONST_RETURN gchar *
gtk_label_get_text (GtkLabel *label)
{
  g_return_val_if_fail (label != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LABEL (label), NULL);

  return label->text;
}

static PangoAttrList *
gtk_label_pattern_to_attrs (GtkLabel      *label,
			    const gchar   *pattern)
{
  const char *start;
  const char *p = label->text;
  const char *q = pattern;
  PangoAttrList *attrs;

  attrs = pango_attr_list_new ();

  while (1)
    {
      while (*p && *q && *q != '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      start = p;
      while (*p && *q && *q == '_')
	{
	  p = g_utf8_next_char (p);
	  q++;
	}
      
      if (p > start)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_LOW);
	  attr->start_index = start - label->text;
	  attr->end_index = p - label->text;
	  
	  pango_attr_list_insert (attrs, attr);
	}
      else
	break;
    }

  return attrs;
}

static void
gtk_label_set_pattern_internal (GtkLabel    *label,
				const gchar *pattern)
{
  PangoAttrList *attrs;
  g_return_if_fail (GTK_IS_LABEL (label));
  
  attrs = gtk_label_pattern_to_attrs (label, pattern);

  gtk_label_set_attributes_internal (label, attrs);
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  gtk_label_set_pattern_internal (label, pattern);

  gtk_label_clear_layout (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}


void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      gtk_label_clear_layout (label);
      
      g_object_notify (G_OBJECT (label), "justify");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      label->wrap = wrap;
      g_object_notify (G_OBJECT (label), "wrap");
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_get (GtkLabel *label,
	       gchar   **str)
{
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);
  
  *str = label->text;
}

static void
gtk_label_finalize (GObject *object)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (object));
  
  label = GTK_LABEL (object);
  
  g_free (label->label);
  g_free (label->text);

  if (label->layout)
    g_object_unref (G_OBJECT (label->layout));

  if (label->attrs)
    pango_attr_list_unref (label->attrs);

  g_free (label->select_info);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtk_label_clear_layout (GtkLabel *label)
{
  if (label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }
}

static void
gtk_label_ensure_layout (GtkLabel *label,
                         gint     *widthp,
                         gint     *heightp)
{
  GtkWidget *widget;
  PangoRectangle logical_rect;
  gint rwidth, rheight;

  widget = GTK_WIDGET (label);

  /*
   * There are a number of conditions which will necessitate re-filling
   * our text:
   *
   *     1. text changed.
   *     2. justification changed either from to to GTK_JUSTIFY_FILL.
   *     3. font changed.
   *
   * These have been detected elsewhere, and label->words will be zero,
   * if one of the above has occured.
   *
   * Additionally, though, if GTK_JUSTIFY_FILL, we need to re-fill if:
   *
   *     4. gtk_widget_set_usize has changed the requested width.
   *     5. gtk_misc_set_padding has changed xpad.
   *     6.  maybe others?...
   *
   * Too much of a pain to detect all these case, so always re-fill.  I
   * don't think it's really that slow.
   */

  rwidth = label->misc.xpad * 2;
  rheight = label->misc.ypad * 2;

  if (!label->layout)
    {
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */

      label->layout = gtk_widget_create_pango_layout (widget, label->text);

      if (label->attrs)
	pango_layout_set_attributes (label->layout, label->attrs);
      
      switch (label->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_FILL:
	  /* FIXME: This just doesn't work to do this */
	  align = PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (label->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (label->layout, align);
    }

  if (label->wrap)
    {
      GtkWidgetAuxInfo *aux_info;
      gint longest_paragraph;
      gint width, height;
      gint real_width;

      aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
      if (aux_info && aux_info->width > 0)
	{
	  pango_layout_set_width (label->layout, aux_info->width * PANGO_SCALE);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);

	  rwidth += aux_info->width;
	  rheight += PANGO_PIXELS (logical_rect.height);
	}
      else
	{
	  pango_layout_set_width (label->layout, -1);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);
      
	  width = logical_rect.width;
	  height = logical_rect.height;
	  
	  /* Try to guess a reasonable maximum width
	   */
	  longest_paragraph = width;

	  width = MIN (width,
		       PANGO_SCALE * gdk_string_width (GTK_WIDGET (label)->style->font,
						"This long string gives a good enough length for any line to have."));
	  width = MIN (width,
		       PANGO_SCALE * (gdk_screen_width () + 1) / 2);

	  pango_layout_set_width (label->layout, width);
	  pango_layout_get_extents (label->layout, NULL, &logical_rect);
	  real_width = logical_rect.width;
	  height = logical_rect.height;
	  
	  /* Unfortunately, the above may leave us with a very unbalanced looking paragraph,
	   * so we try short search for a narrower width that leaves us with the same height
	   */
	  if (longest_paragraph > 0)
	    {
	      gint nlines, perfect_width;

	      nlines = pango_layout_get_line_count (label->layout);
	      perfect_width = (longest_paragraph + nlines - 1) / nlines;
	      
	      if (perfect_width < width)
		{
		  pango_layout_set_width (label->layout, perfect_width);
		  pango_layout_get_extents (label->layout, NULL, &logical_rect);
		  
		  if (logical_rect.height <= height)
		    {
		      width = perfect_width;
		      real_width = logical_rect.width;
		      height = logical_rect.height;
		    }
		  else
		    {
		      gint mid_width = (perfect_width + width) / 2;

		      if (mid_width > perfect_width)
			{
			  pango_layout_set_width (label->layout, mid_width);
			  pango_layout_get_extents (label->layout, NULL, &logical_rect);

			  if (logical_rect.height <= height)
			    {
			      width = mid_width;
			      real_width = logical_rect.width;
			      height = logical_rect.height;
			    }
			}
		    }
		}
	    }
	  pango_layout_set_width (label->layout, width);

	  rwidth += PANGO_PIXELS (real_width);
	  rheight += PANGO_PIXELS (height);
	}
    }
  else				/* !label->wrap */
    {
      pango_layout_set_width (label->layout, -1);
      pango_layout_get_extents (label->layout, NULL, &logical_rect);

      rwidth += PANGO_PIXELS (logical_rect.width);
      rheight += PANGO_PIXELS (logical_rect.height);
    }

  if (widthp)
    *widthp = rwidth;

  if (heightp)
    *heightp = rheight;
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  gint width, height;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  label = GTK_LABEL (widget);

  gtk_label_ensure_layout (label, &width, &height);

  requisition->width = width;
  requisition->height = height;
}

static void
gtk_label_size_allocate (GtkWidget     *widget,
                         GtkAllocation *allocation)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

  if (label->select_info && label->select_info->window)
    {
      gdk_window_move_resize (label->select_info->window,
                              allocation->x,
                              allocation->y,
                              allocation->width,
                              allocation->height);
    }
}

static void 
gtk_label_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  
  label = GTK_LABEL (widget);

  /* We have to clear the layout, fonts etc. may have changed */
  gtk_label_clear_layout (label);
}

static void 
gtk_label_direction_changed (GtkWidget        *widget,
			     GtkTextDirection previous_dir)
{
  GtkLabel *label = GTK_LABEL (widget);

  if (label->layout)
    pango_layout_context_changed (label->layout);

  GTK_WIDGET_CLASS (parent_class)->direction_changed (widget, previous_dir);
}

#if 0
static void
gtk_label_paint_word (GtkLabel     *label,
		      gint          x,
		      gint          y,
		      GtkLabelWord *word,
		      GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkLabelULine *uline;
  gchar *tmp_str;
  
  tmp_str = gdk_wcstombs (word->beginning);
  if (tmp_str)
    {
      gtk_paint_string (widget->style, widget->window, widget->state,
			area, widget, "label", 
			x + word->x,
			y + word->y,
			tmp_str);
      g_free (tmp_str);
    }
  
  for (uline = word->uline; uline; uline = uline->next)
    gtk_paint_hline (widget->style, widget->window, 
		     widget->state, area,
		     widget, "label", 
		     x + uline->x1, x + uline->x2, y + uline->y);
}
#endif

static void
get_layout_location (GtkLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkMisc *misc;
  GtkWidget *widget;
  gfloat xalign;
  gint x, y;
  
  misc = GTK_MISC (label);
  widget = GTK_WIDGET (label);
  
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    xalign = misc->xalign;
  else
    xalign = 1.0 - misc->xalign;
  
  x = floor (widget->allocation.x + (gint)misc->xpad
             + ((widget->allocation.width - widget->requisition.width) * xalign)
             + 0.5);
  
  y = floor (widget->allocation.y + (gint)misc->ypad 
             + ((widget->allocation.height - widget->requisition.height) * misc->yalign)
             + 0.5);
  

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  gint x, y;
  
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  label = GTK_LABEL (widget);

  gtk_label_ensure_layout (label, NULL, NULL);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      label->text && (*label->text != '\0'))
    {
      get_layout_location (label, &x, &y);
      
      gtk_paint_layout (widget->style,
                        widget->window,
                        GTK_WIDGET_STATE (widget),
                        &event->area,
                        widget,
                        "label",
                        x, y,
                        label->layout);
      
      if (label->select_info &&
          (label->select_info->selection_anchor !=
           label->select_info->selection_end))
        {
          gint range[2];
          GdkRegion *clip;
          
          range[0] = label->select_info->selection_anchor;
          range[1] = label->select_info->selection_end;

          if (range[0] > range[1])
            {
              gint tmp = range[0];
              range[0] = range[1];
              range[1] = tmp;
            }

          clip = gdk_pango_layout_get_clip_region (label->layout,
                                                   x, y,
                                                   range,
                                                   1);

          /* FIXME should use gtk_paint, but it can't use a clip
           * region
           */

          gdk_gc_set_clip_region (widget->style->white_gc, clip);
          
          gdk_draw_layout_with_colors (widget->window,
                                       widget->style->white_gc,
                                       x, y,
                                       label->layout,
                                       &widget->style->fg[GTK_STATE_SELECTED],
                                       &widget->style->bg[GTK_STATE_SELECTED]);

          gdk_gc_set_clip_region (widget->style->white_gc, NULL);
          gdk_region_destroy (clip);
        }
    }

  return TRUE;
}

void
gtk_label_set_uline_text_internal (GtkLabel    *label,
				   const gchar *str)
{
  guint accel_key = GDK_VoidSymbol;

  gchar *new_str;
  gchar *pattern;
  const gchar *src;
  gchar *dest, *pattern_dest;
  gboolean underscore;
      
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  /* Convert text to wide characters */

  new_str = g_new (gchar, strlen (str) + 1);
  pattern = g_new (gchar, g_utf8_strlen (str, -1) + 1);
  
  underscore = FALSE;

  if (str == NULL)
    str = "";
  
  src = str;
  dest = new_str;
  pattern_dest = pattern;
  
  while (*src)
    {
      gunichar c;
      gchar *next_src;

      c = g_utf8_get_char (src);
      if (c == (gunichar)-1)
	{
	  g_warning ("Invalid input string");
	  g_free (new_str);
	  g_free (pattern);
	  return;
	}
      next_src = g_utf8_next_char (src);
      
      if (underscore)
	{
	  if (c == '_')
	    *pattern_dest++ = ' ';
	  else
	    {
	      *pattern_dest++ = '_';
	      if (accel_key == GDK_VoidSymbol)
		accel_key = gdk_keyval_to_lower (c);
	    }

	  while (src < next_src)
	    *dest++ = *src++;
	  
	  underscore = FALSE;
	}
      else
	{
	  if (c == '_')
	    {
	      underscore = TRUE;
	      src = next_src;
	    }
	  else
	    {
	      while (src < next_src)
		*dest++ = *src++;
	  
	      *pattern_dest++ = ' ';
	    }
	}
    }
  *dest = 0;
  *pattern_dest = 0;
  
  gtk_label_set_text_internal (label, new_str);
  gtk_label_set_pattern_internal (label, pattern);
  
  g_free (pattern);

  label->accel_keyval = accel_key;
}

guint      
gtk_label_parse_uline (GtkLabel    *label,
		       const gchar *str)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);
  g_return_val_if_fail (str != NULL, GDK_VoidSymbol);

  gtk_label_set_label_internal (label, g_strdup (str ? str : ""));
  gtk_label_set_use_markup_internal (label, FALSE);
  gtk_label_set_use_underline_internal (label, TRUE);
  
  gtk_label_recalculate (label);
  return label->accel_keyval;
}


static void
gtk_label_realize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

  if (label->select_info)
    gtk_label_create_window (label);
}

static void
gtk_label_unrealize (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    gtk_label_destroy_window (label);
  
  (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_label_map (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  (* GTK_WIDGET_CLASS (parent_class)->map) (widget);
  
  if (label->select_info)
    gdk_window_show (label->select_info->window);
}

static void
gtk_label_unmap (GtkWidget *widget)
{
  GtkLabel *label;

  label = GTK_LABEL (widget);

  if (label->select_info)
    gdk_window_hide (label->select_info->window);
  
  (* GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}

static void
window_to_layout_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += widget->allocation.x; /* go to widget->window */
      *x -= lx;                   /* go to layout */
    }

  if (y)
    {
      *y += widget->allocation.y; /* go to widget->window */
      *y -= ly;                   /* go to layout */
    }
}

static void
layout_to_window_coords (GtkLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;
  GtkWidget *widget;

  widget = GTK_WIDGET (label);
  
  /* get layout location in widget->window coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    {
      *x += lx;                   /* go to widget->window */
      *x -= widget->allocation.x; /* go to selection window */
    }

  if (y)
    {
      *y += ly;                   /* go to widget->window */
      *y -= widget->allocation.y; /* go to selection window */
    }
}

static void
get_layout_index (GtkLabel *label,
                  gint      x,
                  gint      y,
                  gint     *index)
{
  gint trailing = 0;
  const gchar *cluster;
  const gchar *cluster_end;

  *index = 0;
  
  gtk_label_ensure_layout (label, NULL, NULL);
  
  window_to_layout_coords (label, &x, &y);

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;
  
  pango_layout_xy_to_index (label->layout,
                            x, y,
                            index, &trailing);

  
  cluster = label->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);
}

static gint
gtk_label_button_press (GtkWidget      *widget,
                        GdkEventButton *event)
{
  GtkLabel *label;
  gint index = 0;
  
  label = GTK_LABEL (widget);

  if (label->select_info == NULL)
    return FALSE;

  if (event->button != 1)
    return FALSE;

  get_layout_index (label, event->x, event->y, &index);
  
  if ((label->select_info->selection_anchor !=
       label->select_info->selection_end) &&
      (event->state & GDK_SHIFT_MASK))
    {
      /* extend (same as motion) */
      if (index < label->select_info->selection_end)
        gtk_label_select_region_index (label,
                                       index,
                                       label->select_info->selection_end);
      else
        gtk_label_select_region_index (label,
                                       label->select_info->selection_anchor,
                                       index);

      /* ensure the anchor is opposite index */
      if (index == label->select_info->selection_anchor)
        {
          gint tmp = label->select_info->selection_end;
          label->select_info->selection_end = label->select_info->selection_anchor;
          label->select_info->selection_anchor = tmp;
        }
    }
  else
    {
      /* start a replacement */
      gtk_label_select_region_index (label, index, index);
    }
  
  return TRUE;
}

static gint
gtk_label_button_release (GtkWidget      *widget,
                          GdkEventButton *event)

{
  GtkLabel *label;

  label = GTK_LABEL (widget);
  
  if (label->select_info == NULL)
    return FALSE;
  
  if (event->button != 1)
    return FALSE;
  
  /* The goal here is to return TRUE iff we ate the
   * button press to start selecting.
   */
  
  return TRUE;
}

static gint
gtk_label_motion (GtkWidget      *widget,
                  GdkEventMotion *event)
{
  GtkLabel *label;
  gint index;
  gint x, y;
  
  label = GTK_LABEL (widget);
  
  if (label->select_info == NULL)
    return FALSE;  

  if ((event->state & GDK_BUTTON1_MASK) == 0)
    return FALSE;

  gdk_window_get_pointer (label->select_info->window,
                          &x, &y, NULL);
  
  get_layout_index (label, x, y, &index);

  gtk_label_select_region_index (label,
                                 label->select_info->selection_anchor,
                                 index);
  
  return TRUE;
}

static void
gtk_label_create_window (GtkLabel *label)
{
  GtkWidget *widget;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_assert (label->select_info);
  g_assert (GTK_WIDGET_REALIZED (label));
  
  if (label->select_info->window)
    return;
  
  widget = GTK_WIDGET (label);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_BUTTON_PRESS_MASK        |
    GDK_BUTTON_RELEASE_MASK      |
    GDK_BUTTON_MOTION_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  label->select_info->window = gdk_window_new (widget->window,
                                               &attributes, attributes_mask);
  gdk_window_set_user_data (label->select_info->window, widget);  
}

static void
gtk_label_destroy_window (GtkLabel *label)
{
  g_assert (label->select_info);

  if (label->select_info->window == NULL)
    return;
  
  gdk_window_set_user_data (label->select_info->window, NULL);
  gdk_window_destroy (label->select_info->window);
  label->select_info->window = NULL;
}

void
gtk_label_set_selectable (GtkLabel *label,
                          gboolean  setting)
{
  gboolean old_setting;
  
  g_return_if_fail (GTK_IS_LABEL (label));
  
  setting = setting != FALSE;
  old_setting = label->select_info != NULL;
  
  if (setting)
    {
      if (label->select_info == NULL)
        {
          label->select_info = g_new (GtkLabelSelectionInfo, 1);

          label->select_info->window = NULL;
          label->select_info->selection_anchor = 0;
          label->select_info->selection_end = 0;

          if (GTK_WIDGET_REALIZED (label))
            gtk_label_create_window (label);

          if (GTK_WIDGET_MAPPED (label))
            gdk_window_show (label->select_info->window);
        }
    }
  else
    {
      if (label->select_info)
        {
          if (label->select_info->window)
            gtk_label_destroy_window (label);

          g_free (label->select_info);

          label->select_info = NULL;
        }
    }
  if (setting != old_setting)
    {
       g_object_notify (G_OBJECT (label), "selectable");
       gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

gboolean
gtk_label_get_selectable (GtkLabel *label)
{
  g_return_val_if_fail (GTK_IS_LABEL (label), FALSE);

  return label->select_info != NULL;
}

static void
get_text_callback (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          user_data_or_owner)
{
  GtkLabel *label;
  gchar *str;
  
  label = GTK_LABEL (user_data_or_owner);
  
  if ((label->select_info->selection_anchor !=
       label->select_info->selection_end) &&
      label->text)
    {
      gint start, end;
      
      start = MIN (label->select_info->selection_anchor,
                   label->select_info->selection_end);
      end = MAX (label->select_info->selection_anchor,
                 label->select_info->selection_end);
      
      str = g_strndup (label->text + start,
                       end - start);
      
      gtk_selection_data_set_text (selection_data, 
                                   str);

      g_free (str);
    }
}

static void
clear_text_callback (GtkClipboard     *clipboard,
                     gpointer          user_data_or_owner)
{
  GtkLabel *label;

  label = GTK_LABEL (user_data_or_owner);

  if (label->select_info)
    {
      label->select_info->selection_anchor = 0;
      label->select_info->selection_end = 0;
      
      gtk_label_clear_layout (label);
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

static void
gtk_label_select_region_index (GtkLabel *label,
                               gint      anchor_index,
                               gint      end_index)
{
  static const GtkTargetEntry targets[] = {
    { "STRING", 0, 0 },
    { "TEXT",   0, 0 }, 
    { "COMPOUND_TEXT", 0, 0 },
    { "UTF8_STRING", 0, 0 }
  };
  
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->select_info)
    {
      GtkClipboard *clipboard;

      label->select_info->selection_anchor = anchor_index;
      label->select_info->selection_end = end_index;

      clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);      

      gtk_clipboard_set_with_owner (clipboard,
                                    targets,
                                    G_N_ELEMENTS (targets),
                                    get_text_callback,
                                    clear_text_callback,
                                    G_OBJECT (label));

      gtk_label_clear_layout (label);
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }
}

void
gtk_label_select_region  (GtkLabel *label,
                          gint      start_offset,
                          gint      end_offset)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->text && label->select_info)
    {
      GtkClipboard *clipboard;

      if (start_offset < 0)
        start_offset = 0;

      if (end_offset < 0)
        end_offset = g_utf8_strlen (label->text, -1);
      
      gtk_label_select_region_index (label,
                                     g_utf8_offset_to_pointer (label->text, start_offset) - label->text,
                                     g_utf8_offset_to_pointer (label->text, end_offset) - label->text);
    }
}

