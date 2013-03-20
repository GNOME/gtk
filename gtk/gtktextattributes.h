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

#ifndef __GTK_TEXT_ATTRIBUTES_H__
#define __GTK_TEXT_ATTRIBUTES_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkenums.h>


G_BEGIN_DECLS

typedef struct _GtkTextAttributes GtkTextAttributes;

#define GTK_TYPE_TEXT_ATTRIBUTES     (gtk_text_attributes_get_type ())

typedef struct _GtkTextAppearance GtkTextAppearance;

struct _GtkTextAppearance
{
  /*< public >*/
  GdkColor bg_color;
  GdkColor fg_color;

  /* super/subscript rise, can be negative */
  gint rise;

  /*< public >*/
  guint underline : 4;          /* PangoUnderline */
  guint strikethrough : 1;

  /* Whether to use background-related values; this is irrelevant for
   * the values struct when in a tag, but is used for the composite
   * values struct; it's true if any of the tags being composited
   * had background stuff set.
   */
  guint draw_bg : 1;

  /* These are only used when we are actually laying out and rendering
   * a paragraph; not when a GtkTextAppearance is part of a
   * GtkTextAttributes.
   */
  guint inside_selection : 1;
  guint is_text : 1;

  GdkRGBA *rgba[2];

#if (defined(__SIZEOF_INT__) && defined(__SIZEOF_POINTER__)) && (__SIZEOF_INT__ == __SIZEOF_POINTER__)
  /* unusable, just for ABI compat */
  guint padding[2];
#endif
};

/**
 * GtkTextAttributes:
 *
 * Using #GtkTextAttributes directly should rarely be necessary.
 * It's primarily useful with gtk_text_iter_get_attributes().
 * As with most GTK+ structs, the fields in this struct should only
 * be read, never modified directly.
 */
struct _GtkTextAttributes
{
  /*< private >*/
  guint refcount;

  /*< public >*/
  GtkTextAppearance appearance;

  GtkJustification justification;
  GtkTextDirection direction;

  /* Individual chunks of this can be set/unset as a group */
  PangoFontDescription *font;

  gdouble font_scale;

  gint left_margin;
  gint right_margin;
  gint indent;

  gint pixels_above_lines;
  gint pixels_below_lines;
  gint pixels_inside_wrap;

  PangoTabArray *tabs;

  GtkWrapMode wrap_mode;        /* How to handle wrap-around for this tag.
                                 * Must be GTK_WRAPMODE_CHAR,
                                 * GTK_WRAPMODE_NONE, GTK_WRAPMODE_WORD
                                 */

  PangoLanguage *language;

  /*< private >*/
  /* Paragraph background */
  GdkColor *pg_bg_color;

  /*< public >*/
  /* hide the text  */
  guint invisible : 1;

  /* Background is fit to full line height rather than
   * baseline +/- ascent/descent (font height)
   */
  guint bg_full_height : 1;

  /* can edit this text */
  guint editable : 1;

  /*< private >*/
  /* Paragraph background */
  GdkRGBA *pg_bg_rgba;

  guint padding[3];
};

GDK_AVAILABLE_IN_ALL
GtkTextAttributes* gtk_text_attributes_new         (void);
GDK_AVAILABLE_IN_ALL
GtkTextAttributes* gtk_text_attributes_copy        (GtkTextAttributes *src);
GDK_AVAILABLE_IN_ALL
void               gtk_text_attributes_copy_values (GtkTextAttributes *src,
                                                    GtkTextAttributes *dest);
GDK_AVAILABLE_IN_ALL
void               gtk_text_attributes_unref       (GtkTextAttributes *values);
GDK_AVAILABLE_IN_ALL
GtkTextAttributes *gtk_text_attributes_ref         (GtkTextAttributes *values);

GDK_AVAILABLE_IN_ALL
GType              gtk_text_attributes_get_type    (void) G_GNUC_CONST;


G_END_DECLS

#endif

