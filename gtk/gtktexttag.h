#ifndef __GTK_TEXT_TAG_H__
#define __GTK_TEXT_TAG_H__

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtkenums.h>

/* Not needed, retained for compatibility -Yosh */
#include <gtk/gtkobject.h>

G_BEGIN_DECLS

typedef struct _GtkTextIter GtkTextIter;
typedef struct _GtkTextTagTable GtkTextTagTable;

typedef struct _GtkTextAttributes GtkTextAttributes;

#define GTK_TYPE_TEXT_TAG            (gtk_text_tag_get_type ())
#define GTK_TEXT_TAG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TEXT_TAG, GtkTextTag))
#define GTK_TEXT_TAG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_TAG, GtkTextTagClass))
#define GTK_IS_TEXT_TAG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT_TAG))
#define GTK_IS_TEXT_TAG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_TAG))
#define GTK_TEXT_TAG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_TAG, GtkTextTagClass))

#define GTK_TYPE_TEXT_ATTRIBUTES     (gtk_text_attributes_get_type ())

typedef struct _GtkTextTag GtkTextTag;
typedef struct _GtkTextTagClass GtkTextTagClass;

struct _GtkTextTag
{
  GObject parent_instance;

  GtkTextTagTable *table;

  char *name;                   /* Name of this tag.  This field is actually
                                 * a pointer to the key from the entry in
                                 * tkxt->tagTable, so it needn't be freed
                                 * explicitly. */
  int priority;         /* Priority of this tag within widget.  0
                         * means lowest priority.  Exactly one tag
                         * has each integer value between 0 and
                         * numTags-1. */
  /*
   * Information for displaying text with this tag.  The information
   * belows acts as an override on information specified by lower-priority
   * tags.  If no value is specified, then the next-lower-priority tag
   * on the text determins the value.  The text widget itself provides
   * defaults if no tag specifies an override.
   */

  GtkTextAttributes *values;
  
  /* Flags for whether a given value is set; if a value is unset, then
   * this tag does not affect it.
   */
  guint bg_color_set : 1;
  guint bg_stipple_set : 1;
  guint fg_color_set : 1;
  guint scale_set : 1;
  guint fg_stipple_set : 1;
  guint justification_set : 1;
  guint left_margin_set : 1;
  guint indent_set : 1;
  guint rise_set : 1;
  guint strikethrough_set : 1;
  guint right_margin_set : 1;
  guint pixels_above_lines_set : 1;
  guint pixels_below_lines_set : 1;
  guint pixels_inside_wrap_set : 1;
  guint tabs_set : 1;
  guint underline_set : 1;
  guint wrap_mode_set : 1;
  guint bg_full_height_set : 1;
  guint invisible_set : 1;
  guint editable_set : 1;
  guint language_set : 1;
  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
};

struct _GtkTextTagClass
{
  GObjectClass parent_class;

  gboolean (* event) (GtkTextTag        *tag,
                      GObject           *event_object, /* widget, canvas item, whatever */
                      GdkEvent          *event,        /* the event itself */
                      const GtkTextIter *iter);        /* location of event in buffer */

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType        gtk_text_tag_get_type     (void) G_GNUC_CONST;
GtkTextTag  *gtk_text_tag_new          (const gchar       *name);
gint         gtk_text_tag_get_priority (GtkTextTag        *tag);
void         gtk_text_tag_set_priority (GtkTextTag        *tag,
                                        gint               priority);
gboolean     gtk_text_tag_event        (GtkTextTag        *tag,
                                        GObject           *event_object,
                                        GdkEvent          *event,
                                        const GtkTextIter *iter);

/*
 * Style object created by folding a set of tags together
 */

typedef struct _GtkTextAppearance GtkTextAppearance;

struct _GtkTextAppearance
{
  /*< public >*/
  GdkColor bg_color;
  GdkColor fg_color;
  GdkBitmap *bg_stipple;
  GdkBitmap *fg_stipple;

  /* super/subscript rise, can be negative */
  gint rise;

  /*< private >*/
  /* I'm not sure this can really be used without breaking some things
   * an app might do :-/
   */
  gpointer padding1;

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

  /*< private >*/
  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
  guint pad4 : 1;
};

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

  gint indent;  

  gint right_margin;

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
  /* I'm not sure this can really be used without breaking some things
   * an app might do :-/
   */
  gpointer padding1;

  /*< public >*/
  /* hide the text  */
  guint invisible : 1;

  /* Background is fit to full line height rather than
   * baseline +/- ascent/descent (font height)
   */
  guint bg_full_height : 1;

  /* can edit this text */
  guint editable : 1;

  /* colors are allocated etc. */
  guint realized : 1;

  /*< private >*/
  guint pad1 : 1;
  guint pad2 : 1;
  guint pad3 : 1;
  guint pad4 : 1;
};

GtkTextAttributes* gtk_text_attributes_new         (void);
GtkTextAttributes* gtk_text_attributes_copy        (GtkTextAttributes *src);
void               gtk_text_attributes_copy_values (GtkTextAttributes *src,
                                                    GtkTextAttributes *dest);
void               gtk_text_attributes_unref       (GtkTextAttributes *values);
void               gtk_text_attributes_ref         (GtkTextAttributes *values);

GType              gtk_text_attributes_get_type    (void) G_GNUC_CONST;


G_END_DECLS

#endif

