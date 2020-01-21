/* ATK - The Accessibility Toolkit for GTK+
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __ATK_TEXT_H__
#define __ATK_TEXT_H__

#if defined(ATK_DISABLE_SINGLE_INCLUDES) && !defined (__ATK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <atk/atk.h> can be included directly."
#endif

#include <glib-object.h>
#include <atk/atkobject.h>
#include <atk/atkutil.h>
#include <atk/atkcomponent.h>

G_BEGIN_DECLS

GDK_AVAILABLE_IN_ALL
AtkTextAttribute         atk_text_attribute_register   (const gchar *name);


#define ATK_TYPE_TEXT                    (atk_text_get_type ())
#define ATK_IS_TEXT(obj)                 G_TYPE_CHECK_INSTANCE_TYPE ((obj), ATK_TYPE_TEXT)
#define ATK_TEXT(obj)                    G_TYPE_CHECK_INSTANCE_CAST ((obj), ATK_TYPE_TEXT, AtkText)
#define ATK_TEXT_GET_IFACE(obj)          (G_TYPE_INSTANCE_GET_INTERFACE ((obj), ATK_TYPE_TEXT, AtkTextIface))

#ifndef _TYPEDEF_ATK_TEXT_
#define _TYPEDEF_ATK_TEXT_
typedef struct _AtkText AtkText;
#endif
typedef struct _AtkTextIface AtkTextIface;


/**
 * AtkTextRectangle:
 * @x: The horizontal coordinate of a rectangle
 * @y: The vertical coordinate of a rectangle
 * @width: The width of a rectangle
 * @height: The height of a rectangle
 *
 * A structure used to store a rectangle used by AtkText.
 **/

typedef struct _AtkTextRectangle AtkTextRectangle;

struct _AtkTextRectangle {
  gint x;
  gint y;
  gint width;
  gint height;
};

/**
 * AtkTextRange:
 * @bounds: A rectangle giving the bounds of the text range
 * @start_offset: The start offset of a AtkTextRange
 * @end_offset: The end offset of a AtkTextRange
 * @content: The text in the text range
 *
 * A structure used to describe a text range.
 **/
typedef struct _AtkTextRange AtkTextRange;

struct _AtkTextRange {
  AtkTextRectangle bounds;
  gint start_offset;
  gint end_offset;
  gchar* content;
};

GDK_AVAILABLE_IN_ALL
GType atk_text_range_get_type (void);

/**
 * AtkTextIface:
 * @get_text_after_offset: Gets specified text. This virtual function
 *   is deprecated and it should not be overridden.
 * @get_text_at_offset: Gets specified text. This virtual function
 *   is deprecated and it should not be overridden.
 * @get_text_before_offset: Gets specified text. This virtual function
 *   is deprecated and it should not be overridden.
 * @get_string_at_offset: Gets a portion of the text exposed through
 *   an AtkText according to a given offset and a specific
 *   granularity, along with the start and end offsets defining the
 *   boundaries of such a portion of text.
 * @text_changed: the signal handler which is executed when there is a
 *   text change. This virtual function is deprecated sice 2.9.4 and
 *   it should not be overriden.
 */
struct _AtkTextIface
{
  GTypeInterface parent;

  gchar*         (* get_text)                     (AtkText          *text,
                                                   gint             start_offset,
                                                   gint             end_offset);
  gchar*         (* get_text_after_offset)        (AtkText          *text,
                                                   gint             offset,
                                                   AtkTextBoundary  boundary_type,
						   gint             *start_offset,
						   gint             *end_offset);
  gchar*         (* get_text_at_offset)           (AtkText          *text,
                                                   gint             offset,
                                                   AtkTextBoundary  boundary_type,
						   gint             *start_offset,
						   gint             *end_offset);
  gunichar       (* get_character_at_offset)      (AtkText          *text,
                                                   gint             offset);
  gchar*         (* get_text_before_offset)       (AtkText          *text,
                                                   gint             offset,
                                                   AtkTextBoundary  boundary_type,
 						   gint             *start_offset,
						   gint             *end_offset);
  gint           (* get_caret_offset)             (AtkText          *text);
  AtkAttributeSet* (* get_run_attributes)         (AtkText	    *text,
						   gint	  	    offset,
						   gint             *start_offset,
						   gint	 	    *end_offset);
  AtkAttributeSet* (* get_default_attributes)     (AtkText	    *text);
  void           (* get_character_extents)        (AtkText          *text,
                                                   gint             offset,
                                                   gint             *x,
                                                   gint             *y,
                                                   gint             *width,
                                                   gint             *height,
                                                   AtkCoordType	    coords);
  gint           (* get_character_count)          (AtkText          *text);
  gint           (* get_offset_at_point)          (AtkText          *text,
                                                   gint             x,
                                                   gint             y,
                                                   AtkCoordType	    coords);
  gint		 (* get_n_selections)		  (AtkText          *text);
  gchar*         (* get_selection)	          (AtkText          *text,
						   gint		    selection_num,
						   gint		    *start_offset,
						   gint		    *end_offset);
  gboolean       (* add_selection)		  (AtkText          *text,
						   gint		    start_offset,
						   gint		    end_offset);
  gboolean       (* remove_selection)		  (AtkText          *text,
						   gint             selection_num);
  gboolean       (* set_selection)		  (AtkText          *text,
						   gint		    selection_num,
						   gint		    start_offset,
						   gint		    end_offset);
  gboolean       (* set_caret_offset)             (AtkText          *text,
                                                   gint             offset);

  /*
   * signal handlers
   */
  void		 (* text_changed)                 (AtkText          *text,
                                                   gint             position,
                                                   gint             length);
  void           (* text_caret_moved)             (AtkText          *text,
                                                   gint             location);
  void           (* text_selection_changed)       (AtkText          *text);

  void           (* text_attributes_changed)      (AtkText          *text);


  void           (* get_range_extents)            (AtkText          *text,
                                                   gint             start_offset,
                                                   gint             end_offset,
                                                   AtkCoordType     coord_type,
                                                   AtkTextRectangle *rect);

  AtkTextRange** (* get_bounded_ranges)           (AtkText          *text,
                                                   AtkTextRectangle *rect,
                                                   AtkCoordType     coord_type,
                                                   AtkTextClipType  x_clip_type,
                                                   AtkTextClipType  y_clip_type);

  gchar*         (* get_string_at_offset)         (AtkText            *text,
                                                   gint               offset,
                                                   AtkTextGranularity granularity,
                                                   gint               *start_offset,
                                                   gint               *end_offset);
  /*
   * Scrolls this text range so it becomes visible on the screen.
   *
   * scroll_substring_to lets the implementation compute an appropriate target
   * position on the screen, with type used as a positioning hint.
   *
   * scroll_substring_to_point lets the client specify a precise target position
   * on the screen for the top-left of the substring.
   *
   * Since ATK 2.32
   */
  gboolean       (* scroll_substring_to)          (AtkText          *text,
                                                   gint             start_offset,
                                                   gint             end_offset,
                                                   AtkScrollType    type);
  gboolean       (* scroll_substring_to_point)    (AtkText          *text,
                                                   gint             start_offset,
                                                   gint             end_offset,
                                                   AtkCoordType     coords,
                                                   gint             x,
                                                   gint             y);
};

GDK_AVAILABLE_IN_ALL
GType            atk_text_get_type (void);


/*
 * Additional AtkObject properties used by AtkText:
 *    "accessible_text" (accessible text has changed)
 *    "accessible_caret" (accessible text cursor position changed:
 *                         editable text only)
 */

GDK_AVAILABLE_IN_ALL
gchar*        atk_text_get_text                           (AtkText          *text,
                                                           gint             start_offset,
                                                           gint             end_offset);
GDK_AVAILABLE_IN_ALL
gunichar      atk_text_get_character_at_offset            (AtkText          *text,
                                                           gint             offset);
GDK_AVAILABLE_IN_ALL
gchar*        atk_text_get_string_at_offset               (AtkText            *text,
                                                           gint               offset,
                                                           AtkTextGranularity granularity,
                                                           gint               *start_offset,
                                                           gint               *end_offset);
GDK_AVAILABLE_IN_ALL
gint          atk_text_get_caret_offset                   (AtkText          *text);
GDK_AVAILABLE_IN_ALL
void          atk_text_get_character_extents              (AtkText          *text,
                                                           gint             offset,
                                                           gint             *x,
                                                           gint             *y,
                                                           gint             *width,
                                                           gint             *height,
                                                           AtkCoordType	    coords);
GDK_AVAILABLE_IN_ALL
AtkAttributeSet* atk_text_get_run_attributes              (AtkText	    *text,
						           gint	  	    offset,
						           gint             *start_offset,
						           gint	 	    *end_offset);
GDK_AVAILABLE_IN_ALL
AtkAttributeSet* atk_text_get_default_attributes          (AtkText	    *text);
GDK_AVAILABLE_IN_ALL
gint          atk_text_get_character_count                (AtkText          *text);
GDK_AVAILABLE_IN_ALL
gint          atk_text_get_offset_at_point                (AtkText          *text,
                                                           gint             x,
                                                           gint             y,
                                                           AtkCoordType	    coords);
GDK_AVAILABLE_IN_ALL
gint          atk_text_get_n_selections			  (AtkText          *text);
GDK_AVAILABLE_IN_ALL
gchar*        atk_text_get_selection			  (AtkText          *text,
							   gint		    selection_num,
							   gint             *start_offset,
							   gint             *end_offset);
GDK_AVAILABLE_IN_ALL
gboolean      atk_text_add_selection                      (AtkText          *text,
							   gint             start_offset,
							   gint             end_offset);
GDK_AVAILABLE_IN_ALL
gboolean      atk_text_remove_selection                   (AtkText          *text,
							   gint		    selection_num);
GDK_AVAILABLE_IN_ALL
gboolean      atk_text_set_selection                      (AtkText          *text,
							   gint		    selection_num,
							   gint             start_offset,
							   gint             end_offset);
GDK_AVAILABLE_IN_ALL
gboolean      atk_text_set_caret_offset                   (AtkText          *text,
                                                           gint             offset);
GDK_AVAILABLE_IN_ALL
void          atk_text_get_range_extents                  (AtkText          *text,

                                                           gint             start_offset,
                                                           gint             end_offset,
                                                           AtkCoordType     coord_type,
                                                           AtkTextRectangle *rect);
GDK_AVAILABLE_IN_ALL
AtkTextRange**  atk_text_get_bounded_ranges               (AtkText          *text,
                                                           AtkTextRectangle *rect,
                                                           AtkCoordType     coord_type,
                                                           AtkTextClipType  x_clip_type,
                                                           AtkTextClipType  y_clip_type);
GDK_AVAILABLE_IN_ALL
void          atk_text_free_ranges                        (AtkTextRange     **ranges);
GDK_AVAILABLE_IN_ALL
void 	      atk_attribute_set_free                      (AtkAttributeSet  *attrib_set);
GDK_AVAILABLE_IN_ALL
const gchar*  atk_text_attribute_get_name                 (AtkTextAttribute attr);
GDK_AVAILABLE_IN_ALL
AtkTextAttribute       atk_text_attribute_for_name        (const gchar      *name);
GDK_AVAILABLE_IN_ALL
const gchar*  atk_text_attribute_get_value                (AtkTextAttribute attr,
                                                           gint             index_);

GDK_AVAILABLE_IN_ALL
gboolean      atk_text_scroll_substring_to                (AtkText          *text,
                                                           gint             start_offset,
                                                           gint             end_offset,
                                                           AtkScrollType    type);

GDK_AVAILABLE_IN_ALL
gboolean      atk_text_scroll_substring_to_point          (AtkText          *text,
                                                           gint             start_offset,
                                                           gint             end_offset,
                                                           AtkCoordType     coords,
                                                           gint             x,
                                                           gint             y);

G_END_DECLS

#endif /* __ATK_TEXT_H__ */
