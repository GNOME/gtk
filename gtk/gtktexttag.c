/* gtktexttag.c - text tag object
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 *
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 *
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 *
 */

#include "gtkmain.h"
#include "gtktexttag.h"
#include "gtktexttypes.h"
#include "gtktexttagtable.h"
#include "gtksignal.h"
#include "gtkmain.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"

#include <stdlib.h>
#include <string.h>

enum {
  EVENT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  /* Construct args */
  PROP_NAME,

  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND_GDK,
  PROP_BACKGROUND_STIPPLE,
  PROP_FOREGROUND_STIPPLE,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_PIXELS_ABOVE_LINES,
  PROP_PIXELS_BELOW_LINES,
  PROP_PIXELS_INSIDE_WRAP,
  PROP_EDITABLE,
  PROP_WRAP_MODE,
  PROP_JUSTIFICATION,
  PROP_DIRECTION,
  PROP_LEFT_MARGIN,
  PROP_INDENT,
  PROP_STRIKETHROUGH,
  PROP_RIGHT_MARGIN,
  PROP_UNDERLINE,
  PROP_RISE,
  PROP_BG_FULL_HEIGHT,
  PROP_LANGUAGE,
  PROP_TABS,
  PROP_INVISIBLE,
  
  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_BACKGROUND_GDK_SET,
  PROP_FOREGROUND_GDK_SET,
  PROP_BACKGROUND_STIPPLE_SET,
  PROP_FOREGROUND_STIPPLE_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_PIXELS_ABOVE_LINES_SET,
  PROP_PIXELS_BELOW_LINES_SET,
  PROP_PIXELS_INSIDE_WRAP_SET,
  PROP_EDITABLE_SET,
  PROP_WRAP_MODE_SET,
  PROP_JUSTIFICATION_SET,
  PROP_LEFT_MARGIN_SET,
  PROP_INDENT_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_RIGHT_MARGIN_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET,
  PROP_BG_FULL_HEIGHT_SET,
  PROP_LANGUAGE_SET,
  PROP_TABS_SET,
  PROP_INVISIBLE_SET,

  LAST_ARG
};
static void gtk_text_tag_init         (GtkTextTag      *text_tag);
static void gtk_text_tag_class_init   (GtkTextTagClass *klass);
static void gtk_text_tag_finalize     (GObject         *object);
static void gtk_text_tag_set_property (GObject         *object,
                                       guint            prop_id,
                                       const GValue    *value,
                                       GParamSpec      *pspec,
                                       const gchar     *trailer);
static void gtk_text_tag_get_property (GObject         *object,
                                       guint            prop_id,
                                       GValue          *value,
                                       GParamSpec      *pspec,
                                       const gchar     *trailer);

static GObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GType
gtk_text_tag_get_type (void)
{
  static GType our_type = 0;

  if (our_type == 0)
    {
      static const GTypeInfo our_info =
      {
        sizeof (GtkTextTagClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_text_tag_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GtkTextTag),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_text_tag_init
      };

      our_type = g_type_register_static (G_TYPE_OBJECT,
                                         "GtkTextTag",
                                         &our_info,
                                         0);
    }

  return our_type;
}

static void
gtk_text_tag_class_init (GtkTextTagClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->set_property = gtk_text_tag_set_property;
  object_class->get_property = gtk_text_tag_get_property;
  
  object_class->finalize = gtk_text_tag_finalize;

  /* Construct */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("Tag name"),
                                                        _("Name used to refer to the text tag"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /* Style args */

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
                                   PROP_BG_FULL_HEIGHT,
                                   g_param_spec_boolean ("background_full_height",
                                                         _("Background full height"),
                                                         _("Whether the background color fills the entire line height or only the height of the tagged characters"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_STIPPLE,
                                   g_param_spec_object ("background_stipple",
                                                        _("Background stipple mask"),
                                                        _("Bitmap to use as a mask when drawing the text background"),
                                                        GDK_TYPE_PIXMAP,
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
                                   PROP_FOREGROUND_STIPPLE,
                                   g_param_spec_object ("foreground_stipple",
                                                        _("Foreground stipple mask"),
                                                        _("Bitmap to use as a mask when drawing the text foreground"),
                                                        GDK_TYPE_PIXMAP,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));  
  
  g_object_class_install_property (object_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      _("Text direction"),
                                                      _("Text direction, e.g. right-to-left or left-to-right"),
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_LTR,
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
                                   PROP_JUSTIFICATION,
                                   g_param_spec_enum ("justification",
                                                      _("Justification"),
                                                      _("Left, right, or center justification"),
                                                      GTK_TYPE_JUSTIFICATION,
                                                      GTK_JUSTIFY_LEFT,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_LANGUAGE,
                                   g_param_spec_string ("language",
                                                        _("Language"),
                                                        _("Language engine code to use for rendering the text"),
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));  

  g_object_class_install_property (object_class,
                                   PROP_LEFT_MARGIN,
                                   g_param_spec_int ("left_margin",
                                                     _("Left margin"),
                                                     _("Width of the left margin in pixels"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_RIGHT_MARGIN,
                                   g_param_spec_int ("right_margin",
                                                     _("Right margin"),
                                                     _("Width of the right margin in pixels"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  
  g_object_class_install_property (object_class,
                                   PROP_INDENT,
                                   g_param_spec_int ("indent",
                                                     _("Indent"),
                                                     _("Amount to indent the paragraph, in pixels"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
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
                                   PROP_PIXELS_ABOVE_LINES,
                                   g_param_spec_int ("pixels_above_lines",
                                                     _("Pixels above lines"),
                                                     _("Pixels of blank space above paragraphs"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_PIXELS_BELOW_LINES,
                                   g_param_spec_int ("pixels_below_lines",
                                                     _("Pixels below lines"),
                                                     _("Pixels of blank space below paragraphs"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_PIXELS_INSIDE_WRAP,
                                   g_param_spec_int ("pixels_inside_wrap",
                                                     _("Pixels inside wrap"),
                                                     _("Pixels of blank space between wrapped lines in a paragraph"),
                                                     0,
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
  
  g_object_class_install_property (object_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap_mode",
                                                     _("Wrap mode"),
                                                     _("Whether to wrap lines never, at word boundaries, or at character boundaries"),
                                                      GTK_TYPE_WRAP_MODE,
                                                      GTK_WRAPMODE_NONE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  

  g_object_class_install_property (object_class,
                                   PROP_TABS,
                                   g_param_spec_boxed ("tabs",
                                                       _("Tabs"),
                                                       _("Custom tabs for this text"),
                                                       GTK_TYPE_PANGO_TAB_ARRAY,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_INVISIBLE,
                                   g_param_spec_boolean ("invisible",
                                                         _("Invisible"),
                                                         _("Whether this text is hidden"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

  ADD_SET_PROP ("background_set", PROP_BACKGROUND_SET,
                _("Background set"),
                _("Whether this tag affects the background color"));
  
  ADD_SET_PROP ("background_full_height_set", PROP_BG_FULL_HEIGHT_SET,
                _("Background full height set"),
                _("Whether this tag affects background height"));

  ADD_SET_PROP ("background_gdk_set", PROP_BACKGROUND_GDK_SET,
                _("Background set"),
                _("Whether this tag affects the background color"));

  ADD_SET_PROP ("background_stipple_set", PROP_BACKGROUND_STIPPLE_SET,
                _("Background stipple set"),
                _("Whether this tag affects the background stipple"));  

  ADD_SET_PROP ("foreground_set", PROP_FOREGROUND_SET,
                _("Foreground set"),
                _("Whether this tag affects the foreground color"));

  ADD_SET_PROP ("foreground_gdk_set", PROP_FOREGROUND_GDK_SET,
                _("Foreground set"),
                _("Whether this tag affects the foreground color"));

  ADD_SET_PROP ("foreground_stipple_set", PROP_FOREGROUND_STIPPLE_SET,
                _("Foreground stipple set"),
                _("Whether this tag affects the foreground stipple"));
  
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

  ADD_SET_PROP ("justification_set", PROP_JUSTIFICATION_SET,
                _("Justification set"),
                _("Whether this tag affects paragraph justification"));
  
  ADD_SET_PROP ("language_set", PROP_LANGUAGE_SET,
                _("Language set"),
                _("Whether this tag affects the language the text is rendered as"));

  ADD_SET_PROP ("left_margin_set", PROP_LEFT_MARGIN_SET,
                _("Left margin set"),
                _("Whether this tag affects the left margin"));

  ADD_SET_PROP ("indent_set", PROP_INDENT_SET,
                _("Indent set"),
                _("Whether this tag affects indentation"));

  ADD_SET_PROP ("rise_set", PROP_RISE_SET,
                _("Rise set"),
                _("Whether this tag affects the rise"));

  ADD_SET_PROP ("pixels_above_lines_set", PROP_PIXELS_ABOVE_LINES_SET,
                _("Pixels above lines set"),
                _("Whether this tag affects the number of pixels above lines"));

  ADD_SET_PROP ("pixels_below_lines_set", PROP_PIXELS_BELOW_LINES_SET,
                _("Pixels below lines set"),
                _("Whether this tag affects the number of pixels above lines"));

  ADD_SET_PROP ("pixels_inside_wrap_set", PROP_PIXELS_INSIDE_WRAP_SET,
                _("Pixels inside wrap set"),
                _("Whether this tag affects the number of pixels between wrapped lines"));

  ADD_SET_PROP ("strikethrough_set", PROP_STRIKETHROUGH_SET,
                _("Strikethrough set"),
                _("Whether this tag affects strikethrough"));
  
  ADD_SET_PROP ("right_margin_set", PROP_RIGHT_MARGIN_SET,
                _("Right margin set"),
                _("Whether this tag affects the right margin"));

  ADD_SET_PROP ("underline_set", PROP_UNDERLINE_SET,
                _("Underline set"),
                _("Whether this tag affects underlining"));

  ADD_SET_PROP ("wrap_mode_set", PROP_WRAP_MODE_SET,
                _("Wrap mode set"),
                _("Whether this tag affects line wrap mode"));

  ADD_SET_PROP ("tabs_set", PROP_TABS_SET,
                _("Tabs set"),
                _("Whether this tag affects tabs"));

  ADD_SET_PROP ("invisible_set", PROP_INVISIBLE_SET,
                _("Invisible set"),
                _("Whether this tag affects text visibility"));

  signals[EVENT] =
    g_signal_newc ("event",
                   G_TYPE_FROM_CLASS (object_class),
                   G_SIGNAL_RUN_LAST,
                   GTK_SIGNAL_OFFSET (GtkTextTagClass, event),
                   NULL,
                   gtk_marshal_INT__OBJECT_BOXED_BOXED,
                   GTK_TYPE_INT,
                   3,
                   G_TYPE_OBJECT,
                   GTK_TYPE_GDK_EVENT,
                   GTK_TYPE_TEXT_ITER);
}

void
gtk_text_tag_init (GtkTextTag *text_tag)
{
  /* 0 is basically a fine way to initialize everything in the
     entire struct */
  
  text_tag->values = gtk_text_attributes_new ();
}

/**
 * gtk_text_tag_new:
 * @name: tag name, or %NULL
 * 
 * Creates a #GtkTextTag. Configure the tag using object arguments,
 * i.e. using g_object_set().
 * 
 * Return value: a new #GtkTextTag
 **/
GtkTextTag*
gtk_text_tag_new (const gchar *name)
{
  GtkTextTag *tag;

  tag = GTK_TEXT_TAG (g_object_new (gtk_text_tag_get_type (),
                                    "name", name,
                                    NULL));

  return tag;
}

static void
gtk_text_tag_finalize (GObject *object)
{
  GtkTextTag *text_tag;

  text_tag = GTK_TEXT_TAG (object);

  g_assert (!text_tag->values->realized);

  if (text_tag->table)
    gtk_text_tag_table_remove (text_tag->table, text_tag);

  g_assert (text_tag->table == NULL);

  gtk_text_attributes_unref (text_tag->values);
  text_tag->values = NULL;
  
  g_free (text_tag->name);
  text_tag->name = NULL;

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
set_bg_color (GtkTextTag *tag, GdkColor *color)
{
  if (color)
    {
      if (!tag->bg_color_set)
        {
          tag->bg_color_set = TRUE;
          g_object_notify (G_OBJECT (tag), "background_set");
        }
      
      tag->values->appearance.bg_color = *color;
    }
  else
    {
      if (tag->bg_color_set)
        {
          tag->bg_color_set = FALSE;
          g_object_notify (G_OBJECT (tag), "background_set");
        }
    }
}

static void
set_fg_color (GtkTextTag *tag, GdkColor *color)
{
  if (color)
    {
      if (!tag->fg_color_set)
        {
          tag->fg_color_set = TRUE;
          g_object_notify (G_OBJECT (tag), "foreground_set");
        }
      tag->values->appearance.fg_color = *color;
    }
  else
    {
      if (tag->fg_color_set)
        {
          tag->fg_color_set = FALSE;
          g_object_notify (G_OBJECT (tag), "foreground_set");
        }
    }
}

static void
set_font_description (GtkTextTag           *text_tag,
                      PangoFontDescription *font_desc)
{
  if (font_desc != NULL)
    {
      /* pango_font_description_from_string() will sometimes return
       * a NULL family or -1 size, so handle those cases.
       */
      
      if (font_desc->family_name)
        g_object_set (G_OBJECT (text_tag),
                      "family", font_desc->family_name,
                      NULL);
      
      if (font_desc->size >= 0)
        g_object_set (G_OBJECT (text_tag),
                      "size", font_desc->size,
                      NULL);
        
      g_object_set (G_OBJECT (text_tag),
                    "style", font_desc->style,
                    "variant", font_desc->variant,
                    "weight", font_desc->weight,
                    "stretch", font_desc->stretch,
                    NULL);
    }
  else
    {
      if (text_tag->family_set)
        {
          text_tag->family_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "family_set");
        }
      if (text_tag->style_set)
        {
          text_tag->style_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "style_set");
        }
      if (text_tag->variant_set)
        {
          text_tag->variant_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "variant_set");
        }
      if (text_tag->weight_set)
        {
          text_tag->weight_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "weight_set");
        }
      if (text_tag->stretch_set)
        {
          text_tag->stretch_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "stretch_set");
        }
      if (text_tag->size_set)
        {
          text_tag->size_set = FALSE;
          g_object_notify (G_OBJECT (text_tag), "size_set");
        }
    }
}

static void
gtk_text_tag_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec,
                           const gchar  *trailer)
{
  GtkTextTag *text_tag;
  gboolean size_changed = FALSE;

  text_tag = GTK_TEXT_TAG (object);

  g_return_if_fail (!text_tag->values->realized);

  switch (prop_id)
    {
    case PROP_NAME:
      g_return_if_fail (text_tag->name == NULL);
      text_tag->name = g_strdup (g_value_get_string (value));
      break;

    case PROP_BACKGROUND:
      {
        GdkColor color;

        if (gdk_color_parse (g_value_get_string (value), &color))
          set_bg_color (text_tag, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));
      }
      break;

    case PROP_FOREGROUND:
      {
        GdkColor color;

        if (gdk_color_parse (g_value_get_string (value), &color))
          set_fg_color (text_tag, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));
      }
      break;

    case PROP_BACKGROUND_GDK:
      {
        GdkColor *color = g_value_get_as_pointer (value);
        set_bg_color (text_tag, color);
      }
      break;

    case PROP_FOREGROUND_GDK:
      {
        GdkColor *color = g_value_get_as_pointer (value);
        set_fg_color (text_tag, color);
      }
      break;

    case PROP_BACKGROUND_STIPPLE:
      {
        GdkBitmap *bitmap = g_value_get_as_pointer (value);

        text_tag->bg_stipple_set = TRUE;
        g_object_notify (G_OBJECT (text_tag), "bg_stipple_set");
        
        if (text_tag->values->appearance.bg_stipple != bitmap)
          {
            if (bitmap != NULL)
              gdk_bitmap_ref (bitmap);

            if (text_tag->values->appearance.bg_stipple)
              gdk_bitmap_unref (text_tag->values->appearance.bg_stipple);

            text_tag->values->appearance.bg_stipple = bitmap;
          }
      }
      break;

    case PROP_FOREGROUND_STIPPLE:
      {
        GdkBitmap *bitmap = g_value_get_as_pointer (value);

        text_tag->fg_stipple_set = TRUE;
        g_object_notify (G_OBJECT (text_tag), "fg_stipple_set");

        if (text_tag->values->appearance.fg_stipple != bitmap)
          {
            if (bitmap != NULL)
              gdk_bitmap_ref (bitmap);

            if (text_tag->values->appearance.fg_stipple)
              gdk_bitmap_unref (text_tag->values->appearance.fg_stipple);

            text_tag->values->appearance.fg_stipple = bitmap;
          }
      }
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const gchar *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (text_tag, font_desc);
        
        if (font_desc)
          pango_font_description_free (font_desc); 

        size_changed = TRUE;
      }
      break;

    case PROP_FONT_DESC:
      {
        PangoFontDescription *font_desc;

        font_desc = g_value_get_as_pointer (value);

        set_font_description (text_tag, font_desc);

        size_changed = TRUE;
      }
      break;

    case PROP_FAMILY:
      if (text_tag->values->font.family_name)
        g_free (text_tag->values->font.family_name);
      text_tag->values->font.family_name = g_strdup (g_value_get_string (value));
      text_tag->family_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "family_set");
      size_changed = TRUE;
      break;

    case PROP_STYLE:
      text_tag->values->font.style = g_value_get_enum (value);
      text_tag->style_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "style_set");
      size_changed = TRUE;
      break;

    case PROP_VARIANT:
      text_tag->values->font.variant = g_value_get_enum (value);
      text_tag->variant_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "variant_set");
      size_changed = TRUE;
      break;

    case PROP_WEIGHT:
      text_tag->values->font.weight = g_value_get_int (value);
      text_tag->weight_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "weight_set");
      size_changed = TRUE;
      break;

    case PROP_STRETCH:
      text_tag->values->font.stretch = g_value_get_enum (value);
      text_tag->stretch_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "stretch_set");
      size_changed = TRUE;
      break;

    case PROP_SIZE:
      text_tag->values->font.size = g_value_get_int (value);
      text_tag->size_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "size_set");
      size_changed = TRUE;
      break;

    case PROP_SIZE_POINTS:
      text_tag->values->font.size = g_value_get_double (value) * PANGO_SCALE;
      text_tag->size_set = TRUE;
      g_object_notify (G_OBJECT (text_tag), "size_set");
      size_changed = TRUE;
      break;
      
    case PROP_PIXELS_ABOVE_LINES:
      text_tag->pixels_above_lines_set = TRUE;
      text_tag->values->pixels_above_lines = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "pixels_above_lines_set");
      size_changed = TRUE;
      break;

    case PROP_PIXELS_BELOW_LINES:
      text_tag->pixels_below_lines_set = TRUE;
      text_tag->values->pixels_below_lines = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "pixels_below_lines_set");
      size_changed = TRUE;
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      text_tag->pixels_inside_wrap_set = TRUE;
      text_tag->values->pixels_inside_wrap = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "pixels_inside_wrap_set");
      size_changed = TRUE;
      break;

    case PROP_EDITABLE:
      text_tag->editable_set = TRUE;
      text_tag->values->editable = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (text_tag), "editable_set");
      break;

    case PROP_WRAP_MODE:
      text_tag->wrap_mode_set = TRUE;
      text_tag->values->wrap_mode = g_value_get_enum (value);
      g_object_notify (G_OBJECT (text_tag), "wrap_mode_set");
      size_changed = TRUE;
      break;

    case PROP_JUSTIFICATION:
      text_tag->justification_set = TRUE;
      text_tag->values->justification = g_value_get_enum (value);
      g_object_notify (G_OBJECT (text_tag), "justification_set");
      size_changed = TRUE;
      break;

    case PROP_DIRECTION:
      text_tag->values->direction = g_value_get_enum (value);
      break;

    case PROP_LEFT_MARGIN:
      text_tag->left_margin_set = TRUE;
      text_tag->values->left_margin = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "left_margin_set");
      size_changed = TRUE;
      break;

    case PROP_INDENT:
      text_tag->indent_set = TRUE;
      text_tag->values->indent = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "indent_set");
      size_changed = TRUE;
      break;

    case PROP_STRIKETHROUGH:
      text_tag->strikethrough_set = TRUE;
      text_tag->values->appearance.strikethrough = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (text_tag), "strikethrough_set");
      break;

    case PROP_RIGHT_MARGIN:
      text_tag->right_margin_set = TRUE;
      text_tag->values->right_margin = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "right_margin_set");
      size_changed = TRUE;
      break;

    case PROP_UNDERLINE:
      text_tag->underline_set = TRUE;
      text_tag->values->appearance.underline = g_value_get_enum (value);
      g_object_notify (G_OBJECT (text_tag), "underline_set");
      break;

    case PROP_RISE:
      text_tag->rise_set = TRUE;
      text_tag->values->appearance.rise = g_value_get_int (value);
      g_object_notify (G_OBJECT (text_tag), "rise_set");
      size_changed = TRUE;      
      break;

    case PROP_BG_FULL_HEIGHT:
      text_tag->bg_full_height_set = TRUE;
      text_tag->values->bg_full_height = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (text_tag), "bg_full_height_set");
      break;

    case PROP_LANGUAGE:
      text_tag->language_set = TRUE;
      text_tag->values->language = g_strdup (g_value_get_string (value));
      g_object_notify (G_OBJECT (text_tag), "language_set");
      break;

    case PROP_TABS:
      text_tag->tabs_set = TRUE;

      if (text_tag->values->tabs)
        pango_tab_array_free (text_tag->values->tabs);

      /* FIXME I'm not sure if this is a memleak or not */
      text_tag->values->tabs =
        pango_tab_array_copy (g_value_get_as_pointer (value));

      g_object_notify (G_OBJECT (text_tag), "tabs_set");
      
      size_changed = TRUE;
      break;

    case PROP_INVISIBLE:
      text_tag->invisible_set = TRUE;
      text_tag->values->invisible = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (text_tag), "invisible_set");
      size_changed = TRUE;
      break;
      
      /* Whether the value should be used... */

    case PROP_BACKGROUND_SET:
    case PROP_BACKGROUND_GDK_SET:
      text_tag->bg_color_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
    case PROP_FOREGROUND_GDK_SET:
      text_tag->fg_color_set = g_value_get_boolean (value);
      break;

    case PROP_BACKGROUND_STIPPLE_SET:
      text_tag->bg_stipple_set = g_value_get_boolean (value);
      if (!text_tag->bg_stipple_set &&
          text_tag->values->appearance.bg_stipple)
        {
          g_object_unref (G_OBJECT (text_tag->values->appearance.bg_stipple));
          text_tag->values->appearance.bg_stipple = NULL;
        }
      break;

    case PROP_FOREGROUND_STIPPLE_SET:
      text_tag->fg_stipple_set = g_value_get_boolean (value);
      if (!text_tag->fg_stipple_set &&
          text_tag->values->appearance.fg_stipple)
        {
          g_object_unref (G_OBJECT (text_tag->values->appearance.fg_stipple));
          text_tag->values->appearance.fg_stipple = NULL;
        }
      break;

    case PROP_FAMILY_SET:
      text_tag->family_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_STYLE_SET:
      text_tag->style_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_VARIANT_SET:
      text_tag->variant_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_WEIGHT_SET:
      text_tag->weight_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_STRETCH_SET:
      text_tag->stretch_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_SIZE_SET:
      text_tag->size_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    case PROP_PIXELS_ABOVE_LINES_SET:
      text_tag->pixels_above_lines_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_PIXELS_BELOW_LINES_SET:
      text_tag->pixels_below_lines_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_PIXELS_INSIDE_WRAP_SET:
      text_tag->pixels_inside_wrap_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_EDITABLE_SET:
      text_tag->editable_set = g_value_get_boolean (value);
      break;

    case PROP_WRAP_MODE_SET:
      text_tag->wrap_mode_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_JUSTIFICATION_SET:
      text_tag->justification_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    case PROP_LEFT_MARGIN_SET:
      text_tag->left_margin_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_INDENT_SET:
      text_tag->indent_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_STRIKETHROUGH_SET:
      text_tag->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_RIGHT_MARGIN_SET:
      text_tag->right_margin_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_UNDERLINE_SET:
      text_tag->underline_set = g_value_get_boolean (value);
      break;

    case PROP_RISE_SET:
      text_tag->rise_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_BG_FULL_HEIGHT_SET:
      text_tag->bg_full_height_set = g_value_get_boolean (value);
      break;

    case PROP_LANGUAGE_SET:
      text_tag->language_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_TABS_SET:
      text_tag->tabs_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;

    case PROP_INVISIBLE_SET:
      text_tag->invisible_set = g_value_get_boolean (value);
      size_changed = TRUE;
      break;
      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

  /* FIXME I would like to do this after all set_property in a single
   * g_object_set () have been called. But an idle function won't
   * work; we need to emit when the tag is changed, not when we get
   * around to the event loop. So blah, we eat some inefficiency.
   */

  /* This is also somewhat weird since we emit another object's
   * signal here, but the two objects are already tightly bound.
   */

  if (text_tag->table)
    g_signal_emit_by_name (G_OBJECT (text_tag->table),
                           "tag_changed",
                           text_tag, size_changed);
}

static void
get_color_arg (GValue *value, GdkColor *orig)
{
  GdkColor *color;

  color = g_new (GdkColor, 1);
  *color = *orig;
  g_value_init (value, GTK_TYPE_GDK_COLOR);
  g_value_set_boxed (value, color);
}

static void
gtk_text_tag_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec,
                           const gchar  *trailer)
{
  GtkTextTag *tag;

  tag = GTK_TEXT_TAG (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, tag->name);
      break;

    case PROP_BACKGROUND_GDK:
      get_color_arg (value, &tag->values->appearance.bg_color);
      break;

    case PROP_FOREGROUND_GDK:
      get_color_arg (value, &tag->values->appearance.fg_color);
      break;

    case PROP_BACKGROUND_STIPPLE:
      if (tag->bg_stipple_set)
        g_value_set_boxed (value, tag->values->appearance.bg_stipple);
      break;

    case PROP_FOREGROUND_STIPPLE:
      if (tag->fg_stipple_set)
        g_value_set_boxed (value, tag->values->appearance.fg_stipple);
      break;

    case PROP_FONT:
      if (tag->family_set &&
          tag->style_set &&
          tag->variant_set &&
          tag->size_set &&
          tag->stretch_set &&
          tag->weight_set)
        {
          /* FIXME GValue imposes a totally gratuitous string copy
           * here, we could just hand off string ownership
           */
          gchar *str = pango_font_description_to_string (&tag->values->font);
          g_value_set_string (value, str);
          g_free (str);
        }
      break;

    case PROP_FONT_DESC:
      if (tag->family_set &&
          tag->style_set &&
          tag->variant_set &&
          tag->size_set &&
          tag->stretch_set &&
          tag->weight_set)
        g_value_set_boxed (value, &tag->values->font);
      break;

    case PROP_FAMILY:
      g_value_set_string (value, tag->values->font.family_name);
      break;

    case PROP_STYLE:
      g_value_set_enum (value, tag->values->font.style);
      break;

    case PROP_VARIANT:
      g_value_set_enum (value, tag->values->font.variant);
      break;

    case PROP_WEIGHT:
      g_value_set_int (value, tag->values->font.weight);
      break;

    case PROP_STRETCH:
      g_value_set_enum (value, tag->values->font.stretch);
      break;

    case PROP_SIZE:
      g_value_set_int (value,  tag->values->font.size);
      break;

    case PROP_SIZE_POINTS:
      g_value_set_double (value, ((double)tag->values->font.size) / (double)PANGO_SCALE);
      break;
      
    case PROP_PIXELS_ABOVE_LINES:
      g_value_set_int (value,  tag->values->pixels_above_lines);
      break;

    case PROP_PIXELS_BELOW_LINES:
      g_value_set_int (value,  tag->values->pixels_below_lines);
      break;

    case PROP_PIXELS_INSIDE_WRAP:
      g_value_set_int (value,  tag->values->pixels_inside_wrap);
      break;

    case PROP_EDITABLE:
      g_value_set_boolean (value, tag->values->editable);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, tag->values->wrap_mode);
      break;

    case PROP_JUSTIFICATION:
      g_value_set_enum (value, tag->values->justification);
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, tag->values->direction);
      break;
      
    case PROP_LEFT_MARGIN:
      g_value_set_int (value,  tag->values->left_margin);
      break;

    case PROP_INDENT:
      g_value_set_int (value,  tag->values->indent);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, tag->values->appearance.strikethrough);
      break;

    case PROP_RIGHT_MARGIN:
      g_value_set_int (value, tag->values->right_margin);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, tag->values->appearance.underline);
      break;

    case PROP_RISE:
      g_value_set_int (value, tag->values->appearance.rise);
      break;

    case PROP_BG_FULL_HEIGHT:
      g_value_set_boolean (value, tag->values->bg_full_height);
      break;

    case PROP_LANGUAGE:
      g_value_set_string (value, tag->values->language);
      break;

    case PROP_TABS:
      if (tag->values->tabs)
        g_value_set_boxed (value, tag->values->tabs);
      break;

    case PROP_INVISIBLE:
      g_value_set_boolean (value, tag->values->invisible);
      break;
      
    case PROP_BACKGROUND_SET:
    case PROP_BACKGROUND_GDK_SET:
      g_value_set_boolean (value, tag->bg_color_set);
      break;

    case PROP_FOREGROUND_SET:
    case PROP_FOREGROUND_GDK_SET:
      g_value_set_boolean (value, tag->fg_color_set);
      break;

    case PROP_BACKGROUND_STIPPLE_SET:
      g_value_set_boolean (value, tag->bg_stipple_set);
      break;

    case PROP_FOREGROUND_STIPPLE_SET:
      g_value_set_boolean (value, tag->fg_stipple_set);
      break;

    case PROP_FAMILY_SET:
      g_value_set_boolean (value, tag->family_set);
      break;

    case PROP_STYLE_SET:
      g_value_set_boolean (value, tag->style_set);
      break;

    case PROP_VARIANT_SET:
      g_value_set_boolean (value, tag->variant_set);
      break;

    case PROP_WEIGHT_SET:
      g_value_set_boolean (value, tag->weight_set);
      break;

    case PROP_STRETCH_SET:
      g_value_set_boolean (value, tag->stretch_set);
      break;

    case PROP_SIZE_SET:
      g_value_set_boolean (value, tag->size_set);
      break;
      
    case PROP_PIXELS_ABOVE_LINES_SET:
      g_value_set_boolean (value, tag->pixels_above_lines_set);
      break;

    case PROP_PIXELS_BELOW_LINES_SET:
      g_value_set_boolean (value, tag->pixels_below_lines_set);
      break;

    case PROP_PIXELS_INSIDE_WRAP_SET:
      g_value_set_boolean (value, tag->pixels_inside_wrap_set);
      break;

    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, tag->editable_set);
      break;

    case PROP_WRAP_MODE_SET:
      g_value_set_boolean (value, tag->wrap_mode_set);
      break;

    case PROP_JUSTIFICATION_SET:
      g_value_set_boolean (value, tag->justification_set);
      break;
      
    case PROP_LEFT_MARGIN_SET:
      g_value_set_boolean (value, tag->left_margin_set);
      break;

    case PROP_INDENT_SET:
      g_value_set_boolean (value, tag->indent_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, tag->strikethrough_set);
      break;

    case PROP_RIGHT_MARGIN_SET:
      g_value_set_boolean (value, tag->right_margin_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, tag->underline_set);
      break;

    case PROP_RISE_SET:
      g_value_set_boolean (value, tag->rise_set);
      break;

    case PROP_BG_FULL_HEIGHT_SET:
      g_value_set_boolean (value, tag->bg_full_height_set);
      break;

    case PROP_LANGUAGE_SET:
      g_value_set_boolean (value, tag->language_set);
      break;

    case PROP_TABS_SET:
      g_value_set_boolean (value, tag->tabs_set);
      break;

    case PROP_INVISIBLE_SET:
      g_value_set_boolean (value, tag->invisible_set);
      break;
      
    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
      g_warning ("'foreground' and 'background' properties are not readable, use 'foreground_gdk' and 'background_gdk'");
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*
 * Tag operations
 */

typedef struct {
  gint high;
  gint low;
  gint delta;
} DeltaData;

static void
delta_priority_foreach (GtkTextTag *tag, gpointer user_data)
{
  DeltaData *dd = user_data;

  if (tag->priority >= dd->low && tag->priority <= dd->high)
    tag->priority += dd->delta;
}

/**
 * gtk_text_tag_get_priority:
 * @tag: a #GtkTextTag
 * 
 * Get the tag priority.
 * 
 * Return value: The tag's priority.
 **/
gint
gtk_text_tag_get_priority (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), 0);

  return tag->priority;
}

/**
 * gtk_text_tag_set_priority:
 * @tag: a #GtkTextTag
 * @priority: the new priority
 * 
 * Sets the priority of a #GtkTextTag. Valid priorities are
 * start at 0 and go to one less than gtk_text_tag_table_size().
 * Each tag in a table has a unique priority; setting the priority
 * of one tag shifts the priorities of all the other tags in the
 * table to maintain a unique priority for each tag. Higher priority
 * tags "win" if two tags both set the same text attribute. When adding
 * a tag to a tag table, it will be assigned the highest priority in
 * the table by default; so normally the precedence of a set of tags
 * is the order in which they were added to the table, or created with
 * gtk_text_buffer_create_tag(), which adds the tag to the buffer's table
 * automatically.
 **/
void
gtk_text_tag_set_priority (GtkTextTag *tag,
                           gint        priority)
{
  DeltaData dd;

  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (tag->table != NULL);
  g_return_if_fail (priority >= 0);
  g_return_if_fail (priority < gtk_text_tag_table_size (tag->table));

  if (priority == tag->priority)
    return;

  if (priority < tag->priority)
    {
      dd.low = priority;
      dd.high = tag->priority - 1;
      dd.delta = 1;
    }
  else
    {
      dd.low = tag->priority + 1;
      dd.high = priority;
      dd.delta = -1;
    }

  gtk_text_tag_table_foreach (tag->table,
                              delta_priority_foreach,
                              &dd);

  tag->priority = priority;
}

/**
 * gtk_text_tag_event:
 * @tag: a #GtkTextTag
 * @event_object: object that received the event, such as a widget
 * @event: the event
 * @iter: location where the event was received
 * 
 * Emits the "event" signal on the #GtkTextTag.
 * 
 * Return value: result of signal emission (whether the event was handled)
 **/
gint
gtk_text_tag_event (GtkTextTag        *tag,
                    GObject           *event_object,
                    GdkEvent          *event,
                    const GtkTextIter *iter)
{
  gint retval = FALSE;

  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), FALSE);
  g_return_val_if_fail (GTK_IS_OBJECT (event_object), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  g_signal_emit (G_OBJECT (tag),
                 signals[EVENT],
                 0,
                 event_object,
                 event,
                 iter,
                 &retval);

  return retval;
}

static int
tag_sort_func (gconstpointer first, gconstpointer second)
{
  GtkTextTag *tag1, *tag2;

  tag1 = * (GtkTextTag **) first;
  tag2 = * (GtkTextTag **) second;
  return tag1->priority - tag2->priority;
}

void
_gtk_text_tag_array_sort (GtkTextTag** tag_array_p,
                          guint len)
{
  int i, j, prio;
  GtkTextTag **tag;
  GtkTextTag **maxPtrPtr, *tmp;

  g_return_if_fail (tag_array_p != NULL);
  g_return_if_fail (len > 0);

  if (len < 2) {
    return;
  }
  if (len < 20) {
    GtkTextTag **iter = tag_array_p;

    for (i = len-1; i > 0; i--, iter++) {
      maxPtrPtr = tag = iter;
      prio = tag[0]->priority;
      for (j = i, tag++; j > 0; j--, tag++) {
        if (tag[0]->priority < prio) {
          prio = tag[0]->priority;
          maxPtrPtr = tag;
        }
      }
      tmp = *maxPtrPtr;
      *maxPtrPtr = *iter;
      *iter = tmp;
    }
  } else {
    qsort ((void *) tag_array_p, (unsigned) len, sizeof (GtkTextTag *),
           tag_sort_func);
  }

#if 0
  {
    printf ("Sorted tag array: \n");
    i = 0;
    while (i < len)
      {
        GtkTextTag *t = tag_array_p[i];
        printf ("  %s priority %d\n", t->name, t->priority);

        ++i;
      }
  }
#endif
}

/*
 * Attributes
 */

/**
 * gtk_text_attributes_new:
 * 
 * Creates a #GtkTextAttributes, which describes
 * a set of properties on some text.
 * 
 * Return value: a new #GtkTextAttributes
 **/
GtkTextAttributes*
gtk_text_attributes_new (void)
{
  GtkTextAttributes *values;

  values = g_new0 (GtkTextAttributes, 1);

  /* 0 is a valid value for most of the struct */

  values->refcount = 1;

  values->language = gtk_get_default_language ();

  return values;
}

GtkTextAttributes*
gtk_text_attributes_copy (GtkTextAttributes *src)
{
  GtkTextAttributes *dest;

  dest = gtk_text_attributes_new ();
  gtk_text_attributes_copy_values (src, dest);

  return dest;
}

/**
 * gtk_text_attributes_copy_values:
 * @src: a #GtkTextAttributes
 * @dest: another #GtkTextAttributes
 * 
 * Copies the values from @src to @dest so that @dest has the same values
 * as @src.
 **/
void
gtk_text_attributes_copy_values (GtkTextAttributes *src,
                                 GtkTextAttributes *dest)
{
  guint orig_refcount;

  g_return_if_fail (!dest->realized);

  if (src == dest)
    return;

  /* Add refs */

  if (src->appearance.bg_stipple)
    gdk_bitmap_ref (src->appearance.bg_stipple);

  if (src->appearance.fg_stipple)
    gdk_bitmap_ref (src->appearance.fg_stipple);

  /* Remove refs */

  if (dest->appearance.bg_stipple)
    gdk_bitmap_unref (dest->appearance.bg_stipple);

  if (dest->appearance.fg_stipple)
    gdk_bitmap_unref (dest->appearance.fg_stipple);

  if (dest->language)
    g_free (dest->language);

  if (dest->font.family_name)
    g_free (dest->font.family_name);
  
  /* Copy */
  orig_refcount = dest->refcount;

  *dest = *src;

  if (src->tabs)
    dest->tabs = pango_tab_array_copy (src->tabs);

  dest->language = g_strdup (src->language);

  dest->font.family_name = g_strdup (src->font.family_name);
  
  dest->refcount = orig_refcount;
  dest->realized = FALSE;
}

void
gtk_text_attributes_ref (GtkTextAttributes *values)
{
  g_return_if_fail (values != NULL);

  values->refcount += 1;
}

void
gtk_text_attributes_unref (GtkTextAttributes *values)
{
  g_return_if_fail (values != NULL);
  g_return_if_fail (values->refcount > 0);

  values->refcount -= 1;

  if (values->refcount == 0)
    {
      g_assert (!values->realized);

      if (values->appearance.bg_stipple)
        gdk_bitmap_unref (values->appearance.bg_stipple);

      if (values->appearance.fg_stipple)
        gdk_bitmap_unref (values->appearance.fg_stipple);

      if (values->tabs)
        pango_tab_array_free (values->tabs);

      if (values->language)
        g_free (values->language);

      if (values->font.family_name)
        g_free (values->font.family_name);
      
      g_free (values);
    }
}

void
_gtk_text_attributes_realize (GtkTextAttributes *values,
                              GdkColormap *cmap,
                              GdkVisual *visual)
{
  g_return_if_fail (values != NULL);
  g_return_if_fail (values->refcount > 0);
  g_return_if_fail (!values->realized);

  /* It is wrong to use this colormap, FIXME */
  gdk_colormap_alloc_color (cmap,
                            &values->appearance.fg_color,
                            FALSE, TRUE);

  gdk_colormap_alloc_color (cmap,
                            &values->appearance.bg_color,
                            FALSE, TRUE);

  values->realized = TRUE;
}

void
_gtk_text_attributes_unrealize (GtkTextAttributes *values,
                                GdkColormap *cmap,
                                GdkVisual *visual)
{
  g_return_if_fail (values != NULL);
  g_return_if_fail (values->refcount > 0);
  g_return_if_fail (values->realized);

  gdk_colormap_free_colors (cmap,
                            &values->appearance.fg_color, 1);


  gdk_colormap_free_colors (cmap,
                            &values->appearance.bg_color, 1);

  values->appearance.fg_color.pixel = 0;
  values->appearance.bg_color.pixel = 0;

  values->realized = FALSE;
}

void
_gtk_text_attributes_fill_from_tags (GtkTextAttributes *dest,
                                     GtkTextTag**       tags,
                                     guint              n_tags)
{
  guint n = 0;

  g_return_if_fail (!dest->realized);

  while (n < n_tags)
    {
      GtkTextTag *tag = tags[n];
      GtkTextAttributes *vals = tag->values;

      if (n > 0)
        g_assert (tags[n]->priority > tags[n-1]->priority);

      if (tag->bg_color_set)
        {
          dest->appearance.bg_color = vals->appearance.bg_color;

          dest->appearance.draw_bg = TRUE;
        }
      if (tag->fg_color_set)
        dest->appearance.fg_color = vals->appearance.fg_color;
      
      if (tag->bg_stipple_set)
        {
          gdk_bitmap_ref (vals->appearance.bg_stipple);
          if (dest->appearance.bg_stipple)
            gdk_bitmap_unref (dest->appearance.bg_stipple);
          dest->appearance.bg_stipple = vals->appearance.bg_stipple;

          dest->appearance.draw_bg = TRUE;
        }

      if (tag->fg_stipple_set)
        {
          gdk_bitmap_ref (vals->appearance.fg_stipple);
          if (dest->appearance.fg_stipple)
            gdk_bitmap_unref (dest->appearance.fg_stipple);
          dest->appearance.fg_stipple = vals->appearance.fg_stipple;
        }

      if (tag->family_set)
        {
          if (dest->font.family_name)
            g_free (dest->font.family_name);

          dest->font.family_name = g_strdup (vals->font.family_name);
        }

      if (tag->style_set)
        dest->font.style = vals->font.style;

      if (tag->variant_set)
        dest->font.variant = vals->font.variant;

      if (tag->weight_set)
        dest->font.weight = vals->font.weight;

      if (tag->stretch_set)
        dest->font.stretch = vals->font.stretch;

      if (tag->size_set)
        dest->font.size = vals->font.size;

      if (tag->justification_set)
        dest->justification = vals->justification;

      if (vals->direction != GTK_TEXT_DIR_NONE)
        dest->direction = vals->direction;

      if (tag->left_margin_set)
        dest->left_margin = vals->left_margin;

      if (tag->indent_set)
        dest->indent = vals->indent;

      if (tag->rise_set)
        dest->appearance.rise = vals->appearance.rise;

      if (tag->right_margin_set)
        dest->right_margin = vals->right_margin;

      if (tag->pixels_above_lines_set)
        dest->pixels_above_lines = vals->pixels_above_lines;

      if (tag->pixels_below_lines_set)
        dest->pixels_below_lines = vals->pixels_below_lines;

      if (tag->pixels_inside_wrap_set)
        dest->pixels_inside_wrap = vals->pixels_inside_wrap;

      if (tag->tabs_set)
        {
          if (dest->tabs)
            pango_tab_array_free (dest->tabs);
          dest->tabs = pango_tab_array_copy (vals->tabs);
        }

      if (tag->wrap_mode_set)
        dest->wrap_mode = vals->wrap_mode;

      if (tag->underline_set)
        dest->appearance.underline = vals->appearance.underline;

      if (tag->strikethrough_set)
        dest->appearance.strikethrough = vals->appearance.strikethrough;

      if (tag->invisible_set)
        dest->invisible = vals->invisible;

      if (tag->editable_set)
        dest->editable = vals->editable;

      if (tag->bg_full_height_set)
        dest->bg_full_height = vals->bg_full_height;

      if (tag->language_set)
        {
          g_free (dest->language);
          dest->language = g_strdup (vals->language);
        }

      ++n;
    }
}

gboolean
_gtk_text_tag_affects_size (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), FALSE);

  return
    tag->family_set ||
    tag->style_set ||
    tag->variant_set ||
    tag->weight_set ||
    tag->size_set ||
    tag->stretch_set ||
    tag->justification_set ||
    tag->left_margin_set ||
    tag->indent_set ||
    tag->rise_set ||
    tag->right_margin_set ||
    tag->pixels_above_lines_set ||
    tag->pixels_below_lines_set ||
    tag->pixels_inside_wrap_set ||
    tag->tabs_set ||
    tag->underline_set ||
    tag->wrap_mode_set ||
    tag->invisible_set;
}

gboolean
_gtk_text_tag_affects_nonsize_appearance (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), FALSE);

  return
    tag->bg_color_set ||
    tag->bg_stipple_set ||
    tag->fg_color_set ||
    tag->fg_stipple_set ||
    tag->strikethrough_set ||
    tag->bg_full_height_set;
}
