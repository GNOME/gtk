/* gtkcellrenderertext.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include <stdlib.h>
#include "gtkcellrenderertext.h"
#include "gtkintl.h"

static void gtk_cell_renderer_text_init       (GtkCellRendererText      *celltext);
static void gtk_cell_renderer_text_class_init (GtkCellRendererTextClass *class);
static void gtk_cell_renderer_text_finalize   (GObject                  *object);

static void gtk_cell_renderer_text_get_property  (GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec,
						  const gchar              *trailer);
static void gtk_cell_renderer_text_set_property  (GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec,
						  const gchar              *trailer);
static void gtk_cell_renderer_text_get_size   (GtkCellRenderer          *cell,
					       GtkWidget                *widget,
					       gint                     *width,
					       gint                     *height);
static void gtk_cell_renderer_text_render     (GtkCellRenderer          *cell,
					       GdkWindow                *window,
					       GtkWidget                *widget,
					       GdkRectangle             *background_area,
					       GdkRectangle             *cell_area,
					       GdkRectangle             *expose_area,
					       guint                     flags);



enum {
  PROP_0,

  PROP_TEXT,
  
  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND_GDK,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_EDITABLE,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_RISE,
  
  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_EDITABLE_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET
};

static gpointer parent_class;

GtkType
gtk_cell_renderer_text_get_type (void)
{
  static GtkType cell_text_type = 0;

  if (!cell_text_type)
    {
      static const GTypeInfo cell_text_info =
      {
        sizeof (GtkCellRendererTextClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_cell_renderer_text_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkCellRendererText),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_cell_renderer_text_init,
      };

      cell_text_type = g_type_register_static (GTK_TYPE_CELL_RENDERER, "GtkCellRendererText", &cell_text_info, 0);
    }

  return cell_text_type;
}

static void
gtk_cell_renderer_text_init (GtkCellRendererText *celltext)
{
  GTK_CELL_RENDERER (celltext)->xalign = 0.0;
  GTK_CELL_RENDERER (celltext)->yalign = 0.5;
  GTK_CELL_RENDERER (celltext)->xpad = 2;
  GTK_CELL_RENDERER (celltext)->ypad = 2;
}

static void
gtk_cell_renderer_text_class_init (GtkCellRendererTextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  object_class->finalize = gtk_cell_renderer_text_finalize;
  
  object_class->get_property = gtk_cell_renderer_text_get_property;
  object_class->set_property = gtk_cell_renderer_text_set_property;

  cell_class->get_size = gtk_cell_renderer_text_get_size;
  cell_class->render = gtk_cell_renderer_text_render;

  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        _("Text"),
                                                        _("Text to render"),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        _("Background color name"),
                                                        _("Background color as a string"),
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background_gdk",
                                                       _("Background color"),
                                                       _("Background color as a GdkColor"),
                                                       GTK_TYPE_GDK_COLOR,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));  

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_string ("foreground",
                                                        _("Foreground color name"),
                                                        _("Foreground color as a string"),
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_GDK,
                                   g_param_spec_boxed ("foreground_gdk",
                                                       _("Foreground color"),
                                                       _("Foreground color as a GdkColor"),
                                                       GTK_TYPE_GDK_COLOR,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         _("Editable"),
                                                         _("Whether the text can be modified by the user"),
                                                         TRUE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FONT,
                                   g_param_spec_string ("font",
                                                        _("Font"),
                                                        _("Font description as a string"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FONT_DESC,
                                   g_param_spec_boxed ("font_desc",
                                                       _("Font"),
                                                       _("Font description as a PangoFontDescription struct"),
                                                       GTK_TYPE_PANGO_FONT_DESCRIPTION,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

  
  g_object_class_install_property (object_class,
                                   PROP_FAMILY,
                                   g_param_spec_string ("family",
                                                        _("Font family"),
                                                        _("Name of the font family, e.g. Sans, Helvetica, Times, Monospace"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("style",
                                                      _("Font style"),
                                                      _("Font style"),
                                                      PANGO_TYPE_STYLE,
                                                      PANGO_STYLE_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_VARIANT,
                                   g_param_spec_enum ("variant",
                                                     _("Font variant"),
                                                     _("Font variant"),
                                                      PANGO_TYPE_VARIANT,
                                                      PANGO_VARIANT_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_WEIGHT,
                                   g_param_spec_int ("weight",
                                                     _("Font weight"),
                                                     _("Font weight"),
                                                     0,
                                                     G_MAXINT,
                                                     PANGO_WEIGHT_NORMAL,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));
  

  g_object_class_install_property (object_class,
                                   PROP_STRETCH,
                                   g_param_spec_enum ("stretch",
                                                      _("Font stretch"),
                                                      _("Font stretch"),
                                                      PANGO_TYPE_STRETCH,
                                                      PANGO_STRETCH_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     _("Font size"),
                                                     _("Font size"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_POINTS,
                                   g_param_spec_double ("size_points",
                                                        _("Font points"),
                                                        _("Font size in points"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));  
  
  g_object_class_install_property (object_class,
                                   PROP_RISE,
                                   g_param_spec_int ("rise",
                                                     _("Rise"),
                                                     _("Offset of text above the baseline (below the baseline if rise is negative)"),
                                                     -G_MAXINT,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH,
                                   g_param_spec_boolean ("strikethrough",
                                                         _("Strikethrough"),
                                                         _("Whether to strike through the text"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE,
                                   g_param_spec_enum ("underline",
                                                      _("Underline"),
                                                      _("Style of underline for this text"),
                                                      PANGO_TYPE_UNDERLINE,
                                                      PANGO_UNDERLINE_NONE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

  ADD_SET_PROP ("background_set", PROP_BACKGROUND_SET,
                _("Background set"),
                _("Whether this tag affects the background color"));

  ADD_SET_PROP ("foreground_set", PROP_FOREGROUND_SET,
                _("Foreground set"),
                _("Whether this tag affects the foreground color"));
  
  ADD_SET_PROP ("editable_set", PROP_EDITABLE_SET,
                _("Editability set"),
                _("Whether this tag affects text editability"));

  ADD_SET_PROP ("family_set", PROP_FAMILY_SET,
                _("Font family set"),
                _("Whether this tag affects the font family"));  

  ADD_SET_PROP ("style_set", PROP_STYLE_SET,
                _("Font style set"),
                _("Whether this tag affects the font style"));

  ADD_SET_PROP ("variant_set", PROP_VARIANT_SET,
                _("Font variant set"),
                _("Whether this tag affects the font variant"));

  ADD_SET_PROP ("weight_set", PROP_WEIGHT_SET,
                _("Font weight set"),
                _("Whether this tag affects the font weight"));

  ADD_SET_PROP ("stretch_set", PROP_STRETCH_SET,
                _("Font stretch set"),
                _("Whether this tag affects the font stretch"));

  ADD_SET_PROP ("size_set", PROP_SIZE_SET,
                _("Font size set"),
                _("Whether this tag affects the font size"));

  ADD_SET_PROP ("rise_set", PROP_RISE_SET,
                _("Rise set"),
                _("Whether this tag affects the rise"));

  ADD_SET_PROP ("strikethrough_set", PROP_STRIKETHROUGH_SET,
                _("Strikethrough set"),
                _("Whether this tag affects strikethrough"));

  ADD_SET_PROP ("underline_set", PROP_UNDERLINE_SET,
                _("Underline set"),
                _("Whether this tag affects underlining"));
}

static void
gtk_cell_renderer_text_finalize (GObject *object)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);

  if (celltext->font.family_name)
    g_free (celltext->font.family_name);

  if (celltext->text)
    g_free (celltext->text);
  
  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_cell_renderer_text_get_property (GObject        *object,
				     guint           param_id,
				     GValue         *value,
				     GParamSpec     *pspec,
				     const gchar    *trailer)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, celltext->text);
      break;
      
    case PROP_BACKGROUND_GDK:
      {
        GdkColor color;
        
        color.red = celltext->background.red;
        color.green = celltext->background.green;
        color.blue = celltext->background.blue;
        
        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_FOREGROUND_GDK:
      {
        GdkColor color;
        
        color.red = celltext->foreground.red;
        color.green = celltext->foreground.green;
        color.blue = celltext->foreground.blue;
        
        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_FONT:
      {
        /* FIXME GValue imposes a totally gratuitous string copy
         * here, we could just hand off string ownership
         */
        gchar *str = pango_font_description_to_string (&celltext->font);
        g_value_set_string (value, str);
        g_free (str);
      }
      break;
      
    case PROP_FONT_DESC:
      g_value_set_boxed (value, &celltext->font);
      break;

    case PROP_FAMILY:
      g_value_set_string (value, celltext->font.family_name);
      break;

    case PROP_STYLE:
      g_value_set_enum (value, celltext->font.style);
      break;

    case PROP_VARIANT:
      g_value_set_enum (value, celltext->font.variant);
      break;

    case PROP_WEIGHT:
      g_value_set_int (value, celltext->font.weight);
      break;

    case PROP_STRETCH:
      g_value_set_enum (value, celltext->font.stretch);
      break;

    case PROP_SIZE:
      g_value_set_int (value, celltext->font.size);
      break;

    case PROP_SIZE_POINTS:
      g_value_set_double (value, ((double)celltext->font.size) / (double)PANGO_SCALE);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, celltext->editable);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, celltext->strikethrough);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, celltext->underline_style);
      break;

    case PROP_RISE:
      g_value_set_int (value, celltext->rise);
      break;  

    case PROP_BACKGROUND_SET:
      g_value_set_boolean (value, celltext->background_set);
      break;

    case PROP_FOREGROUND_SET:
      g_value_set_boolean (value, celltext->foreground_set);
      break;

    case PROP_FAMILY_SET:
      g_value_set_boolean (value, celltext->family_set);
      break;

    case PROP_STYLE_SET:
      g_value_set_boolean (value, celltext->style_set);
      break;

    case PROP_VARIANT_SET:
      g_value_set_boolean (value, celltext->variant_set);
      break;

    case PROP_WEIGHT_SET:
      g_value_set_boolean (value, celltext->weight_set);
      break;

    case PROP_STRETCH_SET:
      g_value_set_boolean (value, celltext->stretch_set);
      break;

    case PROP_SIZE_SET:
      g_value_set_boolean (value, celltext->size_set);
      break;

    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, celltext->editable_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, celltext->strikethrough_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, celltext->underline_set);
      break;

    case  PROP_RISE_SET:
      g_value_set_boolean (value, celltext->rise_set);
      break;
      
    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
set_bg_color (GtkCellRendererText *celltext,
              GdkColor            *color)
{
  if (color)
    {
      if (!celltext->background_set)
        {
          celltext->background_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "background_set");
        }
      
      celltext->background.red = color->red;
      celltext->background.green = color->green;
      celltext->background.blue = color->blue;
    }
  else
    {
      if (celltext->background_set)
        {
          celltext->background_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "background_set");
        }
    }
}


static void
set_fg_color (GtkCellRendererText *celltext,
              GdkColor            *color)
{
  if (color)
    {
      if (!celltext->foreground_set)
        {
          celltext->foreground_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "foreground_set");
        }
      
      celltext->foreground.red = color->red;
      celltext->foreground.green = color->green;
      celltext->foreground.blue = color->blue;
    }
  else
    {
      if (celltext->foreground_set)
        {
          celltext->foreground_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "foreground_set");
        }
    }
}

static void
set_font_description (GtkCellRendererText  *celltext,
                      PangoFontDescription *font_desc)
{
  if (font_desc != NULL)
    {
      /* pango_font_description_from_string() will sometimes return
       * a NULL family or -1 size, so handle those cases.
       */
      
      if (font_desc->family_name)
        g_object_set (G_OBJECT (celltext),
                      "family", font_desc->family_name,
                      NULL);
      
      if (font_desc->size >= 0)
        g_object_set (G_OBJECT (celltext),
                      "size", font_desc->size,
                      NULL);
        
      g_object_set (G_OBJECT (celltext),
                    "style", font_desc->style,
                    "variant", font_desc->variant,
                    "weight", font_desc->weight,
                    "stretch", font_desc->stretch,
                    NULL);
    }
  else
    {
      if (celltext->family_set)
        {
          celltext->family_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "family_set");
        }
      if (celltext->style_set)
        {
          celltext->style_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "style_set");
        }
      if (celltext->variant_set)
        {
          celltext->variant_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "variant_set");
        }
      if (celltext->weight_set)
        {
          celltext->weight_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "weight_set");
        }
      if (celltext->stretch_set)
        {
          celltext->stretch_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "stretch_set");
        }
      if (celltext->size_set)
        {
          celltext->size_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "size_set");
        }
    }
}

static void
gtk_cell_renderer_text_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec,
				     const gchar  *trailer)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
    case PROP_TEXT:
      if (celltext->text)
        g_free (celltext->text);
      celltext->text = g_strdup (g_value_get_string (value));
      break;
      
    case PROP_BACKGROUND:
      {
        GdkColor color;

        if (gdk_color_parse (g_value_get_string (value), &color))
          set_bg_color (celltext, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (G_OBJECT (celltext), "background_gdk");
      }
      break;
      
    case PROP_FOREGROUND:
      {
        GdkColor color;

        if (gdk_color_parse (g_value_get_string (value), &color))
          set_bg_color (celltext, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (G_OBJECT (celltext), "foreground_gdk");
      }
      break;

    case PROP_BACKGROUND_GDK:
      set_bg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FOREGROUND_GDK:
      set_fg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const gchar *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (celltext, font_desc);
        
        if (font_desc)
          pango_font_description_free (font_desc);
      }
      break;

    case PROP_FONT_DESC:
      set_font_description (celltext, g_value_get_boxed (value));
      break;

    case PROP_FAMILY:
      if (celltext->font.family_name)
        g_free (celltext->font.family_name);
      celltext->font.family_name = g_strdup (g_value_get_string (value));

      celltext->family_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "family_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_STYLE:
      celltext->font.style = g_value_get_enum (value);

      celltext->style_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "style_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_VARIANT:
      celltext->font.variant = g_value_get_enum (value);

      celltext->variant_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "variant_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_WEIGHT:
      celltext->font.weight = g_value_get_int (value);
      
      celltext->weight_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "weight_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_STRETCH:
      celltext->font.stretch = g_value_get_enum (value);

      celltext->stretch_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "stretch_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_SIZE:
      celltext->font.size = g_value_get_int (value);

      celltext->size_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "size_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_SIZE_POINTS:
      celltext->font.size = g_value_get_double (value) * PANGO_SCALE;

      celltext->size_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "size_set");
      g_object_notify (G_OBJECT (celltext), "font_desc");
      g_object_notify (G_OBJECT (celltext), "font");
      break;

    case PROP_EDITABLE:
      celltext->editable = g_value_get_boolean (value);
      celltext->editable_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "editable_set");
      break;

    case PROP_STRIKETHROUGH:
      celltext->strikethrough = g_value_get_boolean (value);
      celltext->strikethrough_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "strikethrough_set");
      break;

    case PROP_UNDERLINE:
      celltext->underline_style = g_value_get_enum (value);
      celltext->underline_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "underline_set");
      break;

    case PROP_RISE:
      celltext->rise = g_value_get_int (value);
      celltext->rise_set = TRUE;
      g_object_notify (G_OBJECT (celltext), "rise_set");
      break;  

    case PROP_BACKGROUND_SET:
      celltext->background_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
      celltext->foreground_set = g_value_get_boolean (value);
      break;

    case PROP_FAMILY_SET:
      celltext->family_set = g_value_get_boolean (value);
      break;

    case PROP_STYLE_SET:
      celltext->style_set = g_value_get_boolean (value);
      break;

    case PROP_VARIANT_SET:
      celltext->variant_set = g_value_get_boolean (value);
      break;

    case PROP_WEIGHT_SET:
      celltext->weight_set = g_value_get_boolean (value);
      break;

    case PROP_STRETCH_SET:
      celltext->stretch_set = g_value_get_boolean (value);
      break;

    case PROP_SIZE_SET:
      celltext->size_set = g_value_get_boolean (value);
      break;

    case PROP_EDITABLE_SET:
      celltext->editable_set = g_value_get_boolean (value);
      break;

    case PROP_STRIKETHROUGH_SET:
      celltext->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_UNDERLINE_SET:
      celltext->underline_set = g_value_get_boolean (value);
      break;

    case  PROP_RISE_SET:
      celltext->rise_set = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * gtk_cell_renderer_text_new:
 * 
 * Creates a new #GtkCellRendererText. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
gtk_cell_renderer_text_new (void)
{
  return GTK_CELL_RENDERER (g_object_new (gtk_cell_renderer_text_get_type (), NULL));
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  
  pango_attr_list_insert (attr_list, attr);
}

static PangoLayout*
get_layout (GtkCellRendererText *celltext,
            GtkWidget           *widget,
            gboolean             will_render,
            GtkCellRendererState flags)
{
  PangoAttrList *attr_list;
  PangoLayout *layout;
  PangoUnderline uline;
  
  layout = gtk_widget_create_pango_layout (widget, celltext->text);
  
  attr_list = pango_attr_list_new ();

  if (will_render)
    {
      /* Add options that affect appearance but not size */
      
      /* note that background doesn't go here, since it affects
       * background_area not the PangoLayout area
       */
      
      if (celltext->foreground_set)
        {
          PangoColor color;

          color = celltext->foreground;
          
          add_attr (attr_list,
                    pango_attr_foreground_new (color.red, color.green, color.blue));
        }

      if (celltext->strikethrough_set)
        add_attr (attr_list,
                  pango_attr_strikethrough_new (celltext->strikethrough));
    }

  if (celltext->family_set &&
      celltext->font.family_name)
    add_attr (attr_list, pango_attr_family_new (celltext->font.family_name));
  
  if (celltext->style_set)
    add_attr (attr_list, pango_attr_style_new (celltext->font.style));

  if (celltext->variant_set)
    add_attr (attr_list, pango_attr_variant_new (celltext->font.variant));

  if (celltext->weight_set)
    add_attr (attr_list, pango_attr_weight_new (celltext->font.weight));

  if (celltext->stretch_set)
    add_attr (attr_list, pango_attr_stretch_new (celltext->font.stretch));

  if (celltext->size_set &&
      celltext->font.size >= 0)
    add_attr (attr_list, pango_attr_size_new (celltext->font.size));

  if (celltext->underline_set)
    uline = celltext->underline_style;
  else
    uline = PANGO_UNDERLINE_NONE;
  
  if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
    {
      switch (uline)
        {
        case PANGO_UNDERLINE_NONE:
          uline = PANGO_UNDERLINE_SINGLE;
          break;

        case PANGO_UNDERLINE_SINGLE:
          uline = PANGO_UNDERLINE_DOUBLE;
          break;

        default:
          break;
        }
    }

  if (uline != PANGO_UNDERLINE_NONE)
    add_attr (attr_list, pango_attr_underline_new (celltext->underline_style));

  if (celltext->rise_set)
    add_attr (attr_list, pango_attr_rise_new (celltext->rise));
  
  pango_layout_set_attributes (layout, attr_list);
  pango_layout_set_width (layout, -1);

  pango_attr_list_unref (attr_list);
  
  return layout;
}

static void
gtk_cell_renderer_text_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 gint            *width,
				 gint            *height)
{
  GtkCellRendererText *celltext = (GtkCellRendererText *)cell;
  PangoRectangle rect;
  PangoLayout *layout;

  layout = get_layout (celltext, widget, FALSE, 0);
  
  pango_layout_get_pixel_extents (layout, NULL, &rect);

  if (width)
    *width = GTK_CELL_RENDERER (celltext)->xpad * 2 + rect.width;

  if (height)
    *height = GTK_CELL_RENDERER (celltext)->ypad * 2 + rect.height;

  g_object_unref (G_OBJECT (layout));
}

static void
gtk_cell_renderer_text_render (GtkCellRenderer    *cell,
			       GdkWindow          *window,
			       GtkWidget          *widget,
			       GdkRectangle       *background_area,
			       GdkRectangle       *cell_area,
			       GdkRectangle       *expose_area,
			       guint               flags)

{
  GtkCellRendererText *celltext = (GtkCellRendererText *) cell;
  PangoRectangle rect;
  PangoLayout *layout;
  GtkStateType state;
  
  gint real_xoffset;
  gint real_yoffset;

  layout = get_layout (celltext, widget, TRUE, flags);

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  real_xoffset = cell->xalign * (cell_area->width - rect.width - (2 * cell->xpad));
  real_xoffset = MAX (real_xoffset, 0) + cell->xpad;
  real_yoffset = cell->yalign * (cell_area->height - rect.height - (2 * cell->ypad));
  real_yoffset = MAX (real_yoffset, 0) + cell->ypad;

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    state = GTK_STATE_SELECTED;
  else
    state = GTK_STATE_NORMAL;

  if (celltext->background_set && state != GTK_STATE_SELECTED)
    {
      GdkColor color;
      GdkGC *gc;
      
      color.red = celltext->background.red;
      color.green = celltext->background.green;
      color.blue = celltext->background.blue;

      gc = gdk_gc_new (window);

      gdk_gc_set_rgb_fg_color (gc, &color);
      
      gdk_draw_rectangle (window,
                          gc,
                          TRUE,
                          background_area->x,
                          background_area->y,
                          background_area->width,
                          background_area->height);

      g_object_unref (G_OBJECT (gc));
    }
  
  gtk_paint_layout (widget->style,
                    window,
                    state,
                    cell_area,
                    widget,
                    "cellrenderertext",
                    cell_area->x + real_xoffset,
                    cell_area->y + real_yoffset,
                    layout);

  g_object_unref (G_OBJECT (layout));
}
