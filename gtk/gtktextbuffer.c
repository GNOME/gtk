/* GTK - The GIMP Toolkit
 * gtktextbuffer.c Copyright (C) 2000 Red Hat, Inc.
 *                 Copyright (C) 2004 Nokia Corporation
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <string.h>
#include <stdarg.h>

#include "gtkdnd.h"
#include "gtkmarshalers.h"
#include "gtktextbuffer.h"
#include "gtktextbufferprivate.h"
#include "gtktextbtree.h"
#include "gtktextiterprivate.h"
#include "gtktexttagprivate.h"
#include "gtktexttagtableprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtktextbuffer
 * @Short_description: Stores attributed text for display in a GtkTextView
 * @Title: GtkTextBuffer
 * @See_also: #GtkTextView, #GtkTextIter, #GtkTextMark
 *
 * You may wish to begin by reading the
 * [text widget conceptual overview][TextWidget]
 * which gives an overview of all the objects and data
 * types related to the text widget and how they work together.
 */

typedef struct _GtkTextLogAttrCache GtkTextLogAttrCache;

struct _GtkTextBufferPrivate
{
  GtkTextTagTable *tag_table;
  GtkTextBTree *btree;

  GSList *selection_clipboards;
  GdkContentProvider *selection_content;

  GtkTextLogAttrCache *log_attr_cache;

  guint user_action_count;

  /* Whether the buffer has been modified since last save */
  guint modified : 1;
  guint has_selection : 1;
};

typedef struct _ClipboardRequest ClipboardRequest;

struct _ClipboardRequest
{
  GtkTextBuffer *buffer;
  guint interactive : 1;
  guint default_editable : 1;
  guint replace_selection : 1;
};

enum {
  INSERT_TEXT,
  INSERT_TEXTURE,
  INSERT_CHILD_ANCHOR,
  DELETE_RANGE,
  CHANGED,
  MODIFIED_CHANGED,
  MARK_SET,
  MARK_DELETED,
  APPLY_TAG,
  REMOVE_TAG,
  BEGIN_USER_ACTION,
  END_USER_ACTION,
  PASTE_DONE,
  LAST_SIGNAL
};

enum {
  PROP_0,

  /* Construct */
  PROP_TAG_TABLE,

  /* Normal */
  PROP_TEXT,
  PROP_HAS_SELECTION,
  PROP_CURSOR_POSITION,
  PROP_COPY_TARGET_LIST,
  PROP_PASTE_TARGET_LIST,
  LAST_PROP
};

static void gtk_text_buffer_finalize   (GObject            *object);

static void gtk_text_buffer_real_insert_text           (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *iter,
                                                        const gchar       *text,
                                                        gint               len);
static void gtk_text_buffer_real_insert_texture        (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *iter,
                                                        GdkTexture        *texture);
static void gtk_text_buffer_real_insert_anchor         (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *iter,
                                                        GtkTextChildAnchor *anchor);
static void gtk_text_buffer_real_delete_range          (GtkTextBuffer     *buffer,
                                                        GtkTextIter       *start,
                                                        GtkTextIter       *end);
static void gtk_text_buffer_real_apply_tag             (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);
static void gtk_text_buffer_real_remove_tag            (GtkTextBuffer     *buffer,
                                                        GtkTextTag        *tag,
                                                        const GtkTextIter *start_char,
                                                        const GtkTextIter *end_char);
static void gtk_text_buffer_real_changed               (GtkTextBuffer     *buffer);
static void gtk_text_buffer_real_mark_set              (GtkTextBuffer     *buffer,
                                                        const GtkTextIter *iter,
                                                        GtkTextMark       *mark);

static GtkTextBTree* get_btree (GtkTextBuffer *buffer);
static void          free_log_attr_cache (GtkTextLogAttrCache *cache);

static void remove_all_selection_clipboards       (GtkTextBuffer *buffer);
static void update_selection_clipboards           (GtkTextBuffer *buffer);

static void gtk_text_buffer_set_property (GObject         *object,
				          guint            prop_id,
				          const GValue    *value,
				          GParamSpec      *pspec);
static void gtk_text_buffer_get_property (GObject         *object,
				          guint            prop_id,
				          GValue          *value,
				          GParamSpec      *pspec);

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *text_buffer_props[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (GtkTextBuffer, gtk_text_buffer, G_TYPE_OBJECT)

#define GTK_TYPE_TEXT_BUFFER_CONTENT            (gtk_text_buffer_content_get_type ())
#define GTK_TEXT_BUFFER_CONTENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TEXT_BUFFER_CONTENT, GtkTextBufferContent))
#define GTK_IS_TEXT_BUFFER_CONTENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TEXT_BUFFER_CONTENT))
#define GTK_TEXT_BUFFER_CONTENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_BUFFER_CONTENT, GtkTextBufferContentClass))
#define GTK_IS_TEXT_BUFFER_CONTENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_BUFFER_CONTENT))
#define GTK_TEXT_BUFFER_CONTENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_BUFFER_CONTENT, GtkTextBufferContentClass))

typedef struct _GtkTextBufferContent GtkTextBufferContent;
typedef struct _GtkTextBufferContentClass GtkTextBufferContentClass;

struct _GtkTextBufferContent
{
  GdkContentProvider parent;

  GtkTextBuffer *text_buffer;
};

struct _GtkTextBufferContentClass
{
  GdkContentProviderClass parent_class;
};

GType gtk_text_buffer_content_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (GtkTextBufferContent, gtk_text_buffer_content, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
gtk_text_buffer_content_ref_formats (GdkContentProvider *provider)
{
  GtkTextBufferContent *content = GTK_TEXT_BUFFER_CONTENT (provider);

  if (content->text_buffer)
    return gdk_content_formats_new_for_gtype (GTK_TYPE_TEXT_BUFFER);
  else
    return gdk_content_formats_new (NULL, 0);
}

static gboolean
gtk_text_buffer_content_get_value (GdkContentProvider  *provider,
                                   GValue              *value,
                                   GError             **error)
{
  GtkTextBufferContent *content = GTK_TEXT_BUFFER_CONTENT (provider);

  if (G_VALUE_HOLDS (value, GTK_TYPE_TEXT_BUFFER) &&
      content->text_buffer != NULL)
    {
      g_value_set_object (value, content->text_buffer);
      return TRUE;
    }

  return GDK_CONTENT_PROVIDER_CLASS (gtk_text_buffer_content_parent_class)->get_value (provider, value, error);
}

static void
gtk_text_buffer_content_detach (GdkContentProvider *provider,
                                GdkClipboard       *clipboard)
{
  GtkTextBufferContent *content = GTK_TEXT_BUFFER_CONTENT (provider);
  GtkTextIter insert;
  GtkTextIter selection_bound;

  if (content->text_buffer == NULL)
    return;

  /* Move selection_bound to the insertion point */
  gtk_text_buffer_get_iter_at_mark (content->text_buffer, &insert,
                                    gtk_text_buffer_get_insert (content->text_buffer));
  gtk_text_buffer_get_iter_at_mark (content->text_buffer, &selection_bound,
                                    gtk_text_buffer_get_selection_bound (content->text_buffer));

  if (!gtk_text_iter_equal (&insert, &selection_bound))
    gtk_text_buffer_move_mark (content->text_buffer,
                               gtk_text_buffer_get_selection_bound (content->text_buffer),
                               &insert);
}

static void
gtk_text_buffer_content_class_init (GtkTextBufferContentClass *class)
{
  GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS (class);

  provider_class->ref_formats = gtk_text_buffer_content_ref_formats;
  provider_class->get_value = gtk_text_buffer_content_get_value;
  provider_class->detach_clipboard = gtk_text_buffer_content_detach;
}

static void
gtk_text_buffer_content_init (GtkTextBufferContent *content)
{
}

static GdkContentProvider *
gtk_text_buffer_content_new (GtkTextBuffer *buffer)
{
  GtkTextBufferContent *content;

  content = g_object_new (GTK_TYPE_TEXT_BUFFER_CONTENT, NULL);
  content->text_buffer = g_object_ref (buffer);
  
  return GDK_CONTENT_PROVIDER (content);
}

static void
gtk_text_buffer_deserialize_text_plain_finish (GObject      *source,
                                               GAsyncResult *result,
                                               gpointer      deserializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  GError *error = NULL;
  gssize written;

  written = g_output_stream_splice_finish (stream, result, &error);
  if (written < 0)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }

  buffer = g_value_get_object (gdk_content_deserializer_get_value (deserializer));
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_insert (buffer, 
                          &end,
                          g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (
                               g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (stream)))),
                          -1);
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  gtk_text_buffer_select_range (buffer, &start, &end);

  gdk_content_deserializer_return_success (deserializer);
}

static void
gtk_text_buffer_deserialize_text_plain (GdkContentDeserializer *deserializer)
{
  GOutputStream *output, *filter;
  GCharsetConverter *converter;
  GtkTextBuffer *buffer;
  GError *error = NULL;

  buffer = gtk_text_buffer_new (NULL);
  g_value_set_object (gdk_content_deserializer_get_value (deserializer),
                      buffer);

  /* validates the stream */
  converter = g_charset_converter_new ("utf-8", "utf-8", &error);
  if (converter == NULL)
    {
      gdk_content_deserializer_return_error (deserializer, error);
      return;
    }
  g_charset_converter_set_use_fallback (converter, TRUE);

  output = g_memory_output_stream_new_resizable ();
  filter = g_converter_output_stream_new (output, G_CONVERTER (converter));
  g_object_unref (output);
  g_object_unref (converter);

  g_output_stream_splice_async (filter,
                                gdk_content_deserializer_get_input_stream (deserializer),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                gdk_content_deserializer_get_priority (deserializer),
                                gdk_content_deserializer_get_cancellable (deserializer),
                                gtk_text_buffer_deserialize_text_plain_finish,
                                deserializer);
  g_object_unref (filter);
}

static void
gtk_text_buffer_serialize_text_plain_finish (GObject      *source,
                                             GAsyncResult *result,
                                             gpointer      serializer)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GError *error = NULL;

  if (!g_output_stream_write_all_finish (stream, result, NULL, &error))
    gdk_content_serializer_return_error (serializer, error);
  else
    gdk_content_serializer_return_success (serializer);
}

static void
gtk_text_buffer_serialize_text_plain (GdkContentSerializer *serializer)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;
  char *str;

  buffer = g_value_get_object (gdk_content_serializer_get_value (serializer));

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      str = gtk_text_iter_get_visible_text (&start, &end);
    }
  else
    {
      str = g_strdup ("");
    }
  gdk_content_serializer_set_task_data (serializer, str, g_free);

  g_output_stream_write_all_async (gdk_content_serializer_get_output_stream (serializer),
                                   str,
                                   strlen (str) + 1,
                                   gdk_content_serializer_get_priority (serializer),
                                   gdk_content_serializer_get_cancellable (serializer),
                                   gtk_text_buffer_serialize_text_plain_finish,
                                   serializer);
}

static void
gtk_text_buffer_register_serializers (void)
{
  gdk_content_register_deserializer ("text/plain;charset=utf-8",
                                     GTK_TYPE_TEXT_BUFFER,
                                     gtk_text_buffer_deserialize_text_plain,
                                     NULL,
                                     NULL);
  gdk_content_register_serializer (GTK_TYPE_TEXT_BUFFER,
                                   "text/plain;charset=utf-8",
                                   gtk_text_buffer_serialize_text_plain,
                                   NULL,
                                   NULL);
}

static void
gtk_text_buffer_class_init (GtkTextBufferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_text_buffer_finalize;
  object_class->set_property = gtk_text_buffer_set_property;
  object_class->get_property = gtk_text_buffer_get_property;
 
  klass->insert_text = gtk_text_buffer_real_insert_text;
  klass->insert_texture = gtk_text_buffer_real_insert_texture;
  klass->insert_child_anchor = gtk_text_buffer_real_insert_anchor;
  klass->delete_range = gtk_text_buffer_real_delete_range;
  klass->apply_tag = gtk_text_buffer_real_apply_tag;
  klass->remove_tag = gtk_text_buffer_real_remove_tag;
  klass->changed = gtk_text_buffer_real_changed;
  klass->mark_set = gtk_text_buffer_real_mark_set;

  /* Construct */
  text_buffer_props[PROP_TAG_TABLE] =
      g_param_spec_object ("tag-table",
                           P_("Tag Table"),
                           P_("Text Tag Table"),
                           GTK_TYPE_TEXT_TAG_TABLE,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  /* Normal properties */

  /**
   * GtkTextBuffer:text:
   *
   * The text content of the buffer. Without child widgets and images,
   * see gtk_text_buffer_get_text() for more information.
   */
  text_buffer_props[PROP_TEXT] =
      g_param_spec_string ("text",
                           P_("Text"),
                           P_("Current text of the buffer"),
                           "",
                           GTK_PARAM_READWRITE);

  /**
   * GtkTextBuffer:has-selection:
   *
   * Whether the buffer has some text currently selected.
   */
  text_buffer_props[PROP_HAS_SELECTION] =
      g_param_spec_boolean ("has-selection",
                            P_("Has selection"),
                            P_("Whether the buffer has some text currently selected"),
                            FALSE,
                            GTK_PARAM_READABLE);

  /**
   * GtkTextBuffer:cursor-position:
   *
   * The position of the insert mark (as offset from the beginning
   * of the buffer). It is useful for getting notified when the
   * cursor moves.
   */
  text_buffer_props[PROP_CURSOR_POSITION] =
      g_param_spec_int ("cursor-position",
                        P_("Cursor position"),
                        P_("The position of the insert mark (as offset from the beginning of the buffer)"),
			0, G_MAXINT,
                        0,
                        GTK_PARAM_READABLE);

  /**
   * GtkTextBuffer:copy-target-list:
   *
   * The list of targets this buffer supports for clipboard copying
   * and as DND source.
   */
  text_buffer_props[PROP_COPY_TARGET_LIST] =
      g_param_spec_boxed ("copy-target-list",
                          P_("Copy target list"),
                          P_("The list of targets this buffer supports for clipboard copying and DND source"),
                          GDK_TYPE_CONTENT_FORMATS,
                          GTK_PARAM_READABLE);

  /**
   * GtkTextBuffer:paste-target-list:
   *
   * The list of targets this buffer supports for clipboard pasting
   * and as DND destination.
   */
  text_buffer_props[PROP_PASTE_TARGET_LIST] =
      g_param_spec_boxed ("paste-target-list",
                          P_("Paste target list"),
                          P_("The list of targets this buffer supports for clipboard pasting and DND destination"),
                          GDK_TYPE_CONTENT_FORMATS,
                          GTK_PARAM_READABLE);

  g_object_class_install_properties (object_class, LAST_PROP, text_buffer_props);

  /**
   * GtkTextBuffer::insert-text:
   * @textbuffer: the object which received the signal
   * @location: position to insert @text in @textbuffer
   * @text: the UTF-8 text to be inserted
   * @len: length of the inserted text in bytes
   * 
   * The ::insert-text signal is emitted to insert text in a #GtkTextBuffer.
   * Insertion actually occurs in the default handler.  
   * 
   * Note that if your handler runs before the default handler it must not 
   * invalidate the @location iter (or has to revalidate it). 
   * The default signal handler revalidates it to point to the end of the 
   * inserted text.
   * 
   * See also: 
   * gtk_text_buffer_insert(), 
   * gtk_text_buffer_insert_range().
   */
  signals[INSERT_TEXT] =
    g_signal_new (I_("insert-text"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, insert_text),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_STRING_INT,
                  G_TYPE_NONE,
                  3,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_STRING | G_SIGNAL_TYPE_STATIC_SCOPE,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[INSERT_TEXT], G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_STRING_INTv);

  /**
   * GtkTextBuffer::insert-texture:
   * @textbuffer: the object which received the signal
   * @location: position to insert @texture in @textbuffer
   * @texture: the #GdkTexture to be inserted
   *
   * The ::insert-texture signal is emitted to insert a #GdkTexture
   * in a #GtkTextBuffer. Insertion actually occurs in the default handler.
   *
   * Note that if your handler runs before the default handler it must not
   * invalidate the @location iter (or has to revalidate it).
   * The default signal handler revalidates it to be placed after the
   * inserted @texture.
   *
   * See also: gtk_text_buffer_insert_texture().
   */
  signals[INSERT_TEXTURE] =
    g_signal_new (I_("insert-texture"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, insert_texture),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_OBJECT,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  GDK_TYPE_TEXTURE);
  g_signal_set_va_marshaller (signals[INSERT_TEXTURE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_OBJECTv);


  /**
   * GtkTextBuffer::insert-child-anchor:
   * @textbuffer: the object which received the signal
   * @location: position to insert @anchor in @textbuffer
   * @anchor: the #GtkTextChildAnchor to be inserted
   * 
   * The ::insert-child-anchor signal is emitted to insert a
   * #GtkTextChildAnchor in a #GtkTextBuffer.
   * Insertion actually occurs in the default handler.
   * 
   * Note that if your handler runs before the default handler it must
   * not invalidate the @location iter (or has to revalidate it). 
   * The default signal handler revalidates it to be placed after the 
   * inserted @anchor.
   * 
   * See also: gtk_text_buffer_insert_child_anchor().
   */
  signals[INSERT_CHILD_ANCHOR] =
    g_signal_new (I_("insert-child-anchor"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, insert_child_anchor),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_OBJECT,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  GTK_TYPE_TEXT_CHILD_ANCHOR);
  g_signal_set_va_marshaller (signals[INSERT_CHILD_ANCHOR],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_OBJECTv);
  
  /**
   * GtkTextBuffer::delete-range:
   * @textbuffer: the object which received the signal
   * @start: the start of the range to be deleted
   * @end: the end of the range to be deleted
   * 
   * The ::delete-range signal is emitted to delete a range 
   * from a #GtkTextBuffer. 
   * 
   * Note that if your handler runs before the default handler it must not 
   * invalidate the @start and @end iters (or has to revalidate them). 
   * The default signal handler revalidates the @start and @end iters to 
   * both point to the location where text was deleted. Handlers
   * which run after the default handler (see g_signal_connect_after())
   * do not have access to the deleted text.
   * 
   * See also: gtk_text_buffer_delete().
   */
  signals[DELETE_RANGE] =
    g_signal_new (I_("delete-range"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, delete_range),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_BOXED,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE,
                  GTK_TYPE_TEXT_ITER | G_SIGNAL_TYPE_STATIC_SCOPE);
  g_signal_set_va_marshaller (signals[DELETE_RANGE],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_BOXEDv);

  /**
   * GtkTextBuffer::changed:
   * @textbuffer: the object which received the signal
   * 
   * The ::changed signal is emitted when the content of a #GtkTextBuffer 
   * has changed.
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,                   
                  G_STRUCT_OFFSET (GtkTextBufferClass, changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkTextBuffer::modified-changed:
   * @textbuffer: the object which received the signal
   * 
   * The ::modified-changed signal is emitted when the modified bit of a 
   * #GtkTextBuffer flips.
   * 
   * See also:
   * gtk_text_buffer_set_modified().
   */
  signals[MODIFIED_CHANGED] =
    g_signal_new (I_("modified-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, modified_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  /**
   * GtkTextBuffer::mark-set:
   * @textbuffer: the object which received the signal
   * @location: The location of @mark in @textbuffer
   * @mark: The mark that is set
   * 
   * The ::mark-set signal is emitted as notification
   * after a #GtkTextMark is set.
   * 
   * See also: 
   * gtk_text_buffer_create_mark(),
   * gtk_text_buffer_move_mark().
   */
  signals[MARK_SET] =
    g_signal_new (I_("mark-set"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, mark_set),
                  NULL, NULL,
                  _gtk_marshal_VOID__BOXED_OBJECT,
                  G_TYPE_NONE,
                  2,
                  GTK_TYPE_TEXT_ITER,
                  GTK_TYPE_TEXT_MARK);
  g_signal_set_va_marshaller (signals[MARK_SET],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__BOXED_OBJECTv);

  /**
   * GtkTextBuffer::mark-deleted:
   * @textbuffer: the object which received the signal
   * @mark: The mark that was deleted
   * 
   * The ::mark-deleted signal is emitted as notification
   * after a #GtkTextMark is deleted. 
   * 
   * See also:
   * gtk_text_buffer_delete_mark().
   */
  signals[MARK_DELETED] =
    g_signal_new (I_("mark-deleted"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,                   
                  G_STRUCT_OFFSET (GtkTextBufferClass, mark_deleted),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_TEXT_MARK);

   /**
   * GtkTextBuffer::apply-tag:
   * @textbuffer: the object which received the signal
   * @tag: the applied tag
   * @start: the start of the range the tag is applied to
   * @end: the end of the range the tag is applied to
   * 
   * The ::apply-tag signal is emitted to apply a tag to a
   * range of text in a #GtkTextBuffer. 
   * Applying actually occurs in the default handler.
   * 
   * Note that if your handler runs before the default handler it must not 
   * invalidate the @start and @end iters (or has to revalidate them). 
   * 
   * See also: 
   * gtk_text_buffer_apply_tag(),
   * gtk_text_buffer_insert_with_tags(),
   * gtk_text_buffer_insert_range().
   */ 
  signals[APPLY_TAG] =
    g_signal_new (I_("apply-tag"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, apply_tag),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_BOXED_BOXED,
                  G_TYPE_NONE,
                  3,
                  GTK_TYPE_TEXT_TAG,
                  GTK_TYPE_TEXT_ITER,
                  GTK_TYPE_TEXT_ITER);
  g_signal_set_va_marshaller (signals[APPLY_TAG],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__OBJECT_BOXED_BOXEDv);


   /**
   * GtkTextBuffer::remove-tag:
   * @textbuffer: the object which received the signal
   * @tag: the tag to be removed
   * @start: the start of the range the tag is removed from
   * @end: the end of the range the tag is removed from
   * 
   * The ::remove-tag signal is emitted to remove all occurrences of @tag from
   * a range of text in a #GtkTextBuffer. 
   * Removal actually occurs in the default handler.
   * 
   * Note that if your handler runs before the default handler it must not 
   * invalidate the @start and @end iters (or has to revalidate them). 
   * 
   * See also: 
   * gtk_text_buffer_remove_tag(). 
   */ 
  signals[REMOVE_TAG] =
    g_signal_new (I_("remove-tag"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, remove_tag),
                  NULL, NULL,
                  _gtk_marshal_VOID__OBJECT_BOXED_BOXED,
                  G_TYPE_NONE,
                  3,
                  GTK_TYPE_TEXT_TAG,
                  GTK_TYPE_TEXT_ITER,
                  GTK_TYPE_TEXT_ITER);
  g_signal_set_va_marshaller (signals[REMOVE_TAG],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__OBJECT_BOXED_BOXEDv);

   /**
   * GtkTextBuffer::begin-user-action:
   * @textbuffer: the object which received the signal
   * 
   * The ::begin-user-action signal is emitted at the beginning of a single
   * user-visible operation on a #GtkTextBuffer.
   * 
   * See also: 
   * gtk_text_buffer_begin_user_action(),
   * gtk_text_buffer_insert_interactive(),
   * gtk_text_buffer_insert_range_interactive(),
   * gtk_text_buffer_delete_interactive(),
   * gtk_text_buffer_backspace(),
   * gtk_text_buffer_delete_selection().
   */ 
  signals[BEGIN_USER_ACTION] =
    g_signal_new (I_("begin-user-action"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,                   
                  G_STRUCT_OFFSET (GtkTextBufferClass, begin_user_action),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

   /**
   * GtkTextBuffer::end-user-action:
   * @textbuffer: the object which received the signal
   * 
   * The ::end-user-action signal is emitted at the end of a single
   * user-visible operation on the #GtkTextBuffer.
   * 
   * See also: 
   * gtk_text_buffer_end_user_action(),
   * gtk_text_buffer_insert_interactive(),
   * gtk_text_buffer_insert_range_interactive(),
   * gtk_text_buffer_delete_interactive(),
   * gtk_text_buffer_backspace(),
   * gtk_text_buffer_delete_selection(),
   * gtk_text_buffer_backspace().
   */ 
  signals[END_USER_ACTION] =
    g_signal_new (I_("end-user-action"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,                   
                  G_STRUCT_OFFSET (GtkTextBufferClass, end_user_action),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

   /**
   * GtkTextBuffer::paste-done:
   * @textbuffer: the object which received the signal
   * @clipboard: the #GdkClipboard pasted from
   * 
   * The paste-done signal is emitted after paste operation has been completed.
   * This is useful to properly scroll the view to the end of the pasted text.
   * See gtk_text_buffer_paste_clipboard() for more details.
   */ 
  signals[PASTE_DONE] =
    g_signal_new (I_("paste-done"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkTextBufferClass, paste_done),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_CLIPBOARD);

  gtk_text_buffer_register_serializers ();
}

static void
gtk_text_buffer_init (GtkTextBuffer *buffer)
{
  buffer->priv = gtk_text_buffer_get_instance_private (buffer);
  buffer->priv->tag_table = NULL;
}

static void
set_table (GtkTextBuffer *buffer, GtkTextTagTable *table)
{
  GtkTextBufferPrivate *priv = buffer->priv;

  g_return_if_fail (priv->tag_table == NULL);

  if (table)
    {
      priv->tag_table = table;
      g_object_ref (priv->tag_table);
      _gtk_text_tag_table_add_buffer (table, buffer);
    }
}

static GtkTextTagTable*
get_table (GtkTextBuffer *buffer)
{
  GtkTextBufferPrivate *priv = buffer->priv;

  if (priv->tag_table == NULL)
    {
      priv->tag_table = gtk_text_tag_table_new ();
      _gtk_text_tag_table_add_buffer (priv->tag_table, buffer);
    }

  return priv->tag_table;
}

static void
gtk_text_buffer_set_property (GObject         *object,
                              guint            prop_id,
                              const GValue    *value,
                              GParamSpec      *pspec)
{
  GtkTextBuffer *text_buffer;

  text_buffer = GTK_TEXT_BUFFER (object);

  switch (prop_id)
    {
    case PROP_TAG_TABLE:
      set_table (text_buffer, g_value_get_object (value));
      break;

    case PROP_TEXT:
      gtk_text_buffer_set_text (text_buffer,
				g_value_get_string (value), -1);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_text_buffer_get_property (GObject         *object,
                              guint            prop_id,
                              GValue          *value,
                              GParamSpec      *pspec)
{
  GtkTextBuffer *text_buffer;
  GtkTextIter iter;

  text_buffer = GTK_TEXT_BUFFER (object);

  switch (prop_id)
    {
    case PROP_TAG_TABLE:
      g_value_set_object (value, get_table (text_buffer));
      break;

    case PROP_TEXT:
      {
        GtkTextIter start, end;

        gtk_text_buffer_get_start_iter (text_buffer, &start);
        gtk_text_buffer_get_end_iter (text_buffer, &end);

        g_value_take_string (value,
                            gtk_text_buffer_get_text (text_buffer,
                                                      &start, &end, FALSE));
        break;
      }

    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, text_buffer->priv->has_selection);
      break;

    case PROP_CURSOR_POSITION:
      gtk_text_buffer_get_iter_at_mark (text_buffer, &iter, 
    				        gtk_text_buffer_get_insert (text_buffer));
      g_value_set_int (value, gtk_text_iter_get_offset (&iter));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_text_buffer_new:
 * @table: (allow-none): a tag table, or %NULL to create a new one
 *
 * Creates a new text buffer.
 *
 * Returns: a new text buffer
 **/
GtkTextBuffer*
gtk_text_buffer_new (GtkTextTagTable *table)
{
  GtkTextBuffer *text_buffer;

  text_buffer = g_object_new (GTK_TYPE_TEXT_BUFFER, "tag-table", table, NULL);

  return text_buffer;
}

static void
gtk_text_buffer_finalize (GObject *object)
{
  GtkTextBuffer *buffer;
  GtkTextBufferPrivate *priv;

  buffer = GTK_TEXT_BUFFER (object);
  priv = buffer->priv;

  remove_all_selection_clipboards (buffer);

  if (priv->tag_table)
    {
      _gtk_text_tag_table_remove_buffer (priv->tag_table, buffer);
      g_object_unref (priv->tag_table);
      priv->tag_table = NULL;
    }

  if (priv->btree)
    {
      _gtk_text_btree_unref (priv->btree);
      priv->btree = NULL;
    }

  if (priv->log_attr_cache)
    free_log_attr_cache (priv->log_attr_cache);

  priv->log_attr_cache = NULL;

  G_OBJECT_CLASS (gtk_text_buffer_parent_class)->finalize (object);
}

static GtkTextBTree*
get_btree (GtkTextBuffer *buffer)
{
  GtkTextBufferPrivate *priv = buffer->priv;

  if (priv->btree == NULL)
    priv->btree = _gtk_text_btree_new (gtk_text_buffer_get_tag_table (buffer),
                                       buffer);

  return priv->btree;
}

GtkTextBTree*
_gtk_text_buffer_get_btree (GtkTextBuffer *buffer)
{
  return get_btree (buffer);
}

/**
 * gtk_text_buffer_get_tag_table:
 * @buffer: a #GtkTextBuffer
 *
 * Get the #GtkTextTagTable associated with this buffer.
 *
 * Returns: (transfer none): the buffer’s tag table
 **/
GtkTextTagTable*
gtk_text_buffer_get_tag_table (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return get_table (buffer);
}

/**
 * gtk_text_buffer_set_text:
 * @buffer: a #GtkTextBuffer
 * @text: UTF-8 text to insert
 * @len: length of @text in bytes
 *
 * Deletes current contents of @buffer, and inserts @text instead. If
 * @len is -1, @text must be nul-terminated. @text must be valid UTF-8.
 **/
void
gtk_text_buffer_set_text (GtkTextBuffer *buffer,
                          const gchar   *text,
                          gint           len)
{
  GtkTextIter start, end;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  gtk_text_buffer_get_bounds (buffer, &start, &end);

  gtk_text_buffer_delete (buffer, &start, &end);

  if (len > 0)
    {
      gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
      gtk_text_buffer_insert (buffer, &start, text, len);
    }
}

 

/*
 * Insertion
 */

static void
gtk_text_buffer_real_insert_text (GtkTextBuffer *buffer,
                                  GtkTextIter   *iter,
                                  const gchar   *text,
                                  gint           len)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  
  _gtk_text_btree_insert (iter, text, len);

  g_signal_emit (buffer, signals[CHANGED], 0);
  g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_CURSOR_POSITION]);
}

static void
gtk_text_buffer_emit_insert (GtkTextBuffer *buffer,
                             GtkTextIter   *iter,
                             const gchar   *text,
                             gint           len)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);

  if (len < 0)
    len = strlen (text);

  g_return_if_fail (g_utf8_validate (text, len, NULL));
  
  if (len > 0)
    {
      g_signal_emit (buffer, signals[INSERT_TEXT], 0,
                     iter, text, len);
    }
}

/**
 * gtk_text_buffer_insert:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in the buffer
 * @text: text in UTF-8 format
 * @len: length of text in bytes, or -1
 *
 * Inserts @len bytes of @text at position @iter.  If @len is -1,
 * @text must be nul-terminated and will be inserted in its
 * entirety. Emits the “insert-text” signal; insertion actually occurs
 * in the default handler for the signal. @iter is invalidated when
 * insertion occurs (because the buffer contents change), but the
 * default signal handler revalidates it to point to the end of the
 * inserted text.
 **/
void
gtk_text_buffer_insert (GtkTextBuffer *buffer,
                        GtkTextIter   *iter,
                        const gchar   *text,
                        gint           len)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
  
  gtk_text_buffer_emit_insert (buffer, iter, text, len);
}

/**
 * gtk_text_buffer_insert_at_cursor:
 * @buffer: a #GtkTextBuffer
 * @text: text in UTF-8 format
 * @len: length of text, in bytes
 *
 * Simply calls gtk_text_buffer_insert(), using the current
 * cursor position as the insertion point.
 **/
void
gtk_text_buffer_insert_at_cursor (GtkTextBuffer *buffer,
                                  const gchar   *text,
                                  gint           len)
{
  GtkTextIter iter;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (text != NULL);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                    gtk_text_buffer_get_insert (buffer));

  gtk_text_buffer_insert (buffer, &iter, text, len);
}

/**
 * gtk_text_buffer_insert_interactive:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @text: some UTF-8 text
 * @len: length of text in bytes, or -1
 * @default_editable: default editability of buffer
 *
 * Like gtk_text_buffer_insert(), but the insertion will not occur if
 * @iter is at a non-editable location in the buffer. Usually you
 * want to prevent insertions at ineditable locations if the insertion
 * results from a user action (is interactive).
 *
 * @default_editable indicates the editability of text that doesn't
 * have a tag affecting editability applied to it. Typically the
 * result of gtk_text_view_get_editable() is appropriate here.
 *
 * Returns: whether text was actually inserted
 **/
gboolean
gtk_text_buffer_insert_interactive (GtkTextBuffer *buffer,
                                    GtkTextIter   *iter,
                                    const gchar   *text,
                                    gint           len,
                                    gboolean       default_editable)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (text != NULL, FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (iter) == buffer, FALSE);

  if (gtk_text_iter_can_insert (iter, default_editable))
    {
      gtk_text_buffer_begin_user_action (buffer);
      gtk_text_buffer_emit_insert (buffer, iter, text, len);
      gtk_text_buffer_end_user_action (buffer);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_buffer_insert_interactive_at_cursor:
 * @buffer: a #GtkTextBuffer
 * @text: text in UTF-8 format
 * @len: length of text in bytes, or -1
 * @default_editable: default editability of buffer
 *
 * Calls gtk_text_buffer_insert_interactive() at the cursor
 * position.
 *
 * @default_editable indicates the editability of text that doesn't
 * have a tag affecting editability applied to it. Typically the
 * result of gtk_text_view_get_editable() is appropriate here.
 * 
 * Returns: whether text was actually inserted
 **/
gboolean
gtk_text_buffer_insert_interactive_at_cursor (GtkTextBuffer *buffer,
                                              const gchar   *text,
                                              gint           len,
                                              gboolean       default_editable)
{
  GtkTextIter iter;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (text != NULL, FALSE);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter,
                                    gtk_text_buffer_get_insert (buffer));

  return gtk_text_buffer_insert_interactive (buffer, &iter, text, len,
                                             default_editable);
}

static gboolean
possibly_not_text (gunichar ch,
                   gpointer user_data)
{
  return ch == GTK_TEXT_UNKNOWN_CHAR;
}

static void
insert_text_range (GtkTextBuffer     *buffer,
                   GtkTextIter       *iter,
                   const GtkTextIter *orig_start,
                   const GtkTextIter *orig_end,
                   gboolean           interactive)
{
  gchar *text;

  text = gtk_text_iter_get_text (orig_start, orig_end);

  gtk_text_buffer_emit_insert (buffer, iter, text, -1);

  g_free (text);
}

typedef struct _Range Range;
struct _Range
{
  GtkTextBuffer *buffer;
  GtkTextMark *start_mark;
  GtkTextMark *end_mark;
  GtkTextMark *whole_end_mark;
  GtkTextIter *range_start;
  GtkTextIter *range_end;
  GtkTextIter *whole_end;
};

static Range*
save_range (GtkTextIter *range_start,
            GtkTextIter *range_end,
            GtkTextIter *whole_end)
{
  Range *r;

  r = g_slice_new (Range);

  r->buffer = gtk_text_iter_get_buffer (range_start);
  g_object_ref (r->buffer);
  
  r->start_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 range_start,
                                 FALSE);
  r->end_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 range_end,
                                 TRUE);

  r->whole_end_mark = 
    gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (range_start),
                                 NULL,
                                 whole_end,
                                 TRUE);

  r->range_start = range_start;
  r->range_end = range_end;
  r->whole_end = whole_end;

  return r;
}

static void
restore_range (Range *r)
{
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_start,
                                    r->start_mark);
      
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->range_end,
                                    r->end_mark);
      
  gtk_text_buffer_get_iter_at_mark (r->buffer,
                                    r->whole_end,
                                    r->whole_end_mark);  
  
  gtk_text_buffer_delete_mark (r->buffer, r->start_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->end_mark);
  gtk_text_buffer_delete_mark (r->buffer, r->whole_end_mark);

  /* Due to the gravities on the marks, the ordering could have
   * gotten mangled; we switch to an empty range in that
   * case
   */
  
  if (gtk_text_iter_compare (r->range_start, r->range_end) > 0)
    *r->range_start = *r->range_end;

  if (gtk_text_iter_compare (r->range_end, r->whole_end) > 0)
    *r->range_end = *r->whole_end;
  
  g_object_unref (r->buffer);
  g_slice_free (Range, r);
}

static void
insert_range_untagged (GtkTextBuffer     *buffer,
                       GtkTextIter       *iter,
                       const GtkTextIter *orig_start,
                       const GtkTextIter *orig_end,
                       gboolean           interactive)
{
  GtkTextIter range_start;
  GtkTextIter range_end;
  GtkTextIter start, end;
  Range *r;
  
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  start = *orig_start;
  end = *orig_end;
  
  range_start = start;
  range_end = start;
  
  while (TRUE)
    {
      if (gtk_text_iter_equal (&range_start, &range_end))
        {
          /* Figure out how to move forward */

          g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
          
          if (gtk_text_iter_equal (&range_end, &end))
            {
              /* nothing left to do */
              break;
            }
          else if (gtk_text_iter_get_char (&range_end) == GTK_TEXT_UNKNOWN_CHAR)
            {
              GdkTexture *texture;
              GtkTextChildAnchor *anchor;

              texture = gtk_text_iter_get_texture (&range_end);
              anchor = gtk_text_iter_get_child_anchor (&range_end);

              if (texture)
                {
                  r = save_range (&range_start,
                                  &range_end,
                                  &end);

                  gtk_text_buffer_insert_texture (buffer, iter, texture);

                  restore_range (r);
                  r = NULL;

                  gtk_text_iter_forward_char (&range_end);

                  range_start = range_end;
                }
              else if (anchor)
                {
                  /* Just skip anchors */

                  gtk_text_iter_forward_char (&range_end);
                  range_start = range_end;
                }
              else
                {
                  /* The GTK_TEXT_UNKNOWN_CHAR was in a text segment, so
                   * keep going. 
                   */
                  gtk_text_iter_forward_find_char (&range_end,
                                                   possibly_not_text, NULL,
                                                   &end);
                  
                  g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
                }
            }
          else
            {
              /* Text segment starts here, so forward search to
               * find its possible endpoint
               */
              gtk_text_iter_forward_find_char (&range_end,
                                               possibly_not_text, NULL,
                                               &end);
              
              g_assert (gtk_text_iter_compare (&range_end, &end) <= 0);
            }
        }
      else
        {
          r = save_range (&range_start,
                          &range_end,
                          &end);
          
          insert_text_range (buffer,
                             iter,
                             &range_start,
                             &range_end,
                             interactive);

          restore_range (r);
          r = NULL;
          
          range_start = range_end;
        }
    }
}

static void
insert_range_not_inside_self (GtkTextBuffer     *buffer,
                              GtkTextIter       *iter,
                              const GtkTextIter *orig_start,
                              const GtkTextIter *orig_end,
                              gboolean           interactive)
{
  /* Find each range of uniformly-tagged text, insert it,
   * then apply the tags.
   */
  GtkTextIter start = *orig_start;
  GtkTextIter end = *orig_end;
  GtkTextIter range_start;
  GtkTextIter range_end;
  
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;
  
  gtk_text_iter_order (&start, &end);

  range_start = start;
  range_end = start;  
  
  while (TRUE)
    {
      gint start_offset;
      GtkTextIter start_iter;
      GSList *tags;
      GSList *tmp_list;
      Range *r;
      
      if (gtk_text_iter_equal (&range_start, &end))
        break; /* All done */

      g_assert (gtk_text_iter_compare (&range_start, &end) < 0);
      
      gtk_text_iter_forward_to_tag_toggle (&range_end, NULL);

      g_assert (!gtk_text_iter_equal (&range_start, &range_end));

      /* Clamp to the end iterator */
      if (gtk_text_iter_compare (&range_end, &end) > 0)
        range_end = end;
      
      /* We have a range with unique tags; insert it, and
       * apply all tags.
       */
      start_offset = gtk_text_iter_get_offset (iter);

      r = save_range (&range_start, &range_end, &end);
      
      insert_range_untagged (buffer, iter, &range_start, &range_end, interactive);

      restore_range (r);
      r = NULL;
      
      gtk_text_buffer_get_iter_at_offset (buffer, &start_iter, start_offset);
      
      tags = gtk_text_iter_get_tags (&range_start);
      tmp_list = tags;
      while (tmp_list != NULL)
        {
          gtk_text_buffer_apply_tag (buffer,
                                     tmp_list->data,
                                     &start_iter,
                                     iter);

          tmp_list = tmp_list->next;
        }
      g_slist_free (tags);

      range_start = range_end;
    }
}

static void
gtk_text_buffer_real_insert_range (GtkTextBuffer     *buffer,
                                   GtkTextIter       *iter,
                                   const GtkTextIter *orig_start,
                                   const GtkTextIter *orig_end,
                                   gboolean           interactive)
{
  GtkTextBuffer *src_buffer;
  
  /* Find each range of uniformly-tagged text, insert it,
   * then apply the tags.
   */  
  if (gtk_text_iter_equal (orig_start, orig_end))
    return;

  if (interactive)
    gtk_text_buffer_begin_user_action (buffer);
  
  src_buffer = gtk_text_iter_get_buffer (orig_start);
  
  if (gtk_text_iter_get_buffer (iter) != src_buffer ||
      !gtk_text_iter_in_range (iter, orig_start, orig_end))
    {
      insert_range_not_inside_self (buffer, iter, orig_start, orig_end, interactive);
    }
  else
    {
      /* If you insert a range into itself, it could loop infinitely
       * because the region being copied keeps growing as we insert. So
       * we have to separately copy the range before and after
       * the insertion point.
       */
      GtkTextIter start = *orig_start;
      GtkTextIter end = *orig_end;
      GtkTextIter range_start;
      GtkTextIter range_end;
      Range *first_half;
      Range *second_half;

      gtk_text_iter_order (&start, &end);
      
      range_start = start;
      range_end = *iter;
      first_half = save_range (&range_start, &range_end, &end);

      range_start = *iter;
      range_end = end;
      second_half = save_range (&range_start, &range_end, &end);

      restore_range (first_half);
      insert_range_not_inside_self (buffer, iter, &range_start, &range_end, interactive);

      restore_range (second_half);
      insert_range_not_inside_self (buffer, iter, &range_start, &range_end, interactive);
    }
  
  if (interactive)
    gtk_text_buffer_end_user_action (buffer);
}

/**
 * gtk_text_buffer_insert_range:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @start: a position in a #GtkTextBuffer
 * @end: another position in the same buffer as @start
 *
 * Copies text, tags, and texture between @start and @end (the order
 * of @start and @end doesn’t matter) and inserts the copy at @iter.
 * Used instead of simply getting/inserting text because it preserves
 * images and tags. If @start and @end are in a different buffer from
 * @buffer, the two buffers must share the same tag table.
 *
 * Implemented via emissions of the insert_text and apply_tag signals,
 * so expect those.
 **/
void
gtk_text_buffer_insert_range (GtkTextBuffer     *buffer,
                              GtkTextIter       *iter,
                              const GtkTextIter *start,
                              const GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) ==
                    gtk_text_iter_get_buffer (end));
  g_return_if_fail (gtk_text_iter_get_buffer (start)->priv->tag_table ==
                    buffer->priv->tag_table);
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
  
  gtk_text_buffer_real_insert_range (buffer, iter, start, end, FALSE);
}

/**
 * gtk_text_buffer_insert_range_interactive:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @start: a position in a #GtkTextBuffer
 * @end: another position in the same buffer as @start
 * @default_editable: default editability of the buffer
 *
 * Same as gtk_text_buffer_insert_range(), but does nothing if the
 * insertion point isn’t editable. The @default_editable parameter
 * indicates whether the text is editable at @iter if no tags
 * enclosing @iter affect editability. Typically the result of
 * gtk_text_view_get_editable() is appropriate here.
 *
 * Returns: whether an insertion was possible at @iter
 **/
gboolean
gtk_text_buffer_insert_range_interactive (GtkTextBuffer     *buffer,
                                          GtkTextIter       *iter,
                                          const GtkTextIter *start,
                                          const GtkTextIter *end,
                                          gboolean           default_editable)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (start != NULL, FALSE);
  g_return_val_if_fail (end != NULL, FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start) ==
                        gtk_text_iter_get_buffer (end), FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start)->priv->tag_table ==
                        buffer->priv->tag_table, FALSE);

  if (gtk_text_iter_can_insert (iter, default_editable))
    {
      gtk_text_buffer_real_insert_range (buffer, iter, start, end, TRUE);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gtk_text_buffer_insert_with_tags:
 * @buffer: a #GtkTextBuffer
 * @iter: an iterator in @buffer
 * @text: UTF-8 text
 * @len: length of @text, or -1
 * @first_tag: first tag to apply to @text
 * @...: %NULL-terminated list of tags to apply
 *
 * Inserts @text into @buffer at @iter, applying the list of tags to
 * the newly-inserted text. The last tag specified must be %NULL to
 * terminate the list. Equivalent to calling gtk_text_buffer_insert(),
 * then gtk_text_buffer_apply_tag() on the inserted text;
 * gtk_text_buffer_insert_with_tags() is just a convenience function.
 **/
void
gtk_text_buffer_insert_with_tags (GtkTextBuffer *buffer,
                                  GtkTextIter   *iter,
                                  const gchar   *text,
                                  gint           len,
                                  GtkTextTag    *first_tag,
                                  ...)
{
  gint start_offset;
  GtkTextIter start;
  va_list args;
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
  
  start_offset = gtk_text_iter_get_offset (iter);

  gtk_text_buffer_insert (buffer, iter, text, len);

  if (first_tag == NULL)
    return;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);

  va_start (args, first_tag);
  tag = first_tag;
  while (tag)
    {
      gtk_text_buffer_apply_tag (buffer, tag, &start, iter);

      tag = va_arg (args, GtkTextTag*);
    }

  va_end (args);
}

/**
 * gtk_text_buffer_insert_with_tags_by_name:
 * @buffer: a #GtkTextBuffer
 * @iter: position in @buffer
 * @text: UTF-8 text
 * @len: length of @text, or -1
 * @first_tag_name: name of a tag to apply to @text
 * @...: more tag names
 *
 * Same as gtk_text_buffer_insert_with_tags(), but allows you
 * to pass in tag names instead of tag objects.
 **/
void
gtk_text_buffer_insert_with_tags_by_name  (GtkTextBuffer *buffer,
                                           GtkTextIter   *iter,
                                           const gchar   *text,
                                           gint           len,
                                           const gchar   *first_tag_name,
                                           ...)
{
  gint start_offset;
  GtkTextIter start;
  va_list args;
  const gchar *tag_name;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (text != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
  
  start_offset = gtk_text_iter_get_offset (iter);

  gtk_text_buffer_insert (buffer, iter, text, len);

  if (first_tag_name == NULL)
    return;

  gtk_text_buffer_get_iter_at_offset (buffer, &start, start_offset);

  va_start (args, first_tag_name);
  tag_name = first_tag_name;
  while (tag_name)
    {
      GtkTextTag *tag;

      tag = gtk_text_tag_table_lookup (buffer->priv->tag_table,
                                       tag_name);

      if (tag == NULL)
        {
          g_warning ("%s: no tag with name '%s'!", G_STRLOC, tag_name);
          va_end (args);
          return;
        }

      gtk_text_buffer_apply_tag (buffer, tag, &start, iter);

      tag_name = va_arg (args, const gchar*);
    }

  va_end (args);
}


/*
 * Deletion
 */

static void
gtk_text_buffer_real_delete_range (GtkTextBuffer *buffer,
                                   GtkTextIter   *start,
                                   GtkTextIter   *end)
{
  gboolean has_selection;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  _gtk_text_btree_delete (start, end);

  /* may have deleted the selection... */
  update_selection_clipboards (buffer);

  has_selection = gtk_text_buffer_get_selection_bounds (buffer, NULL, NULL);
  if (has_selection != buffer->priv->has_selection)
    {
      buffer->priv->has_selection = has_selection;
      g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_HAS_SELECTION]);
    }

  g_signal_emit (buffer, signals[CHANGED], 0);
  g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_CURSOR_POSITION]);
}

static void
gtk_text_buffer_emit_delete (GtkTextBuffer *buffer,
                             GtkTextIter *start,
                             GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);

  if (gtk_text_iter_equal (start, end))
    return;

  gtk_text_iter_order (start, end);

  g_signal_emit (buffer,
                 signals[DELETE_RANGE],
                 0,
                 start, end);
}

/**
 * gtk_text_buffer_delete:
 * @buffer: a #GtkTextBuffer
 * @start: a position in @buffer
 * @end: another position in @buffer
 *
 * Deletes text between @start and @end. The order of @start and @end
 * is not actually relevant; gtk_text_buffer_delete() will reorder
 * them. This function actually emits the “delete-range” signal, and
 * the default handler of that signal deletes the text. Because the
 * buffer is modified, all outstanding iterators become invalid after
 * calling this function; however, the @start and @end will be
 * re-initialized to point to the location where text was deleted.
 **/
void
gtk_text_buffer_delete (GtkTextBuffer *buffer,
                        GtkTextIter   *start,
                        GtkTextIter   *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
  
  gtk_text_buffer_emit_delete (buffer, start, end);
}

/**
 * gtk_text_buffer_delete_interactive:
 * @buffer: a #GtkTextBuffer
 * @start_iter: start of range to delete
 * @end_iter: end of range
 * @default_editable: whether the buffer is editable by default
 *
 * Deletes all editable text in the given range.
 * Calls gtk_text_buffer_delete() for each editable sub-range of
 * [@start,@end). @start and @end are revalidated to point to
 * the location of the last deleted range, or left untouched if
 * no text was deleted.
 *
 * Returns: whether some text was actually deleted
 **/
gboolean
gtk_text_buffer_delete_interactive (GtkTextBuffer *buffer,
                                    GtkTextIter   *start_iter,
                                    GtkTextIter   *end_iter,
                                    gboolean       default_editable)
{
  GtkTextMark *end_mark;
  GtkTextMark *start_mark;
  GtkTextIter iter;
  gboolean current_state;
  gboolean deleted_stuff = FALSE;

  /* Delete all editable text in the range start_iter, end_iter */

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (start_iter != NULL, FALSE);
  g_return_val_if_fail (end_iter != NULL, FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start_iter) == buffer, FALSE);
  g_return_val_if_fail (gtk_text_iter_get_buffer (end_iter) == buffer, FALSE);

  
  gtk_text_buffer_begin_user_action (buffer);
  
  gtk_text_iter_order (start_iter, end_iter);

  start_mark = gtk_text_buffer_create_mark (buffer, NULL,
                                            start_iter, TRUE);
  end_mark = gtk_text_buffer_create_mark (buffer, NULL,
                                          end_iter, FALSE);

  gtk_text_buffer_get_iter_at_mark (buffer, &iter, start_mark);

  current_state = gtk_text_iter_editable (&iter, default_editable);

  while (TRUE)
    {
      gboolean new_state;
      gboolean done = FALSE;
      GtkTextIter end;

      gtk_text_iter_forward_to_tag_toggle (&iter, NULL);

      gtk_text_buffer_get_iter_at_mark (buffer, &end, end_mark);

      if (gtk_text_iter_compare (&iter, &end) >= 0)
        {
          done = TRUE;
          iter = end; /* clamp to the last boundary */
        }

      new_state = gtk_text_iter_editable (&iter, default_editable);

      if (current_state == new_state)
        {
          if (done)
            {
              if (current_state)
                {
                  /* We're ending an editable region. Delete said region. */
                  GtkTextIter start;

                  gtk_text_buffer_get_iter_at_mark (buffer, &start, start_mark);

                  gtk_text_buffer_emit_delete (buffer, &start, &iter);

                  deleted_stuff = TRUE;

                  /* revalidate user's iterators. */
                  *start_iter = start;
                  *end_iter = iter;
                }

              break;
            }
          else
            continue;
        }

      if (current_state && !new_state)
        {
          /* End of an editable region. Delete it. */
          GtkTextIter start;

          gtk_text_buffer_get_iter_at_mark (buffer, &start, start_mark);

          gtk_text_buffer_emit_delete (buffer, &start, &iter);

	  /* It's more robust to ask for the state again then to assume that
	   * we're on the next not-editable segment. We don't know what the
	   * ::delete-range handler did.... maybe it deleted the following
           * not-editable segment because it was associated with the editable
           * segment.
	   */
	  current_state = gtk_text_iter_editable (&iter, default_editable);
          deleted_stuff = TRUE;

          /* revalidate user's iterators. */
          *start_iter = start;
          *end_iter = iter;
        }
      else
        {
          /* We are at the start of an editable region. We won't be deleting
           * the previous region. Move start mark to start of this region.
           */

          g_assert (!current_state && new_state);

          gtk_text_buffer_move_mark (buffer, start_mark, &iter);

          current_state = TRUE;
        }

      if (done)
        break;
    }

  gtk_text_buffer_delete_mark (buffer, start_mark);
  gtk_text_buffer_delete_mark (buffer, end_mark);

  gtk_text_buffer_end_user_action (buffer);
  
  return deleted_stuff;
}

/*
 * Extracting textual buffer contents
 */

/**
 * gtk_text_buffer_get_text:
 * @buffer: a #GtkTextBuffer
 * @start: start of a range
 * @end: end of a range
 * @include_hidden_chars: whether to include invisible text
 *
 * Returns the text in the range [@start,@end). Excludes undisplayed
 * text (text marked with tags that set the invisibility attribute) if
 * @include_hidden_chars is %FALSE. Does not include characters
 * representing embedded images, so byte and character indexes into
 * the returned string do not correspond to byte
 * and character indexes into the buffer. Contrast with
 * gtk_text_buffer_get_slice().
 *
 * Returns: (transfer full): an allocated UTF-8 string
 **/
gchar*
gtk_text_buffer_get_text (GtkTextBuffer     *buffer,
                          const GtkTextIter *start,
                          const GtkTextIter *end,
                          gboolean           include_hidden_chars)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start) == buffer, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (end) == buffer, NULL);
  
  if (include_hidden_chars)
    return gtk_text_iter_get_text (start, end);
  else
    return gtk_text_iter_get_visible_text (start, end);
}

/**
 * gtk_text_buffer_get_slice:
 * @buffer: a #GtkTextBuffer
 * @start: start of a range
 * @end: end of a range
 * @include_hidden_chars: whether to include invisible text
 *
 * Returns the text in the range [@start,@end). Excludes undisplayed
 * text (text marked with tags that set the invisibility attribute) if
 * @include_hidden_chars is %FALSE. The returned string includes a
 * 0xFFFC character whenever the buffer contains
 * embedded images, so byte and character indexes into
 * the returned string do correspond to byte
 * and character indexes into the buffer. Contrast with
 * gtk_text_buffer_get_text(). Note that 0xFFFC can occur in normal
 * text as well, so it is not a reliable indicator that a texture or
 * widget is in the buffer.
 *
 * Returns: (transfer full): an allocated UTF-8 string
 **/
gchar*
gtk_text_buffer_get_slice (GtkTextBuffer     *buffer,
                           const GtkTextIter *start,
                           const GtkTextIter *end,
                           gboolean           include_hidden_chars)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (start != NULL, NULL);
  g_return_val_if_fail (end != NULL, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (start) == buffer, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (end) == buffer, NULL);
  
  if (include_hidden_chars)
    return gtk_text_iter_get_slice (start, end);
  else
    return gtk_text_iter_get_visible_slice (start, end);
}

/*
 * Pixbufs
 */

static void
gtk_text_buffer_real_insert_texture (GtkTextBuffer *buffer,
                                     GtkTextIter   *iter,
                                     GdkTexture    *texture)
{ 
  _gtk_text_btree_insert_texture (iter, texture);

  g_signal_emit (buffer, signals[CHANGED], 0);
}

/**
 * gtk_text_buffer_insert_texture:
 * @buffer: a #GtkTextBuffer
 * @iter: location to insert the texture
 * @texture: a #GdkTexture
 *
 * Inserts an image into the text buffer at @iter. The image will be
 * counted as one character in character counts, and when obtaining
 * the buffer contents as a string, will be represented by the Unicode
 * “object replacement character” 0xFFFC. Note that the “slice”
 * variants for obtaining portions of the buffer as a string include
 * this character for texture, but the “text” variants do
 * not. e.g. see gtk_text_buffer_get_slice() and
 * gtk_text_buffer_get_text().
 **/
void
gtk_text_buffer_insert_texture (GtkTextBuffer *buffer,
                                GtkTextIter   *iter,
                                GdkTexture    *texture)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);

  g_signal_emit (buffer, signals[INSERT_TEXTURE], 0, iter, texture);
}

/*
 * Child anchor
 */


static void
gtk_text_buffer_real_insert_anchor (GtkTextBuffer      *buffer,
                                    GtkTextIter        *iter,
                                    GtkTextChildAnchor *anchor)
{
  _gtk_text_btree_insert_child_anchor (iter, anchor);

  g_signal_emit (buffer, signals[CHANGED], 0);
}

/**
 * gtk_text_buffer_insert_child_anchor:
 * @buffer: a #GtkTextBuffer
 * @iter: location to insert the anchor
 * @anchor: a #GtkTextChildAnchor
 *
 * Inserts a child widget anchor into the text buffer at @iter. The
 * anchor will be counted as one character in character counts, and
 * when obtaining the buffer contents as a string, will be represented
 * by the Unicode “object replacement character” 0xFFFC. Note that the
 * “slice” variants for obtaining portions of the buffer as a string
 * include this character for child anchors, but the “text” variants do
 * not. E.g. see gtk_text_buffer_get_slice() and
 * gtk_text_buffer_get_text(). Consider
 * gtk_text_buffer_create_child_anchor() as a more convenient
 * alternative to this function. The buffer will add a reference to
 * the anchor, so you can unref it after insertion.
 **/
void
gtk_text_buffer_insert_child_anchor (GtkTextBuffer      *buffer,
                                     GtkTextIter        *iter,
                                     GtkTextChildAnchor *anchor)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (gtk_text_iter_get_buffer (iter) == buffer);
  
  g_signal_emit (buffer, signals[INSERT_CHILD_ANCHOR], 0,
                 iter, anchor);
}

/**
 * gtk_text_buffer_create_child_anchor:
 * @buffer: a #GtkTextBuffer
 * @iter: location in the buffer
 * 
 * This is a convenience function which simply creates a child anchor
 * with gtk_text_child_anchor_new() and inserts it into the buffer
 * with gtk_text_buffer_insert_child_anchor(). The new anchor is
 * owned by the buffer; no reference count is returned to
 * the caller of gtk_text_buffer_create_child_anchor().
 * 
 * Returns: (transfer none): the created child anchor
 **/
GtkTextChildAnchor*
gtk_text_buffer_create_child_anchor (GtkTextBuffer *buffer,
                                     GtkTextIter   *iter)
{
  GtkTextChildAnchor *anchor;
  
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (gtk_text_iter_get_buffer (iter) == buffer, NULL);
  
  anchor = gtk_text_child_anchor_new ();

  gtk_text_buffer_insert_child_anchor (buffer, iter, anchor);

  g_object_unref (anchor);

  return anchor;
}

/*
 * Mark manipulation
 */

static void
gtk_text_buffer_mark_set (GtkTextBuffer     *buffer,
                          const GtkTextIter *location,
                          GtkTextMark       *mark)
{
  /* IMO this should NOT work like insert_text and delete_range,
   * where the real action happens in the default handler.
   * 
   * The reason is that the default handler would be _required_,
   * i.e. the whole widget would start breaking and segfaulting if the
   * default handler didn't get run. So you can't really override the
   * default handler or stop the emission; that is, this signal is
   * purely for notification, and not to allow users to modify the
   * default behavior.
   */

  g_object_ref (mark);

  g_signal_emit (buffer,
                 signals[MARK_SET],
                 0,
                 location,
                 mark);

  g_object_unref (mark);
}

/**
 * gtk_text_buffer_set_mark:
 * @buffer:       a #GtkTextBuffer
 * @mark_name:    name of the mark
 * @iter:         location for the mark
 * @left_gravity: if the mark is created by this function, gravity for 
 *                the new mark
 * @should_exist: if %TRUE, warn if the mark does not exist, and return
 *                immediately
 *
 * Move the mark to the given position, if not @should_exist, 
 * create the mark.
 *
 * Returns: mark
 **/
static GtkTextMark*
gtk_text_buffer_set_mark (GtkTextBuffer     *buffer,
                          GtkTextMark       *existing_mark,
                          const gchar       *mark_name,
                          const GtkTextIter *iter,
                          gboolean           left_gravity,
                          gboolean           should_exist)
{
  GtkTextIter location;
  GtkTextMark *mark;

  g_return_val_if_fail (gtk_text_iter_get_buffer (iter) == buffer, NULL);
  
  mark = _gtk_text_btree_set_mark (get_btree (buffer),
                                   existing_mark,
                                   mark_name,
                                   left_gravity,
                                   iter,
                                   should_exist);
  
  _gtk_text_btree_get_iter_at_mark (get_btree (buffer),
                                   &location,
                                   mark);

  gtk_text_buffer_mark_set (buffer, &location, mark);

  return mark;
}

/**
 * gtk_text_buffer_create_mark:
 * @buffer: a #GtkTextBuffer
 * @mark_name: (allow-none): name for mark, or %NULL
 * @where: location to place mark
 * @left_gravity: whether the mark has left gravity
 *
 * Creates a mark at position @where. If @mark_name is %NULL, the mark
 * is anonymous; otherwise, the mark can be retrieved by name using
 * gtk_text_buffer_get_mark(). If a mark has left gravity, and text is
 * inserted at the mark’s current location, the mark will be moved to
 * the left of the newly-inserted text. If the mark has right gravity
 * (@left_gravity = %FALSE), the mark will end up on the right of
 * newly-inserted text. The standard left-to-right cursor is a mark
 * with right gravity (when you type, the cursor stays on the right
 * side of the text you’re typing).
 *
 * The caller of this function does not own a 
 * reference to the returned #GtkTextMark, so you can ignore the 
 * return value if you like. Marks are owned by the buffer and go 
 * away when the buffer does.
 *
 * Emits the #GtkTextBuffer::mark-set signal as notification of the mark's
 * initial placement.
 *
 * Returns: (transfer none): the new #GtkTextMark object
 **/
GtkTextMark*
gtk_text_buffer_create_mark (GtkTextBuffer     *buffer,
                             const gchar       *mark_name,
                             const GtkTextIter *where,
                             gboolean           left_gravity)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return gtk_text_buffer_set_mark (buffer, NULL, mark_name, where,
                                   left_gravity, FALSE);
}

/**
 * gtk_text_buffer_add_mark:
 * @buffer: a #GtkTextBuffer
 * @mark: the mark to add
 * @where: location to place mark
 *
 * Adds the mark at position @where. The mark must not be added to
 * another buffer, and if its name is not %NULL then there must not
 * be another mark in the buffer with the same name.
 *
 * Emits the #GtkTextBuffer::mark-set signal as notification of the mark's
 * initial placement.
 **/
void
gtk_text_buffer_add_mark (GtkTextBuffer     *buffer,
                          GtkTextMark       *mark,
                          const GtkTextIter *where)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (where != NULL);
  g_return_if_fail (gtk_text_mark_get_buffer (mark) == NULL);

  name = gtk_text_mark_get_name (mark);

  if (name != NULL && gtk_text_buffer_get_mark (buffer, name) != NULL)
    {
      g_critical ("Mark %s already exists in the buffer", name);
      return;
    }

  gtk_text_buffer_set_mark (buffer, mark, NULL, where, FALSE, FALSE);
}

/**
 * gtk_text_buffer_move_mark:
 * @buffer: a #GtkTextBuffer
 * @mark: a #GtkTextMark
 * @where: new location for @mark in @buffer
 *
 * Moves @mark to the new location @where. Emits the #GtkTextBuffer::mark-set
 * signal as notification of the move.
 **/
void
gtk_text_buffer_move_mark (GtkTextBuffer     *buffer,
                           GtkTextMark       *mark,
                           const GtkTextIter *where)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_set_mark (buffer, mark, NULL, where, FALSE, TRUE);
}

/**
 * gtk_text_buffer_get_iter_at_mark:
 * @buffer: a #GtkTextBuffer
 * @iter: (out): iterator to initialize
 * @mark: a #GtkTextMark in @buffer
 *
 * Initializes @iter with the current position of @mark.
 **/
void
gtk_text_buffer_get_iter_at_mark (GtkTextBuffer *buffer,
                                  GtkTextIter   *iter,
                                  GtkTextMark   *mark)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  _gtk_text_btree_get_iter_at_mark (get_btree (buffer),
                                    iter,
                                    mark);
}

/**
 * gtk_text_buffer_delete_mark:
 * @buffer: a #GtkTextBuffer
 * @mark: a #GtkTextMark in @buffer
 *
 * Deletes @mark, so that it’s no longer located anywhere in the
 * buffer. Removes the reference the buffer holds to the mark, so if
 * you haven’t called g_object_ref() on the mark, it will be freed. Even
 * if the mark isn’t freed, most operations on @mark become
 * invalid, until it gets added to a buffer again with 
 * gtk_text_buffer_add_mark(). Use gtk_text_mark_get_deleted() to  
 * find out if a mark has been removed from its buffer.
 * The #GtkTextBuffer::mark-deleted signal will be emitted as notification after
 * the mark is deleted.
 **/
void
gtk_text_buffer_delete_mark (GtkTextBuffer *buffer,
                             GtkTextMark   *mark)
{
  g_return_if_fail (GTK_IS_TEXT_MARK (mark));
  g_return_if_fail (!gtk_text_mark_get_deleted (mark));
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  g_object_ref (mark);

  _gtk_text_btree_remove_mark (get_btree (buffer), mark);

  /* See rationale above for MARK_SET on why we emit this after
   * removing the mark, rather than removing the mark in a default
   * handler.
   */
  g_signal_emit (buffer, signals[MARK_DELETED],
                 0,
                 mark);

  g_object_unref (mark);
}

/**
 * gtk_text_buffer_get_mark:
 * @buffer: a #GtkTextBuffer
 * @name: a mark name
 *
 * Returns the mark named @name in buffer @buffer, or %NULL if no such
 * mark exists in the buffer.
 *
 * Returns: (nullable) (transfer none): a #GtkTextMark, or %NULL
 **/
GtkTextMark*
gtk_text_buffer_get_mark (GtkTextBuffer *buffer,
                          const gchar   *name)
{
  GtkTextMark *mark;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  mark = _gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  return mark;
}

/**
 * gtk_text_buffer_move_mark_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a mark
 * @where: new location for mark
 *
 * Moves the mark named @name (which must exist) to location @where.
 * See gtk_text_buffer_move_mark() for details.
 **/
void
gtk_text_buffer_move_mark_by_name (GtkTextBuffer     *buffer,
                                   const gchar       *name,
                                   const GtkTextIter *where)
{
  GtkTextMark *mark;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);

  mark = _gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  if (mark == NULL)
    {
      g_warning ("%s: no mark named '%s'", G_STRLOC, name);
      return;
    }

  gtk_text_buffer_move_mark (buffer, mark, where);
}

/**
 * gtk_text_buffer_delete_mark_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a mark in @buffer
 *
 * Deletes the mark named @name; the mark must exist. See
 * gtk_text_buffer_delete_mark() for details.
 **/
void
gtk_text_buffer_delete_mark_by_name (GtkTextBuffer *buffer,
                                     const gchar   *name)
{
  GtkTextMark *mark;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);

  mark = _gtk_text_btree_get_mark_by_name (get_btree (buffer), name);

  if (mark == NULL)
    {
      g_warning ("%s: no mark named '%s'", G_STRLOC, name);
      return;
    }

  gtk_text_buffer_delete_mark (buffer, mark);
}

/**
 * gtk_text_buffer_get_insert:
 * @buffer: a #GtkTextBuffer
 *
 * Returns the mark that represents the cursor (insertion point).
 * Equivalent to calling gtk_text_buffer_get_mark() to get the mark
 * named “insert”, but very slightly more efficient, and involves less
 * typing.
 *
 * Returns: (transfer none): insertion point mark
 **/
GtkTextMark*
gtk_text_buffer_get_insert (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return _gtk_text_btree_get_insert (get_btree (buffer));
}

/**
 * gtk_text_buffer_get_selection_bound:
 * @buffer: a #GtkTextBuffer
 *
 * Returns the mark that represents the selection bound.  Equivalent
 * to calling gtk_text_buffer_get_mark() to get the mark named
 * “selection_bound”, but very slightly more efficient, and involves
 * less typing.
 *
 * The currently-selected text in @buffer is the region between the
 * “selection_bound” and “insert” marks. If “selection_bound” and
 * “insert” are in the same place, then there is no current selection.
 * gtk_text_buffer_get_selection_bounds() is another convenient function
 * for handling the selection, if you just want to know whether there’s a
 * selection and what its bounds are.
 *
 * Returns: (transfer none): selection bound mark
 **/
GtkTextMark*
gtk_text_buffer_get_selection_bound (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  return _gtk_text_btree_get_selection_bound (get_btree (buffer));
}

/**
 * gtk_text_buffer_get_iter_at_child_anchor:
 * @buffer: a #GtkTextBuffer
 * @iter: (out): an iterator to be initialized
 * @anchor: a child anchor that appears in @buffer
 *
 * Obtains the location of @anchor within @buffer.
 **/
void
gtk_text_buffer_get_iter_at_child_anchor (GtkTextBuffer      *buffer,
                                          GtkTextIter        *iter,
                                          GtkTextChildAnchor *anchor)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_CHILD_ANCHOR (anchor));
  g_return_if_fail (!gtk_text_child_anchor_get_deleted (anchor));
  
  _gtk_text_btree_get_iter_at_child_anchor (get_btree (buffer),
                                           iter,
                                           anchor);
}

/**
 * gtk_text_buffer_place_cursor:
 * @buffer: a #GtkTextBuffer
 * @where: where to put the cursor
 *
 * This function moves the “insert” and “selection_bound” marks
 * simultaneously.  If you move them to the same place in two steps
 * with gtk_text_buffer_move_mark(), you will temporarily select a
 * region in between their old and new locations, which can be pretty
 * inefficient since the temporarily-selected region will force stuff
 * to be recalculated. This function moves them as a unit, which can
 * be optimized.
 **/
void
gtk_text_buffer_place_cursor (GtkTextBuffer     *buffer,
                              const GtkTextIter *where)
{
  gtk_text_buffer_select_range (buffer, where, where);
}

/**
 * gtk_text_buffer_select_range:
 * @buffer: a #GtkTextBuffer
 * @ins: where to put the “insert” mark
 * @bound: where to put the “selection_bound” mark
 *
 * This function moves the “insert” and “selection_bound” marks
 * simultaneously.  If you move them in two steps
 * with gtk_text_buffer_move_mark(), you will temporarily select a
 * region in between their old and new locations, which can be pretty
 * inefficient since the temporarily-selected region will force stuff
 * to be recalculated. This function moves them as a unit, which can
 * be optimized.
 **/
void
gtk_text_buffer_select_range (GtkTextBuffer     *buffer,
			      const GtkTextIter *ins,
                              const GtkTextIter *bound)
{
  GtkTextIter real_ins;
  GtkTextIter real_bound;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  real_ins = *ins;
  real_bound = *bound;

  _gtk_text_btree_select_range (get_btree (buffer), &real_ins, &real_bound);
  gtk_text_buffer_mark_set (buffer, &real_ins,
                            gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_mark_set (buffer, &real_bound,
                            gtk_text_buffer_get_selection_bound (buffer));
}

/*
 * Tags
 */

/**
 * gtk_text_buffer_create_tag:
 * @buffer: a #GtkTextBuffer
 * @tag_name: (allow-none): name of the new tag, or %NULL
 * @first_property_name: (allow-none): name of first property to set, or %NULL
 * @...: %NULL-terminated list of property names and values
 *
 * Creates a tag and adds it to the tag table for @buffer.
 * Equivalent to calling gtk_text_tag_new() and then adding the
 * tag to the buffer’s tag table. The returned tag is owned by
 * the buffer’s tag table, so the ref count will be equal to one.
 *
 * If @tag_name is %NULL, the tag is anonymous.
 *
 * If @tag_name is non-%NULL, a tag called @tag_name must not already
 * exist in the tag table for this buffer.
 *
 * The @first_property_name argument and subsequent arguments are a list
 * of properties to set on the tag, as with g_object_set().
 *
 * Returns: (transfer none): a new tag
 */
GtkTextTag*
gtk_text_buffer_create_tag (GtkTextBuffer *buffer,
                            const gchar   *tag_name,
                            const gchar   *first_property_name,
                            ...)
{
  GtkTextTag *tag;
  va_list list;
  
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);

  tag = gtk_text_tag_new (tag_name);

  if (!gtk_text_tag_table_add (get_table (buffer), tag))
    {
      g_object_unref (tag);
      return NULL;
    }

  if (first_property_name)
    {
      va_start (list, first_property_name);
      g_object_set_valist (G_OBJECT (tag), first_property_name, list);
      va_end (list);
    }
  
  g_object_unref (tag);

  return tag;
}

static void
gtk_text_buffer_real_apply_tag (GtkTextBuffer     *buffer,
                                GtkTextTag        *tag,
                                const GtkTextIter *start,
                                const GtkTextIter *end)
{
  if (tag->priv->table != buffer->priv->tag_table)
    {
      g_warning ("Can only apply tags that are in the tag table for the buffer");
      return;
    }
  
  _gtk_text_btree_tag (start, end, tag, TRUE);
}

static void
gtk_text_buffer_real_remove_tag (GtkTextBuffer     *buffer,
                                 GtkTextTag        *tag,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  if (tag->priv->table != buffer->priv->tag_table)
    {
      g_warning ("Can only remove tags that are in the tag table for the buffer");
      return;
    }
  
  _gtk_text_btree_tag (start, end, tag, FALSE);
}

static void
gtk_text_buffer_real_changed (GtkTextBuffer *buffer)
{
  gtk_text_buffer_set_modified (buffer, TRUE);

  g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_TEXT]);
}

static void
gtk_text_buffer_real_mark_set (GtkTextBuffer     *buffer,
                               const GtkTextIter *iter,
                               GtkTextMark       *mark)
{
  GtkTextMark *insert;
  
  insert = gtk_text_buffer_get_insert (buffer);

  if (mark == insert || mark == gtk_text_buffer_get_selection_bound (buffer))
    {
      gboolean has_selection;

      update_selection_clipboards (buffer);
    
      has_selection = gtk_text_buffer_get_selection_bounds (buffer,
                                                            NULL,
                                                            NULL);

      if (has_selection != buffer->priv->has_selection)
        {
          buffer->priv->has_selection = has_selection;
          g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_HAS_SELECTION]);
        }
    }
    
    if (mark == insert)
      g_object_notify_by_pspec (G_OBJECT (buffer), text_buffer_props[PROP_CURSOR_POSITION]);
}

static void
gtk_text_buffer_emit_tag (GtkTextBuffer     *buffer,
                          GtkTextTag        *tag,
                          gboolean           apply,
                          const GtkTextIter *start,
                          const GtkTextIter *end)
{
  GtkTextIter start_tmp = *start;
  GtkTextIter end_tmp = *end;

  g_return_if_fail (tag != NULL);

  gtk_text_iter_order (&start_tmp, &end_tmp);

  if (apply)
    g_signal_emit (buffer, signals[APPLY_TAG],
                   0,
                   tag, &start_tmp, &end_tmp);
  else
    g_signal_emit (buffer, signals[REMOVE_TAG],
                   0,
                   tag, &start_tmp, &end_tmp);
}

/**
 * gtk_text_buffer_apply_tag:
 * @buffer: a #GtkTextBuffer
 * @tag: a #GtkTextTag
 * @start: one bound of range to be tagged
 * @end: other bound of range to be tagged
 *
 * Emits the “apply-tag” signal on @buffer. The default
 * handler for the signal applies @tag to the given range.
 * @start and @end do not have to be in order.
 **/
void
gtk_text_buffer_apply_tag (GtkTextBuffer     *buffer,
                           GtkTextTag        *tag,
                           const GtkTextIter *start,
                           const GtkTextIter *end)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
  g_return_if_fail (tag->priv->table == buffer->priv->tag_table);
  
  gtk_text_buffer_emit_tag (buffer, tag, TRUE, start, end);
}

/**
 * gtk_text_buffer_remove_tag:
 * @buffer: a #GtkTextBuffer
 * @tag: a #GtkTextTag
 * @start: one bound of range to be untagged
 * @end: other bound of range to be untagged
 *
 * Emits the “remove-tag” signal. The default handler for the signal
 * removes all occurrences of @tag from the given range. @start and
 * @end don’t have to be in order.
 **/
void
gtk_text_buffer_remove_tag (GtkTextBuffer     *buffer,
                            GtkTextTag        *tag,
                            const GtkTextIter *start,
                            const GtkTextIter *end)

{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (GTK_IS_TEXT_TAG (tag));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
  g_return_if_fail (tag->priv->table == buffer->priv->tag_table);
  
  gtk_text_buffer_emit_tag (buffer, tag, FALSE, start, end);
}

/**
 * gtk_text_buffer_apply_tag_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a named #GtkTextTag
 * @start: one bound of range to be tagged
 * @end: other bound of range to be tagged
 *
 * Calls gtk_text_tag_table_lookup() on the buffer’s tag table to
 * get a #GtkTextTag, then calls gtk_text_buffer_apply_tag().
 **/
void
gtk_text_buffer_apply_tag_by_name (GtkTextBuffer     *buffer,
                                   const gchar       *name,
                                   const GtkTextIter *start,
                                   const GtkTextIter *end)
{
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);

  tag = gtk_text_tag_table_lookup (get_table (buffer),
                                   name);

  if (tag == NULL)
    {
      g_warning ("Unknown tag '%s'", name);
      return;
    }

  gtk_text_buffer_emit_tag (buffer, tag, TRUE, start, end);
}

/**
 * gtk_text_buffer_remove_tag_by_name:
 * @buffer: a #GtkTextBuffer
 * @name: name of a #GtkTextTag
 * @start: one bound of range to be untagged
 * @end: other bound of range to be untagged
 *
 * Calls gtk_text_tag_table_lookup() on the buffer’s tag table to
 * get a #GtkTextTag, then calls gtk_text_buffer_remove_tag().
 **/
void
gtk_text_buffer_remove_tag_by_name (GtkTextBuffer     *buffer,
                                    const gchar       *name,
                                    const GtkTextIter *start,
                                    const GtkTextIter *end)
{
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (name != NULL);
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
  
  tag = gtk_text_tag_table_lookup (get_table (buffer),
                                   name);

  if (tag == NULL)
    {
      g_warning ("Unknown tag '%s'", name);
      return;
    }

  gtk_text_buffer_emit_tag (buffer, tag, FALSE, start, end);
}

static gint
pointer_cmp (gconstpointer a,
             gconstpointer b)
{
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

/**
 * gtk_text_buffer_remove_all_tags:
 * @buffer: a #GtkTextBuffer
 * @start: one bound of range to be untagged
 * @end: other bound of range to be untagged
 * 
 * Removes all tags in the range between @start and @end.  Be careful
 * with this function; it could remove tags added in code unrelated to
 * the code you’re currently writing. That is, using this function is
 * probably a bad idea if you have two or more unrelated code sections
 * that add tags.
 **/
void
gtk_text_buffer_remove_all_tags (GtkTextBuffer     *buffer,
                                 const GtkTextIter *start,
                                 const GtkTextIter *end)
{
  GtkTextIter first, second, tmp;
  GSList *tags;
  GSList *tmp_list;
  GSList *prev, *next;
  GtkTextTag *tag;
  
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (gtk_text_iter_get_buffer (start) == buffer);
  g_return_if_fail (gtk_text_iter_get_buffer (end) == buffer);
  
  first = *start;
  second = *end;

  gtk_text_iter_order (&first, &second);

  /* Get all tags turned on at the start */
  tags = gtk_text_iter_get_tags (&first);
  
  /* Find any that are toggled on within the range */
  tmp = first;
  while (gtk_text_iter_forward_to_tag_toggle (&tmp, NULL))
    {
      GSList *toggled;
      GSList *tmp_list2;

      if (gtk_text_iter_compare (&tmp, &second) >= 0)
        break; /* past the end of the range */
      
      toggled = gtk_text_iter_get_toggled_tags (&tmp, TRUE);

      /* We could end up with a really big-ass list here.
       * Fix it someday.
       */
      tmp_list2 = toggled;
      while (tmp_list2 != NULL)
        {
          tags = g_slist_prepend (tags, tmp_list2->data);

          tmp_list2 = tmp_list2->next;
        }

      g_slist_free (toggled);
    }
  
  /* Sort the list */
  tags = g_slist_sort (tags, pointer_cmp);

  /* Strip duplicates */
  tag = NULL;
  prev = NULL;
  tmp_list = tags;
  while (tmp_list != NULL)
    {
      if (tag == tmp_list->data)
        {
          /* duplicate */
          next = tmp_list->next;
          if (prev)
            prev->next = next;

          tmp_list->next = NULL;

          g_slist_free (tmp_list);

          tmp_list = next;
          /* prev is unchanged */
        }
      else
        {
          /* not a duplicate */
          tag = GTK_TEXT_TAG (tmp_list->data);
          prev = tmp_list;
          tmp_list = tmp_list->next;
        }
    }

  g_slist_foreach (tags, (GFunc) g_object_ref, NULL);
  
  tmp_list = tags;
  while (tmp_list != NULL)
    {
      tag = GTK_TEXT_TAG (tmp_list->data);

      gtk_text_buffer_remove_tag (buffer, tag, &first, &second);
      
      tmp_list = tmp_list->next;
    }

  g_slist_free_full (tags, g_object_unref);
}


/*
 * Obtain various iterators
 */

/**
 * gtk_text_buffer_get_iter_at_line_offset:
 * @buffer: a #GtkTextBuffer
 * @iter: (out): iterator to initialize
 * @line_number: line number counting from 0
 * @char_offset: char offset from start of line
 *
 * Obtains an iterator pointing to @char_offset within the given line. Note
 * characters, not bytes; UTF-8 may encode one character as multiple bytes.
 *
 * Before the 3.20 version, it was not allowed to pass an invalid location.
 *
 * If @line_number is greater than the number of lines
 * in the @buffer, the end iterator is returned. And if @char_offset is off the
 * end of the line, the iterator at the end of the line is returned.
 **/
void
gtk_text_buffer_get_iter_at_line_offset (GtkTextBuffer *buffer,
                                         GtkTextIter   *iter,
                                         gint           line_number,
                                         gint           char_offset)
{
  GtkTextIter end_line_iter;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_end_iter (buffer, iter);
      return;
    }

  _gtk_text_btree_get_iter_at_line_char (get_btree (buffer), iter, line_number, 0);

  end_line_iter = *iter;
  if (!gtk_text_iter_ends_line (&end_line_iter))
    gtk_text_iter_forward_to_line_end (&end_line_iter);

  if (char_offset <= gtk_text_iter_get_line_offset (&end_line_iter))
    gtk_text_iter_set_line_offset (iter, char_offset);
  else
    *iter = end_line_iter;
}

/**
 * gtk_text_buffer_get_iter_at_line_index:
 * @buffer: a #GtkTextBuffer 
 * @iter: (out): iterator to initialize 
 * @line_number: line number counting from 0
 * @byte_index: byte index from start of line
 *
 * Obtains an iterator pointing to @byte_index within the given line.
 * @byte_index must be the start of a UTF-8 character. Note bytes, not
 * characters; UTF-8 may encode one character as multiple bytes.
 *
 * If @line_number is greater than the number of lines
 * in the @buffer, the end iterator is returned. And if @byte_index is off the
 * end of the line, the iterator at the end of the line is returned.
 **/
void
gtk_text_buffer_get_iter_at_line_index  (GtkTextBuffer *buffer,
                                         GtkTextIter   *iter,
                                         gint           line_number,
                                         gint           byte_index)
{
  GtkTextIter end_line_iter;

  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_end_iter (buffer, iter);
      return;
    }

  gtk_text_buffer_get_iter_at_line (buffer, iter, line_number);

  end_line_iter = *iter;
  if (!gtk_text_iter_ends_line (&end_line_iter))
    gtk_text_iter_forward_to_line_end (&end_line_iter);

  if (byte_index <= gtk_text_iter_get_line_index (&end_line_iter))
    gtk_text_iter_set_line_index (iter, byte_index);
  else
    *iter = end_line_iter;
}

/**
 * gtk_text_buffer_get_iter_at_line:
 * @buffer: a #GtkTextBuffer 
 * @iter: (out): iterator to initialize
 * @line_number: line number counting from 0
 *
 * Initializes @iter to the start of the given line. If @line_number is greater
 * than the number of lines in the @buffer, the end iterator is returned.
 **/
void
gtk_text_buffer_get_iter_at_line (GtkTextBuffer *buffer,
                                  GtkTextIter   *iter,
                                  gint           line_number)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  gtk_text_buffer_get_iter_at_line_offset (buffer, iter, line_number, 0);
}

/**
 * gtk_text_buffer_get_iter_at_offset:
 * @buffer: a #GtkTextBuffer 
 * @iter: (out): iterator to initialize
 * @char_offset: char offset from start of buffer, counting from 0, or -1
 *
 * Initializes @iter to a position @char_offset chars from the start
 * of the entire buffer. If @char_offset is -1 or greater than the number
 * of characters in the buffer, @iter is initialized to the end iterator,
 * the iterator one past the last valid character in the buffer.
 **/
void
gtk_text_buffer_get_iter_at_offset (GtkTextBuffer *buffer,
                                    GtkTextIter   *iter,
                                    gint           char_offset)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  _gtk_text_btree_get_iter_at_char (get_btree (buffer), iter, char_offset);
}

/**
 * gtk_text_buffer_get_start_iter:
 * @buffer: a #GtkTextBuffer
 * @iter: (out): iterator to initialize
 *
 * Initialized @iter with the first position in the text buffer. This
 * is the same as using gtk_text_buffer_get_iter_at_offset() to get
 * the iter at character offset 0.
 **/
void
gtk_text_buffer_get_start_iter (GtkTextBuffer *buffer,
                                GtkTextIter   *iter)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  _gtk_text_btree_get_iter_at_char (get_btree (buffer), iter, 0);
}

/**
 * gtk_text_buffer_get_end_iter:
 * @buffer: a #GtkTextBuffer 
 * @iter: (out): iterator to initialize
 *
 * Initializes @iter with the “end iterator,” one past the last valid
 * character in the text buffer. If dereferenced with
 * gtk_text_iter_get_char(), the end iterator has a character value of 0.
 * The entire buffer lies in the range from the first position in
 * the buffer (call gtk_text_buffer_get_start_iter() to get
 * character position 0) to the end iterator.
 **/
void
gtk_text_buffer_get_end_iter (GtkTextBuffer *buffer,
                              GtkTextIter   *iter)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  _gtk_text_btree_get_end_iter (get_btree (buffer), iter);
}

/**
 * gtk_text_buffer_get_bounds:
 * @buffer: a #GtkTextBuffer 
 * @start: (out): iterator to initialize with first position in the buffer
 * @end: (out): iterator to initialize with the end iterator
 *
 * Retrieves the first and last iterators in the buffer, i.e. the
 * entire buffer lies within the range [@start,@end).
 **/
void
gtk_text_buffer_get_bounds (GtkTextBuffer *buffer,
                            GtkTextIter   *start,
                            GtkTextIter   *end)
{
  g_return_if_fail (start != NULL);
  g_return_if_fail (end != NULL);
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  _gtk_text_btree_get_iter_at_char (get_btree (buffer), start, 0);
  _gtk_text_btree_get_end_iter (get_btree (buffer), end);
}

/*
 * Modified flag
 */

/**
 * gtk_text_buffer_get_modified:
 * @buffer: a #GtkTextBuffer 
 * 
 * Indicates whether the buffer has been modified since the last call
 * to gtk_text_buffer_set_modified() set the modification flag to
 * %FALSE. Used for example to enable a “save” function in a text
 * editor.
 * 
 * Returns: %TRUE if the buffer has been modified
 **/
gboolean
gtk_text_buffer_get_modified (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return buffer->priv->modified;
}

/**
 * gtk_text_buffer_set_modified:
 * @buffer: a #GtkTextBuffer 
 * @setting: modification flag setting
 *
 * Used to keep track of whether the buffer has been modified since the
 * last time it was saved. Whenever the buffer is saved to disk, call
 * gtk_text_buffer_set_modified (@buffer, FALSE). When the buffer is modified,
 * it will automatically toggled on the modified bit again. When the modified
 * bit flips, the buffer emits the #GtkTextBuffer::modified-changed signal.
 **/
void
gtk_text_buffer_set_modified (GtkTextBuffer *buffer,
                              gboolean       setting)
{
  gboolean fixed_setting;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  fixed_setting = setting != FALSE;

  if (buffer->priv->modified == fixed_setting)
    return;
  else
    {
      buffer->priv->modified = fixed_setting;
      g_signal_emit (buffer, signals[MODIFIED_CHANGED], 0);
    }
}

/**
 * gtk_text_buffer_get_has_selection:
 * @buffer: a #GtkTextBuffer 
 * 
 * Indicates whether the buffer has some text currently selected.
 * 
 * Returns: %TRUE if the there is text selected
 **/
gboolean
gtk_text_buffer_get_has_selection (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return buffer->priv->has_selection;
}


/*
 * Assorted other stuff
 */

/**
 * gtk_text_buffer_get_line_count:
 * @buffer: a #GtkTextBuffer 
 * 
 * Obtains the number of lines in the buffer. This value is cached, so
 * the function is very fast.
 * 
 * Returns: number of lines in the buffer
 **/
gint
gtk_text_buffer_get_line_count (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), 0);

  return _gtk_text_btree_line_count (get_btree (buffer));
}

/**
 * gtk_text_buffer_get_char_count:
 * @buffer: a #GtkTextBuffer 
 * 
 * Gets the number of characters in the buffer; note that characters
 * and bytes are not the same, you can’t e.g. expect the contents of
 * the buffer in string form to be this many bytes long. The character
 * count is cached, so this function is very fast.
 * 
 * Returns: number of characters in the buffer
 **/
gint
gtk_text_buffer_get_char_count (GtkTextBuffer *buffer)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), 0);

  return _gtk_text_btree_char_count (get_btree (buffer));
}

static GtkTextBuffer *
create_clipboard_contents_buffer (GtkTextBuffer *buffer,
                                  GtkTextIter   *start_iter,
                                  GtkTextIter   *end_iter)
{
  GtkTextIter start, end;
  GtkTextBuffer *contents;

  contents = gtk_text_buffer_new (gtk_text_buffer_get_tag_table (buffer));

  gtk_text_buffer_get_iter_at_offset (contents, &start, 0);
  gtk_text_buffer_insert_range (contents, &start, start_iter, end_iter);
  gtk_text_buffer_get_bounds (contents, &start, &end);
  gtk_text_buffer_select_range (contents, &start, &end);

  /*  Ref the source buffer as long as the clipboard contents buffer
   *  exists, because it's needed for serializing the contents buffer.
   *  See http://bugzilla.gnome.org/show_bug.cgi?id=339195
   */
  g_object_ref (buffer);
  g_object_weak_ref (G_OBJECT (contents), (GWeakNotify) g_object_unref, buffer);

  return contents;
}

static void
get_paste_point (GtkTextBuffer *buffer,
                 GtkTextIter   *iter,
                 gboolean       clear_afterward)
{
  GtkTextIter insert_point;
  GtkTextMark *paste_point_override;

  paste_point_override = gtk_text_buffer_get_mark (buffer,
                                                   "gtk_paste_point_override");

  if (paste_point_override != NULL)
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        paste_point_override);
      if (clear_afterward)
        gtk_text_buffer_delete_mark (buffer, paste_point_override);
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &insert_point,
                                        gtk_text_buffer_get_insert (buffer));
    }

  *iter = insert_point;
}

static void
pre_paste_prep (ClipboardRequest *request_data,
                GtkTextIter      *insert_point)
{
  GtkTextBuffer *buffer = request_data->buffer;
  
  get_paste_point (buffer, insert_point, TRUE);

  if (request_data->replace_selection)
    {
      GtkTextIter start, end;
      
      if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
        {
          if (request_data->interactive)
            gtk_text_buffer_delete_interactive (request_data->buffer,
                                                &start,
                                                &end,
                                                request_data->default_editable);
          else
            gtk_text_buffer_delete (request_data->buffer, &start, &end);

          *insert_point = start;
        }
    }
}

static void
emit_paste_done (GtkTextBuffer *buffer,
                 GdkClipboard  *clipboard)
{
  g_signal_emit (buffer, signals[PASTE_DONE], 0, clipboard);
}

static void
free_clipboard_request (ClipboardRequest *request_data)
{
  g_object_unref (request_data->buffer);
  g_slice_free (ClipboardRequest, request_data);
}

#if 0
/* These are pretty handy functions; maybe something like them
 * should be in the public API. Also, there are other places in this
 * file where they could be used.
 */
static gpointer
save_iter (const GtkTextIter *iter,
           gboolean           left_gravity)
{
  return gtk_text_buffer_create_mark (gtk_text_iter_get_buffer (iter),
                                      NULL,
                                      iter,
                                      TRUE);
}

static void
restore_iter (const GtkTextIter *iter,
              gpointer           save_id)
{
  gtk_text_buffer_get_iter_at_mark (gtk_text_mark_get_buffer (save_id),
                                    (GtkTextIter*) iter,
                                    save_id);
  gtk_text_buffer_delete_mark (gtk_text_mark_get_buffer (save_id),
                               save_id);
}
#endif

static void
paste_from_buffer (GdkClipboard      *clipboard,
                   ClipboardRequest  *request_data,
                   GtkTextBuffer     *src_buffer,
                   const GtkTextIter *start,
                   const GtkTextIter *end)
{
  GtkTextIter insert_point;
  GtkTextBuffer *buffer = request_data->buffer;

  /* We're about to emit a bunch of signals, so be safe */
  g_object_ref (src_buffer);

  /* Replacing the selection with itself */
  if (request_data->replace_selection &&
      buffer == src_buffer)
    {
      /* Clear the paste point if needed */
      get_paste_point (buffer, &insert_point, TRUE);
      goto done;
    }

  if (request_data->interactive) 
    gtk_text_buffer_begin_user_action (buffer);

  pre_paste_prep (request_data, &insert_point);

  if (!gtk_text_iter_equal (start, end))
    {
      if (!request_data->interactive ||
          (gtk_text_iter_can_insert (&insert_point,
                                     request_data->default_editable)))
        gtk_text_buffer_real_insert_range (buffer,
                                           &insert_point,
                                           start,
                                           end,
                                           request_data->interactive);
    }

  if (request_data->interactive) 
    gtk_text_buffer_end_user_action (buffer);

done:
  emit_paste_done (buffer, clipboard);

  g_object_unref (src_buffer);

  free_clipboard_request (request_data);
}

static void
gtk_text_buffer_paste_clipboard_finish (GObject      *source,
                                        GAsyncResult *result,
                                        gpointer      data)
{
  GdkClipboard *clipboard = GDK_CLIPBOARD (source);
  ClipboardRequest *request_data = data;
  GtkTextBuffer *src_buffer;
  GtkTextIter start, end;
  const GValue *value;

  value = gdk_clipboard_read_value_finish (clipboard, result, NULL);
  if (value == NULL)
    return;

  src_buffer = g_value_get_object (value);

  if (gtk_text_buffer_get_selection_bounds (src_buffer, &start, &end))
    paste_from_buffer (clipboard, request_data, src_buffer,
                       &start, &end);
}

typedef struct
{
  GdkClipboard *clipboard;
  guint ref_count;
} SelectionClipboard;

static void
update_selection_clipboards (GtkTextBuffer *buffer)
{
  GtkTextBufferPrivate *priv;
  GtkTextIter start;
  GtkTextIter end;
  GSList *l;

  priv = buffer->priv;

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      if (priv->selection_content)
        gdk_content_provider_content_changed (priv->selection_content);
      else
        priv->selection_content = gtk_text_buffer_content_new (buffer);
      
      for (l = priv->selection_clipboards; l; l = l->next)
        {
          SelectionClipboard *selection_clipboard = l->data;
          gdk_clipboard_set_content (selection_clipboard->clipboard, priv->selection_content);
        }
    }
  else
    {
      if (priv->selection_content)
        {
          GTK_TEXT_BUFFER_CONTENT (priv->selection_content)->text_buffer = NULL;
          for (l = priv->selection_clipboards; l; l = l->next)
            {
              SelectionClipboard *selection_clipboard = l->data;
              GdkClipboard *clipboard = selection_clipboard->clipboard;
              if (gdk_clipboard_get_content (clipboard) == priv->selection_content)
                gdk_clipboard_set_content (clipboard, NULL);
            }
          g_clear_object (&priv->selection_content);
        }
    }
}

static SelectionClipboard *
find_selection_clipboard (GtkTextBuffer *buffer,
			  GdkClipboard  *clipboard)
{
  GSList *tmp_list = buffer->priv->selection_clipboards;
  while (tmp_list)
    {
      SelectionClipboard *selection_clipboard = tmp_list->data;
      if (selection_clipboard->clipboard == clipboard)
	return selection_clipboard;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * gtk_text_buffer_add_selection_clipboard:
 * @buffer: a #GtkTextBuffer
 * @clipboard: a #GdkClipboard
 * 
 * Adds @clipboard to the list of clipboards in which the selection 
 * contents of @buffer are available. In most cases, @clipboard will be 
 * the #GdkClipboard returned by gdk_widget_get_primary_clipboard()
 * for a view of @buffer.
 **/
void
gtk_text_buffer_add_selection_clipboard (GtkTextBuffer *buffer,
					 GdkClipboard  *clipboard)
{
  SelectionClipboard *selection_clipboard;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (clipboard != NULL);

  selection_clipboard = find_selection_clipboard (buffer, clipboard);
  if (selection_clipboard)
    {
      selection_clipboard->ref_count++;
    }
  else
    {
      selection_clipboard = g_slice_new (SelectionClipboard);

      selection_clipboard->clipboard = clipboard;
      selection_clipboard->ref_count = 1;

      buffer->priv->selection_clipboards = g_slist_prepend (buffer->priv->selection_clipboards,
                                                            selection_clipboard);
    }
}

/**
 * gtk_text_buffer_remove_selection_clipboard:
 * @buffer: a #GtkTextBuffer
 * @clipboard: a #GdkClipboard added to @buffer by 
 *             gtk_text_buffer_add_selection_clipboard()
 * 
 * Removes a #GdkClipboard added with 
 * gtk_text_buffer_add_selection_clipboard().
 **/
void 
gtk_text_buffer_remove_selection_clipboard (GtkTextBuffer *buffer,
					    GdkClipboard  *clipboard)
{
  GtkTextBufferPrivate *priv;
  SelectionClipboard *selection_clipboard;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (clipboard != NULL);

  selection_clipboard = find_selection_clipboard (buffer, clipboard);
  g_return_if_fail (selection_clipboard != NULL);

  priv = buffer->priv;
  selection_clipboard->ref_count--;
  if (selection_clipboard->ref_count == 0)
    {
      if (priv->selection_content &&
          gdk_clipboard_get_content (clipboard) == priv->selection_content)
	gdk_clipboard_set_content (clipboard, NULL);

      buffer->priv->selection_clipboards = g_slist_remove (buffer->priv->selection_clipboards,
                                                           selection_clipboard);

      g_slice_free (SelectionClipboard, selection_clipboard);
    }
}

static void
remove_all_selection_clipboards (GtkTextBuffer *buffer)
{
  GtkTextBufferPrivate *priv = buffer->priv;
  GSList *l;

  for (l = priv->selection_clipboards; l != NULL; l = l->next)
    {
      SelectionClipboard *selection_clipboard = l->data;
      g_slice_free (SelectionClipboard, selection_clipboard);
    }

  g_slist_free (priv->selection_clipboards);
  priv->selection_clipboards = NULL;
}

/**
 * gtk_text_buffer_paste_clipboard:
 * @buffer: a #GtkTextBuffer
 * @clipboard: the #GdkClipboard to paste from
 * @override_location: (allow-none): location to insert pasted text, or %NULL
 * @default_editable: whether the buffer is editable by default
 *
 * Pastes the contents of a clipboard. If @override_location is %NULL, the
 * pasted text will be inserted at the cursor position, or the buffer selection
 * will be replaced if the selection is non-empty.
 *
 * Note: pasting is asynchronous, that is, we’ll ask for the paste data and
 * return, and at some point later after the main loop runs, the paste data will
 * be inserted.
 **/
void
gtk_text_buffer_paste_clipboard (GtkTextBuffer *buffer,
				 GdkClipboard  *clipboard,
				 GtkTextIter   *override_location,
                                 gboolean       default_editable)
{
  ClipboardRequest *data = g_slice_new (ClipboardRequest);
  GtkTextIter paste_point;
  GtkTextIter start, end;

  if (override_location != NULL)
    gtk_text_buffer_create_mark (buffer,
                                 "gtk_paste_point_override",
                                 override_location, FALSE);

  data->buffer = g_object_ref (buffer);
  data->interactive = TRUE;
  data->default_editable = !!default_editable;

  /* When pasting with the cursor inside the selection area, you
   * replace the selection with the new text, otherwise, you
   * simply insert the new text at the point where the click
   * occurred, unselecting any selected text. The replace_selection
   * flag toggles this behavior.
   */
  data->replace_selection = FALSE;
  
  get_paste_point (buffer, &paste_point, FALSE);
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end) &&
      (gtk_text_iter_in_range (&paste_point, &start, &end) ||
       gtk_text_iter_equal (&paste_point, &end)))
    data->replace_selection = TRUE;

  gdk_clipboard_read_value_async (clipboard,
                                  GTK_TYPE_TEXT_BUFFER,
                                  G_PRIORITY_DEFAULT,
                                  NULL,
                                  gtk_text_buffer_paste_clipboard_finish,
                                  data);
}

/**
 * gtk_text_buffer_delete_selection:
 * @buffer: a #GtkTextBuffer 
 * @interactive: whether the deletion is caused by user interaction
 * @default_editable: whether the buffer is editable by default
 *
 * Deletes the range between the “insert” and “selection_bound” marks,
 * that is, the currently-selected text. If @interactive is %TRUE,
 * the editability of the selection will be considered (users can’t delete
 * uneditable text).
 * 
 * Returns: whether there was a non-empty selection to delete
 **/
gboolean
gtk_text_buffer_delete_selection (GtkTextBuffer *buffer,
                                  gboolean interactive,
                                  gboolean default_editable)
{
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      return FALSE; /* No selection */
    }
  else
    {
      if (interactive)
        gtk_text_buffer_delete_interactive (buffer, &start, &end, default_editable);
      else
        gtk_text_buffer_delete (buffer, &start, &end);

      return TRUE; /* We deleted stuff */
    }
}

/**
 * gtk_text_buffer_backspace:
 * @buffer: a #GtkTextBuffer
 * @iter: a position in @buffer
 * @interactive: whether the deletion is caused by user interaction
 * @default_editable: whether the buffer is editable by default
 * 
 * Performs the appropriate action as if the user hit the delete
 * key with the cursor at the position specified by @iter. In the
 * normal case a single character will be deleted, but when
 * combining accents are involved, more than one character can
 * be deleted, and when precomposed character and accent combinations
 * are involved, less than one character will be deleted.
 * 
 * Because the buffer is modified, all outstanding iterators become 
 * invalid after calling this function; however, the @iter will be
 * re-initialized to point to the location where text was deleted. 
 *
 * Returns: %TRUE if the buffer was modified
 **/
gboolean
gtk_text_buffer_backspace (GtkTextBuffer *buffer,
			   GtkTextIter   *iter,
			   gboolean       interactive,
			   gboolean       default_editable)
{
  gchar *cluster_text;
  GtkTextIter start;
  GtkTextIter end;
  gboolean retval = FALSE;
  const PangoLogAttr *attrs;
  gint offset;
  gboolean backspace_deletes_character;

  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  start = *iter;
  end = *iter;

  attrs = _gtk_text_buffer_get_line_log_attrs (buffer, &start, NULL);
  offset = gtk_text_iter_get_line_offset (&start);
  backspace_deletes_character = attrs[offset].backspace_deletes_character;

  gtk_text_iter_backward_cursor_position (&start);

  if (gtk_text_iter_equal (&start, &end))
    return FALSE;
    
  cluster_text = gtk_text_iter_get_text (&start, &end);

  if (interactive)
    gtk_text_buffer_begin_user_action (buffer);
  
  if (gtk_text_buffer_delete_interactive (buffer, &start, &end,
					  default_editable))
    {
      /* special case \r\n, since we never want to reinsert \r */
      if (backspace_deletes_character && strcmp ("\r\n", cluster_text))
	{
	  gchar *normalized_text = g_utf8_normalize (cluster_text,
						     strlen (cluster_text),
						     G_NORMALIZE_NFD);
	  glong len = g_utf8_strlen (normalized_text, -1);

	  if (len > 1)
	    gtk_text_buffer_insert_interactive (buffer,
						&start,
						normalized_text,
						g_utf8_offset_to_pointer (normalized_text, len - 1) - normalized_text,
						default_editable);
	  
	  g_free (normalized_text);
	}

      retval = TRUE;
    }
  
  if (interactive)
    gtk_text_buffer_end_user_action (buffer);
  
  g_free (cluster_text);

  /* Revalidate the users iter */
  *iter = start;

  return retval;
}

static void
cut_or_copy (GtkTextBuffer *buffer,
	     GdkClipboard  *clipboard,
             gboolean       delete_region_after,
             gboolean       interactive,
             gboolean       default_editable)
{
  /* We prefer to cut the selected region between selection_bound and
   * insertion point. If that region is empty, then we cut the region
   * between the "anchor" and the insertion point (this is for
   * C-space and M-w and other Emacs-style copy/yank keys). Note that
   * insert and selection_bound are guaranteed to exist, but the
   * anchor only exists sometimes.
   */
  GtkTextIter start;
  GtkTextIter end;

  if (!gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      /* Let's try the anchor thing */
      GtkTextMark * anchor = gtk_text_buffer_get_mark (buffer, "anchor");

      if (anchor == NULL)
        return;
      else
        {
          gtk_text_buffer_get_iter_at_mark (buffer, &end, anchor);
          gtk_text_iter_order (&start, &end);
        }
    }

  if (!gtk_text_iter_equal (&start, &end))
    {
      GtkTextBuffer *contents;

      contents = create_clipboard_contents_buffer (buffer, &start, &end);
      gdk_clipboard_set (clipboard, GTK_TYPE_TEXT_BUFFER, contents);
      g_object_unref (contents);

      if (delete_region_after)
        {
          if (interactive)
            gtk_text_buffer_delete_interactive (buffer, &start, &end,
                                                default_editable);
          else
            gtk_text_buffer_delete (buffer, &start, &end);
        }
    }
}

/**
 * gtk_text_buffer_cut_clipboard:
 * @buffer: a #GtkTextBuffer
 * @clipboard: the #GdkClipboard object to cut to
 * @default_editable: default editability of the buffer
 *
 * Copies the currently-selected text to a clipboard, then deletes
 * said text if it’s editable.
 **/
void
gtk_text_buffer_cut_clipboard (GtkTextBuffer *buffer,
			       GdkClipboard  *clipboard,
                               gboolean       default_editable)
{
  gtk_text_buffer_begin_user_action (buffer);
  cut_or_copy (buffer, clipboard, TRUE, TRUE, default_editable);
  gtk_text_buffer_end_user_action (buffer);
}

/**
 * gtk_text_buffer_copy_clipboard:
 * @buffer: a #GtkTextBuffer 
 * @clipboard: the #GdkClipboard object to copy to
 *
 * Copies the currently-selected text to a clipboard.
 **/
void
gtk_text_buffer_copy_clipboard (GtkTextBuffer *buffer,
				GdkClipboard  *clipboard)
{
  cut_or_copy (buffer, clipboard, FALSE, TRUE, TRUE);
}

/**
 * gtk_text_buffer_get_selection_bounds:
 * @buffer: a #GtkTextBuffer a #GtkTextBuffer
 * @start: (out): iterator to initialize with selection start
 * @end: (out): iterator to initialize with selection end
 *
 * Returns %TRUE if some text is selected; places the bounds
 * of the selection in @start and @end (if the selection has length 0,
 * then @start and @end are filled in with the same value).
 * @start and @end will be in ascending order. If @start and @end are
 * NULL, then they are not filled in, but the return value still indicates
 * whether text is selected.
 *
 * Returns: whether the selection has nonzero length
 **/
gboolean
gtk_text_buffer_get_selection_bounds (GtkTextBuffer *buffer,
                                      GtkTextIter   *start,
                                      GtkTextIter   *end)
{
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), FALSE);

  return _gtk_text_btree_get_selection_bounds (get_btree (buffer), start, end);
}

/**
 * gtk_text_buffer_begin_user_action:
 * @buffer: a #GtkTextBuffer
 * 
 * Called to indicate that the buffer operations between here and a
 * call to gtk_text_buffer_end_user_action() are part of a single
 * user-visible operation. The operations between
 * gtk_text_buffer_begin_user_action() and
 * gtk_text_buffer_end_user_action() can then be grouped when creating
 * an undo stack. #GtkTextBuffer maintains a count of calls to
 * gtk_text_buffer_begin_user_action() that have not been closed with
 * a call to gtk_text_buffer_end_user_action(), and emits the 
 * “begin-user-action” and “end-user-action” signals only for the 
 * outermost pair of calls. This allows you to build user actions 
 * from other user actions.
 *
 * The “interactive” buffer mutation functions, such as
 * gtk_text_buffer_insert_interactive(), automatically call begin/end
 * user action around the buffer operations they perform, so there's
 * no need to add extra calls if you user action consists solely of a
 * single call to one of those functions.
 **/
void
gtk_text_buffer_begin_user_action (GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  buffer->priv->user_action_count += 1;
  
  if (buffer->priv->user_action_count == 1)
    {
      /* Outermost nested user action begin emits the signal */
      g_signal_emit (buffer, signals[BEGIN_USER_ACTION], 0);
    }
}

/**
 * gtk_text_buffer_end_user_action:
 * @buffer: a #GtkTextBuffer
 * 
 * Should be paired with a call to gtk_text_buffer_begin_user_action().
 * See that function for a full explanation.
 **/
void
gtk_text_buffer_end_user_action (GtkTextBuffer *buffer)
{
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (buffer->priv->user_action_count > 0);
  
  buffer->priv->user_action_count -= 1;
  
  if (buffer->priv->user_action_count == 0)
    {
      /* Ended the outermost-nested user action end, so emit the signal */
      g_signal_emit (buffer, signals[END_USER_ACTION], 0);
    }
}

/*
 * Logical attribute cache
 */

#define ATTR_CACHE_SIZE 2

typedef struct _CacheEntry CacheEntry;
struct _CacheEntry
{
  gint line;
  gint char_len;
  PangoLogAttr *attrs;
};

struct _GtkTextLogAttrCache
{
  gint chars_changed_stamp;
  CacheEntry entries[ATTR_CACHE_SIZE];
};

static void
free_log_attr_cache (GtkTextLogAttrCache *cache)
{
  gint i;

  for (i = 0; i < ATTR_CACHE_SIZE; i++)
    g_free (cache->entries[i].attrs);

  g_slice_free (GtkTextLogAttrCache, cache);
}

static void
clear_log_attr_cache (GtkTextLogAttrCache *cache)
{
  gint i;

  for (i = 0; i < ATTR_CACHE_SIZE; i++)
    {
      g_free (cache->entries[i].attrs);
      cache->entries[i].attrs = NULL;
    }
}

static PangoLogAttr*
compute_log_attrs (const GtkTextIter *iter,
                   gint              *char_lenp)
{
  GtkTextIter start;
  GtkTextIter end;
  gchar *paragraph;
  gint char_len, byte_len;
  PangoLogAttr *attrs = NULL;
  
  start = *iter;
  end = *iter;

  gtk_text_iter_set_line_offset (&start, 0);
  gtk_text_iter_forward_line (&end);

  paragraph = gtk_text_iter_get_slice (&start, &end);
  char_len = g_utf8_strlen (paragraph, -1);
  byte_len = strlen (paragraph);

  if (char_lenp != NULL)
    *char_lenp = char_len;

  attrs = g_new (PangoLogAttr, char_len + 1);

  /* FIXME we need to follow PangoLayout and allow different language
   * tags within the paragraph
   */
  pango_get_log_attrs (paragraph, byte_len, -1,
		       gtk_text_iter_get_language (&start),
                       attrs,
                       char_len + 1);
  
  g_free (paragraph);

  return attrs;
}

/* The return value from this is valid until you call this a second time.
 * Returns (char_len + 1) PangoLogAttr's, one for each text position.
 */
const PangoLogAttr *
_gtk_text_buffer_get_line_log_attrs (GtkTextBuffer     *buffer,
                                     const GtkTextIter *anywhere_in_line,
                                     gint              *char_len)
{
  GtkTextBufferPrivate *priv;
  gint line;
  GtkTextLogAttrCache *cache;
  gint i;
  
  g_return_val_if_fail (GTK_IS_TEXT_BUFFER (buffer), NULL);
  g_return_val_if_fail (anywhere_in_line != NULL, NULL);

  priv = buffer->priv;

  /* FIXME we also need to recompute log attrs if the language tag at
   * the start of a paragraph changes
   */
  
  if (priv->log_attr_cache == NULL)
    {
      priv->log_attr_cache = g_slice_new0 (GtkTextLogAttrCache);
      priv->log_attr_cache->chars_changed_stamp =
        _gtk_text_btree_get_chars_changed_stamp (get_btree (buffer));
    }
  else if (priv->log_attr_cache->chars_changed_stamp !=
           _gtk_text_btree_get_chars_changed_stamp (get_btree (buffer)))
    {
      clear_log_attr_cache (priv->log_attr_cache);
    }
  
  cache = priv->log_attr_cache;
  line = gtk_text_iter_get_line (anywhere_in_line);

  for (i = 0; i < ATTR_CACHE_SIZE; i++)
    {
      if (cache->entries[i].attrs != NULL &&
          cache->entries[i].line == line)
        {
          if (char_len != NULL)
            *char_len = cache->entries[i].char_len;
          return cache->entries[i].attrs;
        }
    }

  /* Not in cache; open up the first cache entry */
  g_free (cache->entries[ATTR_CACHE_SIZE-1].attrs);

  memmove (cache->entries + 1, cache->entries,
           sizeof (CacheEntry) * (ATTR_CACHE_SIZE - 1));

  cache->entries[0].line = line;
  cache->entries[0].attrs = compute_log_attrs (anywhere_in_line,
                                               &cache->entries[0].char_len);

  if (char_len != NULL)
    *char_len = cache->entries[0].char_len;

  return cache->entries[0].attrs;
}

void
_gtk_text_buffer_notify_will_remove_tag (GtkTextBuffer *buffer,
                                         GtkTextTag    *tag)
{
  /* This removes tag from the buffer, but DOESN'T emit the
   * remove-tag signal, because we can't afford to have user
   * code messing things up at this point; the tag MUST be removed
   * entirely.
   */
  if (buffer->priv->btree)
    _gtk_text_btree_notify_will_remove_tag (buffer->priv->btree, tag);
}

/*
 * Debug spew
 */

void
_gtk_text_buffer_spew (GtkTextBuffer *buffer)
{
  _gtk_text_btree_spew (get_btree (buffer));
}

void
_gtk_text_buffer_get_text_before (GtkTextBuffer   *buffer,
                                  AtkTextBoundary  boundary_type,
                                  GtkTextIter     *position,
                                  GtkTextIter     *start,
                                  GtkTextIter     *end)
{
  gint line_number;

  *start = *position;
  *end = *start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_backward_char (start);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (start))
        gtk_text_iter_backward_word_start (start);
      *end = *start;
      gtk_text_iter_backward_word_start (start);
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (start) &&
          !gtk_text_iter_starts_word (start))
        gtk_text_iter_backward_word_start (start);
      while (!gtk_text_iter_ends_word (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      *end = *start;
      gtk_text_iter_backward_word_start (start);
      while (!gtk_text_iter_ends_word (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (start))
        gtk_text_iter_backward_sentence_start (start);
      *end = *start;
      gtk_text_iter_backward_sentence_start (start);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (start) &&
          !gtk_text_iter_starts_sentence (start))
        gtk_text_iter_backward_sentence_start (start);
      while (!gtk_text_iter_ends_sentence (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      *end = *start;
      gtk_text_iter_backward_sentence_start (start);
      while (!gtk_text_iter_ends_sentence (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
      line_number = gtk_text_iter_get_line (start);
      if (line_number == 0)
        {
          gtk_text_buffer_get_iter_at_offset (buffer, start, 0);
        }
      else
        {
          gtk_text_iter_backward_line (start);
          gtk_text_iter_forward_line (start);
        }
      *end = *start;
      gtk_text_iter_backward_line (start);
      break;

    case ATK_TEXT_BOUNDARY_LINE_END:
      line_number = gtk_text_iter_get_line (start);
      if (line_number == 0)
        {
          gtk_text_buffer_get_iter_at_offset (buffer, start, 0);
          *end = *start;
        }
      else
        {
          gtk_text_iter_backward_line (start);
          *end = *start;
          while (!gtk_text_iter_ends_line (start))
            {
              if (!gtk_text_iter_backward_char (start))
                break;
            }
          gtk_text_iter_forward_to_line_end (end);
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

void
_gtk_text_buffer_get_text_at (GtkTextBuffer   *buffer,
                              AtkTextBoundary  boundary_type,
                              GtkTextIter     *position,
                              GtkTextIter     *start,
                              GtkTextIter     *end)
{
  gint line_number;

  *start = *position;
  *end = *start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (end);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (!gtk_text_iter_starts_word (start))
        gtk_text_iter_backward_word_start (start);
      if (gtk_text_iter_inside_word (end))
        gtk_text_iter_forward_word_end (end);
      while (!gtk_text_iter_starts_word (end))
        {
          if (!gtk_text_iter_forward_char (end))
            break;
        }
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      if (gtk_text_iter_inside_word (start) &&
          !gtk_text_iter_starts_word (start))
        gtk_text_iter_backward_word_start (start);
      while (!gtk_text_iter_ends_word (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      gtk_text_iter_forward_word_end (end);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (!gtk_text_iter_starts_sentence (start))
        gtk_text_iter_backward_sentence_start (start);
      if (gtk_text_iter_inside_sentence (end))
        gtk_text_iter_forward_sentence_end (end);
      while (!gtk_text_iter_starts_sentence (end))
        {
          if (!gtk_text_iter_forward_char (end))
            break;
        }
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      if (gtk_text_iter_inside_sentence (start) &&
          !gtk_text_iter_starts_sentence (start))
        gtk_text_iter_backward_sentence_start (start);
      while (!gtk_text_iter_ends_sentence (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      gtk_text_iter_forward_sentence_end (end);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
      line_number = gtk_text_iter_get_line (start);
      if (line_number == 0)
        {
          gtk_text_buffer_get_iter_at_offset (buffer, start, 0);
        }
      else
        {
          gtk_text_iter_backward_line (start);
          gtk_text_iter_forward_line (start);
        }
      gtk_text_iter_forward_line (end);
      break;

    case ATK_TEXT_BOUNDARY_LINE_END:
      line_number = gtk_text_iter_get_line (start);
      if (line_number == 0)
        {
          gtk_text_buffer_get_iter_at_offset (buffer, start, 0);
        }
      else
        {
          gtk_text_iter_backward_line (start);
          gtk_text_iter_forward_line (start);
        }
      while (!gtk_text_iter_ends_line (start))
        {
          if (!gtk_text_iter_backward_char (start))
            break;
        }
      gtk_text_iter_forward_to_line_end (end);
      break;

    default:
      g_assert_not_reached ();
      break;
   }
}

void
_gtk_text_buffer_get_text_after (GtkTextBuffer   *buffer,
                                 AtkTextBoundary  boundary_type,
                                 GtkTextIter     *position,
                                 GtkTextIter     *start,
                                 GtkTextIter     *end)
{
  *start = *position;
  *end = *start;

  switch (boundary_type)
    {
    case ATK_TEXT_BOUNDARY_CHAR:
      gtk_text_iter_forward_char (start);
      gtk_text_iter_forward_chars (end, 2);
      break;

    case ATK_TEXT_BOUNDARY_WORD_START:
      if (gtk_text_iter_inside_word (end))
        gtk_text_iter_forward_word_end (end);
      while (!gtk_text_iter_starts_word (end))
        {
          if (!gtk_text_iter_forward_char (end))
            break;
        }
      *start = *end;
      if (!gtk_text_iter_is_end (end))
        {
          gtk_text_iter_forward_word_end (end);
          while (!gtk_text_iter_starts_word (end))
            {
              if (!gtk_text_iter_forward_char (end))
                break;
            }
        }
      break;

    case ATK_TEXT_BOUNDARY_WORD_END:
      gtk_text_iter_forward_word_end (end);
      *start = *end;
      if (!gtk_text_iter_is_end (end))
        gtk_text_iter_forward_word_end (end);
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_START:
      if (gtk_text_iter_inside_sentence (end))
        gtk_text_iter_forward_sentence_end (end);
      while (!gtk_text_iter_starts_sentence (end))
        {
          if (!gtk_text_iter_forward_char (end))
            break;
        }
      *start = *end;
      if (!gtk_text_iter_is_end (end))
        {
          gtk_text_iter_forward_sentence_end (end);
          while (!gtk_text_iter_starts_sentence (end))
            {
              if (!gtk_text_iter_forward_char (end))
                break;
            }
        }
      break;

    case ATK_TEXT_BOUNDARY_SENTENCE_END:
      gtk_text_iter_forward_sentence_end (end);
      *start = *end;
      if (!gtk_text_iter_is_end (end))
        gtk_text_iter_forward_sentence_end (end);
      break;

    case ATK_TEXT_BOUNDARY_LINE_START:
      gtk_text_iter_forward_line (end);
      *start = *end;
      gtk_text_iter_forward_line (end);
      break;

    case ATK_TEXT_BOUNDARY_LINE_END:
      gtk_text_iter_forward_line (start);
      *end = *start;
      if (!gtk_text_iter_is_end (start))
        {
          while (!gtk_text_iter_ends_line (start))
            {
              if (!gtk_text_iter_backward_char (start))
                break;
            }
          gtk_text_iter_forward_to_line_end (end);
        }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static GtkTextTag *
get_tag_for_attributes (PangoAttrIterator *iter)
{
  PangoAttribute *attr;
  GtkTextTag *tag;

  tag = gtk_text_tag_new (NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_LANGUAGE);
  if (attr)
    g_object_set (tag, "language", pango_language_to_string (((PangoAttrLanguage*)attr)->value), NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FAMILY);
  if (attr)
    g_object_set (tag, "family", ((PangoAttrString*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_STYLE);
  if (attr)
    g_object_set (tag, "style", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_WEIGHT);
  if (attr)
    g_object_set (tag, "weight", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_VARIANT);
  if (attr)
    g_object_set (tag, "variant", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_STRETCH);
  if (attr)
    g_object_set (tag, "stretch", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_SIZE);
  if (attr)
    g_object_set (tag, "size", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FONT_DESC);
  if (attr)
    g_object_set (tag, "font-desc", ((PangoAttrFontDesc*)attr)->desc, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FOREGROUND);
  if (attr)
    {
      PangoColor *color;
      GdkRGBA rgba;
     
      color = &((PangoAttrColor*)attr)->color;
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1.;
      g_object_set (tag, "foreground-rgba", &rgba, NULL);
    };

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_BACKGROUND);
  if (attr)
    {
      PangoColor *color;
      GdkRGBA rgba;
     
      color = &((PangoAttrColor*)attr)->color;
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1.;
      g_object_set (tag, "background-rgba", &rgba, NULL);
    };

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE);
  if (attr)
    g_object_set (tag, "underline", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_UNDERLINE_COLOR);
  if (attr)
    {
      PangoColor *color;
      GdkRGBA rgba;

      color = &((PangoAttrColor*)attr)->color;
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1.;
      g_object_set (tag, "underline-rgba", &rgba, NULL);
    }

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_STRIKETHROUGH);
  if (attr)
    g_object_set (tag, "strikethrough", (gboolean) (((PangoAttrInt*)attr)->value != 0), NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_STRIKETHROUGH_COLOR);
  if (attr)
    {
      PangoColor *color;
      GdkRGBA rgba;

      color = &((PangoAttrColor*)attr)->color;
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1.;
      g_object_set (tag, "strikethrough-rgba", &rgba, NULL);
    }

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_RISE);
  if (attr)
    g_object_set (tag, "rise", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_SCALE);
  if (attr)
    g_object_set (tag, "scale", ((PangoAttrFloat*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FALLBACK);
  if (attr)
    g_object_set (tag, "fallback", (gboolean) (((PangoAttrInt*)attr)->value != 0), NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_LETTER_SPACING);
  if (attr)
    g_object_set (tag, "letter-spacing", ((PangoAttrInt*)attr)->value, NULL);

  attr = pango_attr_iterator_get (iter, PANGO_ATTR_FONT_FEATURES);
  if (attr)
    g_object_set (tag, "font-features", ((PangoAttrString*)attr)->value, NULL);

  return tag;
}

static void
gtk_text_buffer_insert_with_attributes (GtkTextBuffer *buffer,
                                        GtkTextIter   *iter,
                                        const gchar   *text,
                                        PangoAttrList *attributes)
{
  GtkTextMark *mark;
  PangoAttrIterator *attr;
  GtkTextTagTable *tags;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  if (!attributes)
    {
      gtk_text_buffer_insert (buffer, iter, text, -1);
      return;
    }

  /* create mark with right gravity */
  mark = gtk_text_buffer_create_mark (buffer, NULL, iter, FALSE);
  attr = pango_attr_list_get_iterator (attributes);
  tags = gtk_text_buffer_get_tag_table (buffer);

  do
    {
      GtkTextTag *tag;
      gint start, end;

      pango_attr_iterator_range (attr, &start, &end);

      if (end == G_MAXINT) /* last chunk */
        end = start - 1; /* resulting in -1 to be passed to _insert */

      tag = get_tag_for_attributes (attr);
      gtk_text_tag_table_add (tags, tag);

      gtk_text_buffer_insert_with_tags (buffer, iter, text + start, end - start, tag, NULL);

      gtk_text_buffer_get_iter_at_mark (buffer, iter, mark);
    }
  while (pango_attr_iterator_next (attr));
  
  gtk_text_buffer_delete_mark (buffer, mark);
  pango_attr_iterator_destroy (attr);
} 

/**
 * gtk_text_buffer_insert_markup:
 * @buffer: a #GtkTextBuffer
 * @iter: location to insert the markup
 * @markup: a nul-terminated UTF-8 string containing [Pango markup][PangoMarkupFormat]
 * @len: length of @markup in bytes, or -1
 *
 * Inserts the text in @markup at position @iter. @markup will be inserted
 * in its entirety and must be nul-terminated and valid UTF-8. Emits the
 * #GtkTextBuffer::insert-text signal, possibly multiple times; insertion
 * actually occurs in the default handler for the signal. @iter will point
 * to the end of the inserted text on return.
 */
void
gtk_text_buffer_insert_markup (GtkTextBuffer *buffer,
                               GtkTextIter   *iter,
                               const gchar   *markup,
                               gint           len)
{
  PangoAttrList *attributes;
  gchar *text;
  GError *error = NULL;

  if (!pango_parse_markup (markup, len, 0, &attributes, &text, NULL, &error))
    {
      g_warning ("Invalid markup string: %s", error->message);
      g_error_free (error);
      return;
    }

  gtk_text_buffer_insert_with_attributes (buffer, iter, text, attributes);

  pango_attr_list_unref (attributes);
  g_free (text); 
}
