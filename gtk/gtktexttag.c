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

#include <stdlib.h>
#include <string.h>

enum {
  EVENT,
  LAST_SIGNAL
};

enum {
  ARG_0,
  /* Construct args */
  ARG_NAME,

  /* Style args */
  ARG_BACKGROUND,
  ARG_FOREGROUND,
  ARG_BACKGROUND_GDK,
  ARG_FOREGROUND_GDK,
  ARG_BACKGROUND_STIPPLE,
  ARG_FOREGROUND_STIPPLE,
  ARG_FONT,
  ARG_FONT_DESC,
  ARG_PIXELS_ABOVE_LINES,
  ARG_PIXELS_BELOW_LINES,
  ARG_PIXELS_INSIDE_WRAP,
  ARG_EDITABLE,
  ARG_WRAP_MODE,
  ARG_JUSTIFY,
  ARG_DIRECTION,
  ARG_LEFT_MARGIN,
  ARG_LEFT_WRAPPED_LINE_MARGIN,
  ARG_STRIKETHROUGH,
  ARG_RIGHT_MARGIN,
  ARG_UNDERLINE,
  ARG_OFFSET,
  ARG_BG_FULL_HEIGHT,
  ARG_LANGUAGE,
  ARG_TABS,
  
  /* Whether-a-style-arg-is-set args */
  ARG_BACKGROUND_SET,
  ARG_FOREGROUND_SET,
  ARG_BACKGROUND_GDK_SET,
  ARG_FOREGROUND_GDK_SET,
  ARG_BACKGROUND_STIPPLE_SET,
  ARG_FOREGROUND_STIPPLE_SET,
  ARG_FONT_SET,
  ARG_PIXELS_ABOVE_LINES_SET,
  ARG_PIXELS_BELOW_LINES_SET,
  ARG_PIXELS_INSIDE_WRAP_SET,
  ARG_EDITABLE_SET,
  ARG_WRAP_MODE_SET,
  ARG_JUSTIFY_SET,
  ARG_LEFT_MARGIN_SET,
  ARG_LEFT_WRAPPED_LINE_MARGIN_SET,
  ARG_STRIKETHROUGH_SET,
  ARG_RIGHT_MARGIN_SET,
  ARG_UNDERLINE_SET,
  ARG_OFFSET_SET,
  ARG_BG_FULL_HEIGHT_SET,
  ARG_LANGUAGE_SET,
  ARG_TABS_SET,
  
  LAST_ARG
};

static void gtk_text_tag_init       (GtkTextTag      *tkxt_tag);
static void gtk_text_tag_class_init (GtkTextTagClass *klass);
static void gtk_text_tag_destroy    (GtkObject       *object);
static void gtk_text_tag_finalize   (GObject         *object);
static void gtk_text_tag_set_arg    (GtkObject       *object,
				     GtkArg          *arg,
				     guint            arg_id);
static void gtk_text_tag_get_arg    (GtkObject       *object,
				     GtkArg          *arg,
				     guint            arg_id);

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_text_tag_get_type (void)
{
  static GtkType our_type = 0;

  if (our_type == 0)
    {
      static const GtkTypeInfo our_info =
      {
        "GtkTextTag",
        sizeof (GtkTextTag),
        sizeof (GtkTextTagClass),
        (GtkClassInitFunc) gtk_text_tag_class_init,
        (GtkObjectInitFunc) gtk_text_tag_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

    our_type = gtk_type_unique (GTK_TYPE_OBJECT, &our_info);
  }

  return our_type;
}

static void
gtk_text_tag_class_init (GtkTextTagClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

  parent_class = gtk_type_class (GTK_TYPE_OBJECT);

  /* Construct */
  gtk_object_add_arg_type ("GtkTextTag::name", GTK_TYPE_STRING,
                           GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
                           ARG_NAME);

  /* Style args */
  gtk_object_add_arg_type ("GtkTextTag::background", GTK_TYPE_STRING,
                           GTK_ARG_WRITABLE, ARG_BACKGROUND);
  gtk_object_add_arg_type ("GtkTextTag::background_gdk", GTK_TYPE_GDK_COLOR,
                           GTK_ARG_READWRITE, ARG_BACKGROUND_GDK);
  gtk_object_add_arg_type ("GtkTextTag::background_full_height", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_BG_FULL_HEIGHT);
  gtk_object_add_arg_type ("GtkTextTag::background_stipple",
                           GDK_TYPE_PIXMAP,
                           GTK_ARG_READWRITE, ARG_BACKGROUND_STIPPLE);
  gtk_object_add_arg_type ("GtkTextTag::direction", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_DIRECTION);
  gtk_object_add_arg_type ("GtkTextTag::editable", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_EDITABLE);
  gtk_object_add_arg_type ("GtkTextTag::font", GTK_TYPE_STRING,
                           GTK_ARG_READWRITE, ARG_FONT);
  gtk_object_add_arg_type ("GtkTextTag::font_desc", GTK_TYPE_BOXED,
                           GTK_ARG_READWRITE, ARG_FONT_DESC);
  gtk_object_add_arg_type ("GtkTextTag::foreground", GTK_TYPE_STRING,
                           GTK_ARG_WRITABLE, ARG_FOREGROUND);
  gtk_object_add_arg_type ("GtkTextTag::foreground_gdk", GTK_TYPE_GDK_COLOR,
                           GTK_ARG_READWRITE, ARG_FOREGROUND_GDK);
  gtk_object_add_arg_type ("GtkTextTag::foreground_stipple",
                           GDK_TYPE_PIXMAP,
                           GTK_ARG_READWRITE, ARG_FOREGROUND_STIPPLE);
  gtk_object_add_arg_type ("GtkTextTag::justify", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_JUSTIFY);
  gtk_object_add_arg_type ("GtkTextTag::language", GTK_TYPE_STRING,
                           GTK_ARG_READWRITE, ARG_LANGUAGE);
  gtk_object_add_arg_type ("GtkTextTag::left_margin", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_LEFT_MARGIN);
  gtk_object_add_arg_type ("GtkTextTag::left_wrapped_line_margin", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_LEFT_WRAPPED_LINE_MARGIN);
  gtk_object_add_arg_type ("GtkTextTag::offset", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_OFFSET);
  gtk_object_add_arg_type ("GtkTextTag::pixels_above_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_ABOVE_LINES);
  gtk_object_add_arg_type ("GtkTextTag::pixels_below_lines", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_BELOW_LINES);
  gtk_object_add_arg_type ("GtkTextTag::pixels_inside_wrap", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_PIXELS_INSIDE_WRAP);
  gtk_object_add_arg_type ("GtkTextTag::right_margin", GTK_TYPE_INT,
                           GTK_ARG_READWRITE, ARG_RIGHT_MARGIN);
  gtk_object_add_arg_type ("GtkTextTag::strikethrough", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_STRIKETHROUGH);
  gtk_object_add_arg_type ("GtkTextTag::underline", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_UNDERLINE);
  gtk_object_add_arg_type ("GtkTextTag::wrap_mode", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_WRAP_MODE);
  gtk_object_add_arg_type ("GtkTextTag::tabs", GTK_TYPE_POINTER,
                           GTK_ARG_READWRITE, ARG_TABS);
  
  /* Style args are set or not */
  gtk_object_add_arg_type ("GtkTextTag::background_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_BACKGROUND_SET);
  gtk_object_add_arg_type ("GtkTextTag::background_full_height_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_BG_FULL_HEIGHT_SET);
  gtk_object_add_arg_type ("GtkTextTag::background_gdk_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_BACKGROUND_GDK_SET);
  gtk_object_add_arg_type ("GtkTextTag::background_stipple_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_BACKGROUND_STIPPLE_SET);
  gtk_object_add_arg_type ("GtkTextTag::editable_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_EDITABLE_SET);
  gtk_object_add_arg_type ("GtkTextTag::font_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_FONT_SET);
  gtk_object_add_arg_type ("GtkTextTag::foreground_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_FOREGROUND_SET);
  gtk_object_add_arg_type ("GtkTextTag::foreground_gdk_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_FOREGROUND_GDK_SET);
  gtk_object_add_arg_type ("GtkTextTag::foreground_stipple_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_FOREGROUND_STIPPLE_SET);
  gtk_object_add_arg_type ("GtkTextTag::justify_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_JUSTIFY_SET);
  gtk_object_add_arg_type ("GtkTextTag::language_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_LANGUAGE_SET);
  gtk_object_add_arg_type ("GtkTextTag::left_margin_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_LEFT_MARGIN_SET);
  gtk_object_add_arg_type ("GtkTextTag::left_wrapped_line_margin_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_LEFT_WRAPPED_LINE_MARGIN_SET);
  gtk_object_add_arg_type ("GtkTextTag::offset_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_OFFSET_SET);  
  gtk_object_add_arg_type ("GtkTextTag::pixels_above_lines_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_PIXELS_ABOVE_LINES_SET);
  gtk_object_add_arg_type ("GtkTextTag::pixels_below_lines_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_PIXELS_BELOW_LINES_SET);
  gtk_object_add_arg_type ("GtkTextTag::pixels_inside_wrap_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_PIXELS_INSIDE_WRAP_SET);
  gtk_object_add_arg_type ("GtkTextTag::strikethrough_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_STRIKETHROUGH_SET);
  gtk_object_add_arg_type ("GtkTextTag::right_margin_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_RIGHT_MARGIN_SET);
  gtk_object_add_arg_type ("GtkTextTag::underline_set", GTK_TYPE_ENUM,
                           GTK_ARG_READWRITE, ARG_UNDERLINE_SET);
  gtk_object_add_arg_type ("GtkTextTag::wrap_mode_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_WRAP_MODE_SET);
  gtk_object_add_arg_type ("GtkTextTag::tabs_set", GTK_TYPE_BOOL,
                           GTK_ARG_READWRITE, ARG_TABS_SET);

  
  signals[EVENT] =
    gtk_signal_new ("event",
                    GTK_RUN_LAST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTextTagClass, event),
                    gtk_marshal_INT__OBJECT_BOXED_POINTER,
                    GTK_TYPE_INT,
                    3,
                    GTK_TYPE_OBJECT,
                    GTK_TYPE_GDK_EVENT,
                    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

  object_class->set_arg = gtk_text_tag_set_arg;
  object_class->get_arg = gtk_text_tag_get_arg;

  object_class->destroy = gtk_text_tag_destroy;
  gobject_class->finalize = gtk_text_tag_finalize;
}

void
gtk_text_tag_init (GtkTextTag *tkxt_tag)
{
  /* 0 is basically a fine way to initialize everything in the
     entire struct */
  
}

GtkTextTag*
gtk_text_tag_new (const gchar *name)
{
  GtkTextTag *tag;

  tag = GTK_TEXT_TAG (gtk_type_new (gtk_text_tag_get_type ()));

  tag->name = g_strdup(name);

  tag->values = gtk_text_attributes_new();
  
  return tag;
}

static void
gtk_text_tag_destroy (GtkObject *object)
{
  GtkTextTag *tkxt_tag;

  tkxt_tag = GTK_TEXT_TAG (object);

  g_assert(!tkxt_tag->values->realized);
  
  if (tkxt_tag->table)
    gtk_text_tag_table_remove(tkxt_tag->table, tkxt_tag);

  g_assert(tkxt_tag->table == NULL);
  
  gtk_text_attributes_unref(tkxt_tag->values);
  tkxt_tag->values = NULL;
  
  (* GTK_OBJECT_CLASS(parent_class)->destroy) (object);
}

static void
gtk_text_tag_finalize (GObject *object)
{
  GtkTextTag *tkxt_tag;

  tkxt_tag = GTK_TEXT_TAG (object);

  g_free(tkxt_tag->name);
  tkxt_tag->name = NULL;

  (* G_OBJECT_CLASS(parent_class)->finalize) (object);
}

static void
set_bg_color(GtkTextTag *tag, GdkColor *color)
{
  if (color)
    {
      tag->bg_color_set = TRUE;
      tag->values->appearance.bg_color = *color;
    }
  else
    {
      tag->bg_color_set = FALSE;
    }
}

static void
set_fg_color(GtkTextTag *tag, GdkColor *color)
{
  if (color)
    {
      tag->fg_color_set = TRUE;
      tag->values->appearance.fg_color = *color;
    }
  else
    {
      tag->fg_color_set = FALSE;
    }
}

static void
gtk_text_tag_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextTag *tkxt_tag;
  gboolean size_changed = FALSE;
  
  tkxt_tag = GTK_TEXT_TAG (object);

  g_return_if_fail(!tkxt_tag->values->realized);
  
  switch (arg_id)
    {
    case ARG_NAME:
      g_return_if_fail(tkxt_tag->name == NULL);
      tkxt_tag->name = g_strdup(GTK_VALUE_STRING(*arg));
      break;

    case ARG_BACKGROUND:
      {
        GdkColor color;

        if (gdk_color_parse(GTK_VALUE_STRING(*arg), &color))
          set_bg_color(tkxt_tag, &color);
        else
          g_warning("Don't know color `%s'", GTK_VALUE_STRING(*arg));
      }
      break;

    case ARG_FOREGROUND:
      {
        GdkColor color;

        if (gdk_color_parse(GTK_VALUE_STRING(*arg), &color))
          set_fg_color(tkxt_tag, &color);
        else
          g_warning("Don't know color `%s'", GTK_VALUE_STRING(*arg));
      }
      break;
      
    case ARG_BACKGROUND_GDK:
      {
        GdkColor *color = GTK_VALUE_POINTER(*arg);
        set_bg_color(tkxt_tag, color);
      }
      break;

    case ARG_FOREGROUND_GDK:
      {
        GdkColor *color = GTK_VALUE_POINTER(*arg);
        set_fg_color(tkxt_tag, color);
      }
      break;

    case ARG_BACKGROUND_STIPPLE:
      {
        GdkBitmap *bitmap = GTK_VALUE_POINTER(*arg);

        tkxt_tag->bg_stipple_set = TRUE;
        
        if (tkxt_tag->values->appearance.bg_stipple != bitmap)
          {
            if (bitmap != NULL)
              gdk_bitmap_ref(bitmap);
            
            if (tkxt_tag->values->appearance.bg_stipple)
              gdk_bitmap_unref(tkxt_tag->values->appearance.bg_stipple);
            
            tkxt_tag->values->appearance.bg_stipple = bitmap;
          }
      }
      break;

    case ARG_FOREGROUND_STIPPLE:
      {
        GdkBitmap *bitmap = GTK_VALUE_POINTER(*arg);

        tkxt_tag->fg_stipple_set = TRUE;
        
        if (tkxt_tag->values->appearance.fg_stipple != bitmap)
          {
            if (bitmap != NULL)
              gdk_bitmap_ref(bitmap);
            
            if (tkxt_tag->values->appearance.fg_stipple)
              gdk_bitmap_unref(tkxt_tag->values->appearance.fg_stipple);
            
            tkxt_tag->values->appearance.fg_stipple = bitmap;
          }
      }
      break;

    case ARG_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const gchar *name;

        name = GTK_VALUE_STRING(*arg);        

        if (name)
	  font_desc = pango_font_description_from_string (name);

	if (tkxt_tag->values->font_desc)
	  pango_font_description_free (tkxt_tag->values->font_desc);
	
	tkxt_tag->font_set = (font_desc != NULL);
	tkxt_tag->values->font_desc = font_desc;

        size_changed = TRUE;
      }
      break;

    case ARG_FONT_DESC:
      {
        PangoFontDescription *font_desc;

        font_desc = GTK_VALUE_BOXED(*arg);        

	if (tkxt_tag->values->font_desc)
	  pango_font_description_free (tkxt_tag->values->font_desc);

	if (font_desc)
	  tkxt_tag->values->font_desc = pango_font_description_copy (font_desc);
	else
	  tkxt_tag->values->font_desc = NULL;
	
	tkxt_tag->font_set = (font_desc != NULL);

        size_changed = TRUE;
      }
      break;

    case ARG_PIXELS_ABOVE_LINES:
      tkxt_tag->pixels_above_lines_set = TRUE;
      tkxt_tag->values->pixels_above_lines = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_PIXELS_BELOW_LINES:
      tkxt_tag->pixels_below_lines_set = TRUE;
      tkxt_tag->values->pixels_below_lines = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      tkxt_tag->pixels_inside_wrap_set = TRUE;
      tkxt_tag->values->pixels_inside_wrap = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_EDITABLE:
      tkxt_tag->editable_set = TRUE;
      tkxt_tag->values->editable = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_WRAP_MODE:
      tkxt_tag->wrap_mode_set = TRUE;
      tkxt_tag->values->wrap_mode = GTK_VALUE_ENUM(*arg);
      size_changed = TRUE;
      break;

    case ARG_JUSTIFY:
      tkxt_tag->justify_set = TRUE;
      tkxt_tag->values->justify = GTK_VALUE_ENUM(*arg);
      size_changed = TRUE;
      break;

    case ARG_DIRECTION:
      tkxt_tag->values->direction = GTK_VALUE_ENUM(*arg);
      break;

    case ARG_LEFT_MARGIN:
      tkxt_tag->left_margin_set = TRUE;
      tkxt_tag->values->left_margin = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_LEFT_WRAPPED_LINE_MARGIN:
      tkxt_tag->left_wrapped_line_margin_set = TRUE;
      tkxt_tag->values->left_wrapped_line_margin = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_STRIKETHROUGH:
      tkxt_tag->strikethrough_set = TRUE;
      tkxt_tag->values->appearance.strikethrough = GTK_VALUE_BOOL(*arg);
      break;
      
    case ARG_RIGHT_MARGIN:
      tkxt_tag->right_margin_set = TRUE;
      tkxt_tag->values->right_margin = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;
      
    case ARG_UNDERLINE:
      tkxt_tag->underline_set = TRUE;
      tkxt_tag->values->appearance.underline = GTK_VALUE_ENUM(*arg);
      break;
      
    case ARG_OFFSET:
      tkxt_tag->offset_set = TRUE;
      tkxt_tag->values->offset = GTK_VALUE_INT(*arg);
      size_changed = TRUE;
      break;

    case ARG_BG_FULL_HEIGHT:
      tkxt_tag->bg_full_height_set = TRUE;
      tkxt_tag->values->bg_full_height = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_LANGUAGE:
      tkxt_tag->language_set = TRUE;
      tkxt_tag->values->language = g_strdup (GTK_VALUE_STRING(*arg));
      break;

    case ARG_TABS:
      tkxt_tag->tabs_set = TRUE;

      if (tkxt_tag->values->tabs)
        pango_tab_array_free (tkxt_tag->values->tabs);
      
      tkxt_tag->values->tabs =
        pango_tab_array_copy (GTK_VALUE_POINTER (*arg));

      size_changed = TRUE;
      break;
      
      /* Whether the value should be used... */
      
    case ARG_BACKGROUND_SET:
    case ARG_BACKGROUND_GDK_SET:
      tkxt_tag->bg_color_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_FOREGROUND_SET:
    case ARG_FOREGROUND_GDK_SET:
      tkxt_tag->fg_color_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_BACKGROUND_STIPPLE_SET:
      tkxt_tag->bg_stipple_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_FOREGROUND_STIPPLE_SET:
      tkxt_tag->fg_stipple_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_FONT_SET:
      tkxt_tag->font_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_PIXELS_ABOVE_LINES_SET:
      tkxt_tag->pixels_above_lines_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_PIXELS_BELOW_LINES_SET:
      tkxt_tag->pixels_below_lines_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_PIXELS_INSIDE_WRAP_SET:
      tkxt_tag->pixels_inside_wrap_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_EDITABLE_SET:
      tkxt_tag->editable_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_WRAP_MODE_SET:
      tkxt_tag->wrap_mode_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_JUSTIFY_SET:
      tkxt_tag->justify_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_LEFT_MARGIN_SET:
      tkxt_tag->left_margin_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_LEFT_WRAPPED_LINE_MARGIN_SET:
      tkxt_tag->left_wrapped_line_margin_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_STRIKETHROUGH_SET:
      tkxt_tag->strikethrough_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_RIGHT_MARGIN_SET:
      tkxt_tag->right_margin_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_UNDERLINE_SET:
      tkxt_tag->underline_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_OFFSET_SET:
      tkxt_tag->offset_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_BG_FULL_HEIGHT_SET:
      tkxt_tag->bg_full_height_set = GTK_VALUE_BOOL(*arg);
      break;

    case ARG_LANGUAGE_SET:
      tkxt_tag->language_set = GTK_VALUE_BOOL(*arg);
      size_changed = TRUE;
      break;

    case ARG_TABS_SET:
      tkxt_tag->tabs_set = GTK_VALUE_BOOL (*arg);
      size_changed = TRUE;
      break;
      
    default:
      g_assert_not_reached();
      break;
    }
  
  /* FIXME I would like to do this after all set_arg in a single
     gtk_object_set() have been called. But an idle function
     won't work; we need to emit when the tag is changed, not
     when we get around to the event loop. So blah, we eat some
     inefficiency. */
  
  /* This is also somewhat weird since we emit another object's
     signal here, but the two objects are already tightly bound. */
  
  if (tkxt_tag->table)
    gtk_signal_emit_by_name(GTK_OBJECT(tkxt_tag->table),
                            "tag_changed",
                            tkxt_tag, size_changed);
}

static void
get_color_arg (GtkArg *arg, GdkColor *orig)
{
  GdkColor *color;
  
  color = g_new (GdkColor, 1);
  *color = *orig;
  GTK_VALUE_BOXED (*arg) = color;
}

static void
gtk_text_tag_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
  GtkTextTag *tag;

  tag = GTK_TEXT_TAG (object);

  switch (arg_id)
    {
    case ARG_NAME:
      GTK_VALUE_STRING(*arg) = g_strdup(tag->name);
      break;
      
    case ARG_BACKGROUND_GDK:
      get_color_arg(arg, &tag->values->appearance.bg_color);
      break;

    case ARG_FOREGROUND_GDK:
      get_color_arg(arg, &tag->values->appearance.fg_color);
      break;

    case ARG_BACKGROUND_STIPPLE:
      GTK_VALUE_BOXED(*arg) = tag->values->appearance.bg_stipple;
      break;

    case ARG_FOREGROUND_STIPPLE:
      GTK_VALUE_BOXED(*arg) = tag->values->appearance.fg_stipple;
      break;

    case ARG_FONT:
      if (tag->values->font_desc)
	GTK_VALUE_STRING(*arg) = pango_font_description_to_string (tag->values->font_desc);
      else
	GTK_VALUE_STRING(*arg) = NULL;
      break;

    case ARG_FONT_DESC:
      if (tag->values->font_desc)
	GTK_VALUE_BOXED(*arg) = pango_font_description_copy (tag->values->font_desc);
      else
	GTK_VALUE_BOXED(*arg) = NULL;
      break;

    case ARG_PIXELS_ABOVE_LINES:
      GTK_VALUE_INT(*arg) = tag->values->pixels_above_lines;
      break;

    case ARG_PIXELS_BELOW_LINES:
      GTK_VALUE_INT(*arg) = tag->values->pixels_below_lines;
      break;

    case ARG_PIXELS_INSIDE_WRAP:
      GTK_VALUE_INT(*arg) = tag->values->pixels_inside_wrap;
      break;

    case ARG_EDITABLE:
      GTK_VALUE_BOOL(*arg) = tag->values->editable;
      break;      

    case ARG_WRAP_MODE:
      GTK_VALUE_ENUM(*arg) = tag->values->wrap_mode;
      break;

    case ARG_JUSTIFY:
      GTK_VALUE_ENUM(*arg) = tag->values->justify;
      break;

    case ARG_LEFT_MARGIN:
      GTK_VALUE_INT(*arg) = tag->values->left_margin;
      break;

    case ARG_LEFT_WRAPPED_LINE_MARGIN:
      GTK_VALUE_INT(*arg) = tag->values->left_wrapped_line_margin;
      break;

    case ARG_STRIKETHROUGH:
      GTK_VALUE_BOOL(*arg) = tag->values->appearance.strikethrough;
      break;
      
    case ARG_RIGHT_MARGIN:
      GTK_VALUE_INT(*arg) = tag->values->right_margin;
      break;
      
    case ARG_UNDERLINE:
      GTK_VALUE_ENUM(*arg) = tag->values->appearance.underline;
      break;

    case ARG_OFFSET:
      GTK_VALUE_INT(*arg) = tag->values->offset;
      break;

    case ARG_BG_FULL_HEIGHT:
      GTK_VALUE_BOOL(*arg) = tag->values->bg_full_height;
      break;

    case ARG_LANGUAGE:
      GTK_VALUE_STRING(*arg) = g_strdup (tag->values->language);
      break;

    case ARG_TABS:
      GTK_VALUE_POINTER (*arg) = tag->values->tabs ? 
        pango_tab_array_copy (tag->values->tabs) : NULL;
      break;
      
    case ARG_BACKGROUND_SET:
    case ARG_BACKGROUND_GDK_SET:
      GTK_VALUE_BOOL(*arg) = tag->bg_color_set;
      break;

    case ARG_FOREGROUND_SET:
    case ARG_FOREGROUND_GDK_SET:
      GTK_VALUE_BOOL(*arg) = tag->fg_color_set;
      break;

    case ARG_BACKGROUND_STIPPLE_SET:
      GTK_VALUE_BOOL(*arg) = tag->bg_stipple_set;
      break;

    case ARG_FOREGROUND_STIPPLE_SET:
      GTK_VALUE_BOOL(*arg) = tag->fg_stipple_set;
      break;

    case ARG_FONT_SET:
      GTK_VALUE_BOOL(*arg) = tag->font_set;
      break;

    case ARG_PIXELS_ABOVE_LINES_SET:
      GTK_VALUE_BOOL(*arg) = tag->pixels_above_lines_set;
      break;

    case ARG_PIXELS_BELOW_LINES_SET:
      GTK_VALUE_BOOL(*arg) = tag->pixels_below_lines_set;
      break;

    case ARG_PIXELS_INSIDE_WRAP_SET:
      GTK_VALUE_BOOL(*arg) = tag->pixels_inside_wrap_set;
      break;

    case ARG_EDITABLE_SET:
      GTK_VALUE_BOOL(*arg) = tag->editable_set;
      break;

    case ARG_WRAP_MODE_SET:
      GTK_VALUE_BOOL(*arg) = tag->wrap_mode_set;
      break;

    case ARG_JUSTIFY_SET:
      GTK_VALUE_BOOL(*arg) = tag->justify_set;
      break;

    case ARG_LEFT_MARGIN_SET:
      GTK_VALUE_BOOL(*arg) = tag->left_margin_set;
      break;

    case ARG_LEFT_WRAPPED_LINE_MARGIN_SET:
      GTK_VALUE_BOOL(*arg) = tag->left_wrapped_line_margin_set;
      break;

    case ARG_STRIKETHROUGH_SET:
      GTK_VALUE_BOOL(*arg) = tag->strikethrough_set;
      break;

    case ARG_RIGHT_MARGIN_SET:
      GTK_VALUE_BOOL(*arg) = tag->right_margin_set;
      break;

    case ARG_UNDERLINE_SET:
      GTK_VALUE_BOOL(*arg) = tag->underline_set;
      break;

    case ARG_OFFSET_SET:
      GTK_VALUE_BOOL(*arg) = tag->offset_set;
      break;

    case ARG_BG_FULL_HEIGHT_SET:
      GTK_VALUE_BOOL(*arg) = tag->bg_full_height_set;
      break;

    case ARG_LANGUAGE_SET:
      GTK_VALUE_BOOL(*arg) = tag->language_set;
      break;

    case ARG_TABS_SET:
      GTK_VALUE_BOOL (*arg) = tag->tabs_set;
      break;
      
    case ARG_BACKGROUND:
    case ARG_FOREGROUND:
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
  /* FIXME */
  arg->type = GTK_TYPE_INVALID;
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
delta_priority_foreach(GtkTextTag *tag, gpointer user_data)
{
  DeltaData *dd = user_data;

  if (tag->priority >= dd->low && tag->priority <= dd->high)
    tag->priority += dd->delta;
}

gint
gtk_text_tag_get_priority (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG(tag), 0);

  return tag->priority;
}

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

gint
gtk_text_tag_event (GtkTextTag *tag,
                    GtkObject *event_object,
                    GdkEvent *event,
                    const GtkTextIter *iter)
{
  gint retval = FALSE;

  g_return_val_if_fail(GTK_IS_TEXT_TAG(tag), FALSE);
  g_return_val_if_fail(GTK_IS_OBJECT(event_object), FALSE);
  g_return_val_if_fail(event != NULL, FALSE);
  
  gtk_signal_emit (GTK_OBJECT(tag),
                   signals[EVENT],
                   event_object,
                   event,
                   iter,
                   &retval);

  return retval;
}

static int
tag_sort_func(gconstpointer first, gconstpointer second)
{
    GtkTextTag *tag1, *tag2;

    tag1 = * (GtkTextTag **) first;
    tag2 = * (GtkTextTag **) second;
    return tag1->priority - tag2->priority;
}

void
gtk_text_tag_array_sort(GtkTextTag** tag_array_p,
                         guint len)
{
  int i, j, prio;
  GtkTextTag **tag;
  GtkTextTag **maxPtrPtr, *tmp;

  g_return_if_fail(tag_array_p != NULL);
  g_return_if_fail(len > 0);
  
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
    qsort((void *) tag_array_p, (unsigned) len, sizeof (GtkTextTag *),
          tag_sort_func);
  }

#if 0
  {
    printf("Sorted tag array: \n");
    i = 0;
    while (i < len)
      {
        GtkTextTag *t = tag_array_p[i];
        printf("  %s priority %d\n", t->name, t->priority);
        
        ++i;
      }
  }
#endif
}

/*
 * Attributes
 */

GtkTextAttributes*
gtk_text_attributes_new(void)
{
  GtkTextAttributes *values;

  values = g_new0(GtkTextAttributes, 1);

  /* 0 is a valid value for most of the struct */

  values->refcount = 1;

  values->language = gtk_get_default_language ();
  
  return values;
}

void
gtk_text_attributes_copy(GtkTextAttributes *src,
                         GtkTextAttributes *dest)
{
  guint orig_refcount;

  g_return_if_fail(!dest->realized);

  if (src == dest)
    return;
  
  /* Add refs */
  
  if (src->appearance.bg_stipple)
    gdk_bitmap_ref(src->appearance.bg_stipple);

  if (src->appearance.fg_stipple)
    gdk_bitmap_ref(src->appearance.fg_stipple);

  /* Remove refs */
  
  if (dest->appearance.bg_stipple)
    gdk_bitmap_unref(dest->appearance.bg_stipple);

  if (dest->appearance.fg_stipple)
    gdk_bitmap_unref(dest->appearance.fg_stipple);

  /* Copy */
  orig_refcount = dest->refcount;
  
  *dest = *src;

  dest->font_desc = pango_font_description_copy (src->font_desc);

  if (src->tabs)
    dest->tabs = pango_tab_array_copy (src->tabs);

  dest->language = g_strdup (src->language);
  
  dest->refcount = orig_refcount;
  dest->realized = FALSE;
}

void
gtk_text_attributes_ref(GtkTextAttributes *values)
{
  g_return_if_fail(values != NULL);

  values->refcount += 1;
}

void
gtk_text_attributes_unref(GtkTextAttributes *values)
{
  g_return_if_fail(values != NULL);
  g_return_if_fail(values->refcount > 0);

  values->refcount -= 1;

  if (values->refcount == 0)
    {
      g_assert(!values->realized);
      
      if (values->appearance.bg_stipple)
        gdk_bitmap_unref(values->appearance.bg_stipple);
      
      if (values->font_desc)
        pango_font_description_free (values->font_desc);
      
      if (values->appearance.fg_stipple)
        gdk_bitmap_unref(values->appearance.fg_stipple);

      if (values->tabs)
        pango_tab_array_free (values->tabs);

      if (values->language)
        g_free (values->language);
      
      g_free(values);
    }
}

void
gtk_text_attributes_realize(GtkTextAttributes *values,
                              GdkColormap *cmap,
                              GdkVisual *visual)
{
  g_return_if_fail(values != NULL);
  g_return_if_fail(values->refcount > 0);
  g_return_if_fail(!values->realized);
  
  /* It is wrong to use this colormap, FIXME */
  gdk_colormap_alloc_color(cmap,
                           &values->appearance.fg_color,
                           FALSE, TRUE);

  gdk_colormap_alloc_color(cmap,
                           &values->appearance.bg_color,
                           FALSE, TRUE);

  values->realized = TRUE;
}

void
gtk_text_attributes_unrealize(GtkTextAttributes *values,
                                GdkColormap *cmap,
                                GdkVisual *visual)
{
  g_return_if_fail(values != NULL);
  g_return_if_fail(values->refcount > 0);
  g_return_if_fail(values->realized);
  
  gdk_colormap_free_colors(cmap,
                           &values->appearance.fg_color, 1);
  
  
  gdk_colormap_free_colors(cmap,
                           &values->appearance.bg_color, 1);

  values->appearance.fg_color.pixel = 0;
  values->appearance.bg_color.pixel = 0;

  values->realized = FALSE;
}

void
gtk_text_attributes_fill_from_tags(GtkTextAttributes *dest,
                                     GtkTextTag**        tags,
                                     guint               n_tags)
{
  guint n = 0;

  g_return_if_fail(!dest->realized);  
  
  while (n < n_tags)
    {
      GtkTextTag *tag = tags[n];
      GtkTextAttributes *vals = tag->values;

      if (n > 0)
        g_assert(tags[n]->priority > tags[n-1]->priority);
      
      if (tag->bg_color_set)
        {
          dest->appearance.bg_color = vals->appearance.bg_color;
          
          dest->appearance.draw_bg = TRUE;
        }

      if (tag->bg_stipple_set)
        {
          gdk_bitmap_ref(vals->appearance.bg_stipple);
          if (dest->appearance.bg_stipple)
            gdk_bitmap_unref(dest->appearance.bg_stipple);
          dest->appearance.bg_stipple = vals->appearance.bg_stipple;

          dest->appearance.draw_bg = TRUE;
        }

      if (tag->fg_color_set)
        dest->appearance.fg_color = vals->appearance.fg_color;
         
      if (tag->font_set)
        {
          if (dest->font_desc)
            pango_font_description_free (dest->font_desc);
          dest->font_desc = pango_font_description_copy (vals->font_desc);
        }

      if (tag->fg_stipple_set)
        {
          gdk_bitmap_ref(vals->appearance.fg_stipple);
          if (dest->appearance.fg_stipple)
            gdk_bitmap_unref(dest->appearance.fg_stipple);
          dest->appearance.fg_stipple = vals->appearance.fg_stipple;
        }

      if (tag->justify_set)
        dest->justify = vals->justify;

      if (vals->direction != GTK_TEXT_DIR_NONE)
        dest->direction = vals->direction;
      
      if (tag->left_margin_set)
        dest->left_margin = vals->left_margin;

      if (tag->left_wrapped_line_margin_set)
        dest->left_wrapped_line_margin = vals->left_wrapped_line_margin;

      if (tag->offset_set)
        dest->offset = vals->offset;

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
gtk_text_tag_affects_size (GtkTextTag *tag)
{
  g_return_val_if_fail (GTK_IS_TEXT_TAG (tag), FALSE);

  return 
    tag->font_set ||
    tag->justify_set ||
    tag->left_margin_set ||
    tag->left_wrapped_line_margin_set ||
    tag->offset_set ||
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
gtk_text_tag_affects_nonsize_appearance (GtkTextTag *tag)
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
