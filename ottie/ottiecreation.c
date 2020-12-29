/*
 * Copyright © 2020 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "ottiecreationprivate.h"

#include "ottielayerprivate.h"
#include "ottieparserprivate.h"
#include "ottiecompositionprivate.h"
#include "ottieprinterprivate.h"
#include "ottiegroupshapeprivate.h"
#include "ottiefontprivate.h"
#include "ottiecharprivate.h"

#include <glib/gi18n-lib.h>
#include <json-glib/json-glib.h>

/**
 * SECTION:ottiecreation
 * @Title: Creation
 * @Short_description: Top-level object for Lottie animations
 *
 * OttieCreation is the top-level object which holds a Lottie
 * animation. You can create an OttieCreation by loading a Lottie
 * file with otte_creation_new_for_file() or ottie_creation_load_file(),
 * or by parsing a Lottie animation from memory with ottie_creation_load_bytes().
 *
 * OttieCreation provides some general information about the loaded
 * animation, such as a name, the frame rate, start and end frames
 * and the dimensions.
 */
struct _OttieCreation
{
  GObject parent;

  char *name;
  double frame_rate;
  double start_frame;
  double end_frame;
  double width;
  double height;

  OttieComposition *layers;
  GHashTable *composition_assets;
  GHashTable *fonts;
  GHashTable *chars;

  GCancellable *cancellable;
};

struct _OttieCreationClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_END_FRAME,
  PROP_FRAME_RATE,
  PROP_HEIGHT,
  PROP_LOADING,
  PROP_NAME,
  PROP_PREPARED,
  PROP_START_FRAME,
  PROP_WIDTH,

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE (OttieCreation, ottie_creation, G_TYPE_OBJECT)

static void
ottie_creation_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)

{
  //OttieCreation *self = OTTIE_CREATION (object);

  switch (prop_id)
    {

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_creation_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  OttieCreation *self = OTTIE_CREATION (object);

  switch (prop_id)
    {
    case PROP_END_FRAME:
      g_value_set_double (value, self->end_frame);
      break;

    case PROP_FRAME_RATE:
      g_value_set_double (value, self->frame_rate);
      break;

    case PROP_HEIGHT:
      g_value_set_double (value, self->height);
      break;

    case PROP_PREPARED:
      g_value_set_boolean (value, self->cancellable != NULL);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_START_FRAME:
      g_value_set_double (value, self->start_frame);
      break;

    case PROP_WIDTH:
      g_value_set_double (value, self->width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
ottie_creation_stop_loading (OttieCreation *self,
                             gboolean       emit)
{
  if (self->cancellable == NULL)
    return;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (emit)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);
}

static void
ottie_creation_reset (OttieCreation *self)
{
  g_clear_object (&self->layers);
  g_hash_table_remove_all (self->composition_assets);
  g_hash_table_remove_all (self->fonts);
  g_hash_table_remove_all (self->chars);

  g_clear_pointer (&self->name, g_free);
  self->frame_rate = 0;
  self->start_frame = 0;
  self->end_frame = 0;
  self->width = 0;
  self->height = 0;
}

static void
ottie_creation_dispose (GObject *object)
{
  OttieCreation *self = OTTIE_CREATION (object);

  ottie_creation_stop_loading (self, FALSE);
  ottie_creation_reset (self);

  G_OBJECT_CLASS (ottie_creation_parent_class)->dispose (object);
}

static void
ottie_creation_finalize (GObject *object)
{
  OttieCreation *self = OTTIE_CREATION (object);

  g_hash_table_unref (self->composition_assets);
  g_hash_table_unref (self->fonts);
  g_hash_table_unref (self->chars);

  G_OBJECT_CLASS (ottie_creation_parent_class)->finalize (object);
}

static void
ottie_creation_class_init (OttieCreationClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = ottie_creation_set_property;
  gobject_class->get_property = ottie_creation_get_property;
  gobject_class->finalize = ottie_creation_finalize;
  gobject_class->dispose = ottie_creation_dispose;

  /**
   * OttieCreation:end-frame:
   *
   * End frame of the creation
   */
  properties[PROP_END_FRAME] =
    g_param_spec_double ("end-frame",
                         "End frame",
                         "End frame of the creation",
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:loading:
   *
   * Whether the creation is currently loading.
   */
  properties[PROP_LOADING] =
    g_param_spec_boolean ("loading",
                          "Loading",
                          "Whether the creation is currently loading",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:frame-rate:
   *
   * Frame rate of this creation
   */
  properties[PROP_FRAME_RATE] =
    g_param_spec_double ("frame-rate",
                         "Frame rate",
                         "Frame rate of this creation",
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:height:
   *
   * Height of this creation
   */
  properties[PROP_HEIGHT] =
    g_param_spec_double ("height",
                         "Height",
                         "Height of this creation",
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:name:
   *
   * The name of the creation.
   */
  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The name of the creation",
                         NULL,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:prepared:
   *
   * Whether the creation is prepared to render
   */
  properties[PROP_PREPARED] =
    g_param_spec_boolean ("prepared",
                          "Prepared",
                          "Whether the creation is prepared to render",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:start-frame:
   *
   * Start frame of the creation
   */
  properties[PROP_START_FRAME] =
    g_param_spec_double ("start-frame",
                         "Start frame",
                         "Start frame of the creation",
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * OttieCreation:width:
   *
   * Width of this creation
   */
  properties[PROP_WIDTH] =
    g_param_spec_double ("width",
                         "Width",
                         "Width of this creation",
                         0.0, G_MAXDOUBLE, 0.0,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
ottie_creation_init (OttieCreation *self)
{
  self->composition_assets = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  self->fonts = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, (GDestroyNotify)ottie_font_free);
  self->chars = g_hash_table_new_full ((GHashFunc)ottie_char_key_hash,
                                       (GEqualFunc)ottie_char_key_equal,
                                       NULL, (GDestroyNotify)ottie_char_free);
}

/**
 * ottie_creation_is_loading:
 * @self: a #OttieCreation
 *
 * Returns whether @self is still in the process of loading. This may not just involve
 * the creation itself, but also any assets that are a part of the creation.
 *
 * Returns: %TRUE if the creation is loading
 */
gboolean
ottie_creation_is_loading (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), FALSE);

  return self->cancellable != NULL;
}

/**
 * ottie_creation_is_prepared:
 * @self: a #OttieCreation
 *
 * Returns whether @self has successfully loaded a document that it can display.
 *
 * Returns: %TRUE if the creation can be used
 */
gboolean
ottie_creation_is_prepared (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), FALSE);

  return self->frame_rate > 0;
}

/**
 * ottie_creation_get_name:
 * @self: a #OttieCreation
 *
 * Returns the name of the current creation or %NULL if the creation is unnamed.
 *
 * Returns: (allow-none): The name of the creation
 */
const char *
ottie_creation_get_name (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), FALSE);

  return self->name;
}

static void
ottie_creation_emit_error (OttieCreation *self,
                           const GError  *error)
{
  g_print ("Ottie is sad: %s\n", error->message);
}

typedef struct {
  char *id;
  OttieComposition *composition;
} OttieParserAsset;

static gboolean
ottie_creation_parse_asset (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  OttieParserOption options[] = {
    { "id", ottie_parser_option_string, G_STRUCT_OFFSET (OttieParserAsset, id) },
    { "layers", ottie_composition_parse_layers, G_STRUCT_OFFSET (OttieParserAsset, composition) },
  };
  OttieCreation *self = data;
  OttieParserAsset asset = { };
  gboolean result;

  result = ottie_parser_parse_object (reader, "asset", options, G_N_ELEMENTS (options), &asset);

  if (result)
    {
      if (asset.id == NULL)
        ottie_parser_error_syntax (reader, "No name given to asset");
      else if (asset.composition == NULL)
        ottie_parser_error_syntax (reader, "No composition layer or image asset defined for name %s", asset.id);
      else
        g_hash_table_insert (self->composition_assets, g_strdup (asset.id), g_object_ref (asset.composition));
    }
  
  g_clear_pointer (&asset.id, g_free);
  g_clear_object (&asset.composition);

  return result;
}

static gboolean
ottie_creation_parse_assets (JsonReader *reader,
                             gsize       offset,
                             gpointer    data)
{
  return ottie_parser_parse_array (reader, "assets",
                                   0, G_MAXUINT, NULL,
                                   offset, 0,
                                   ottie_creation_parse_asset,
                                   data);
}

static gboolean
ottie_creation_parse_marker (JsonReader *reader,
                             gsize       offset,
                             gpointer    data)
{
  ottie_parser_error_unsupported (reader, "Markers are not implemented yet.");

  return TRUE;
}

static gboolean
ottie_creation_parse_markers (JsonReader *reader,
                              gsize       offset,
                              gpointer    data)
{
  return ottie_parser_parse_array (reader, "markers",
                                   0, G_MAXUINT, NULL,
                                   offset, 0,
                                   ottie_creation_parse_marker,
                                   data);
}

static gboolean
ottie_creation_parse_font (JsonReader *reader,
                           gsize       offset,
                           gpointer    data)
{
  OttieParserOption options[] = {
    { "fName", ottie_parser_option_string, G_STRUCT_OFFSET (OttieFont, name) },
    { "fFamily", ottie_parser_option_string, G_STRUCT_OFFSET (OttieFont, family) },
    { "fStyle", ottie_parser_option_string, G_STRUCT_OFFSET (OttieFont, style) },
    { "ascent", ottie_parser_option_double, G_STRUCT_OFFSET (OttieFont, ascent) },
  };
  OttieCreation *self = data;
  OttieFont font = { };
  gboolean result;

  result = ottie_parser_parse_object (reader, "font", options, G_N_ELEMENTS (options), &font);

  if (result)
    {
      if (font.name == NULL)
        ottie_parser_error_syntax (reader, "No name given to font");
      else if (g_hash_table_contains (self->fonts, font.name))
        ottie_parser_error_syntax (reader, "Duplicate font name: %s", font.name);
      else
        g_hash_table_insert (self->fonts, g_strdup (font.name), ottie_font_copy (&font));
    }

  g_clear_pointer (&font.name, g_free);
  g_clear_pointer (&font.family, g_free);
  g_clear_pointer (&font.style, g_free);

  return result;
}

static gboolean
ottie_creation_parse_font_list (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  return ottie_parser_parse_array (reader, "assets",
                                   0, G_MAXUINT, NULL,
                                   offset, 0,
                                   ottie_creation_parse_font,
                                   data);
}

static gboolean
ottie_creation_parse_fonts (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  OttieParserOption options[] = {
    { "list", ottie_creation_parse_font_list, 0 }
  };

  return ottie_parser_parse_object (reader, "fonts",
                                    options, G_N_ELEMENTS (options),
                                    data);
}

static gboolean
ottie_creation_parse_char_data (JsonReader *reader,
                                gsize       offset,
                                gpointer    data)
{
  OttieParserOption options[] = {
    { "shapes", ottie_group_shape_parse_shapes, 0 }
  };
  OttieChar *ch = data;

  ch->shapes = ottie_group_shape_new ();

  return ottie_parser_parse_object (reader, "char data",
                                    options, G_N_ELEMENTS (options),
                                    ch->shapes);
}

static gboolean
ottie_creation_parse_char (JsonReader *reader,
                           gsize       offset,
                           gpointer    data)
{
  OttieParserOption options[] = {
    { "ch", ottie_parser_option_string, G_STRUCT_OFFSET (OttieCharKey, ch) },
    { "fFamily", ottie_parser_option_string, G_STRUCT_OFFSET (OttieCharKey, family) },
    { "style", ottie_parser_option_string, G_STRUCT_OFFSET (OttieCharKey, style) },
    { "size", ottie_parser_option_double, G_STRUCT_OFFSET (OttieChar, size) },
    { "w", ottie_parser_option_double, G_STRUCT_OFFSET (OttieChar, width) },
    { "data", ottie_creation_parse_char_data, G_STRUCT_OFFSET (OttieChar, shapes) },
  };
  OttieCreation *self = data;
  OttieChar ch = { };
  gboolean result;

  result = ottie_parser_parse_object (reader, "char", options, G_N_ELEMENTS (options), &ch);

  if (result)
    {
      if (ch.key.ch == NULL)
        ottie_parser_error_syntax (reader, "Char without \"ch\"");
      else if (ch.shapes == NULL)
        ottie_parser_error_syntax (reader, "Char without \"data\"");
      else if (g_hash_table_contains (self->chars, &ch))
        ottie_parser_error_syntax (reader, "Duplicate char: %s/%s/%s", ch.key.ch, ch.key.family, ch.key.style);
      else
        g_hash_table_add (self->chars, ottie_char_copy (&ch));
    }

  ottie_char_clear (&ch);

  return result;
}
static gboolean
ottie_creation_parse_chars (JsonReader *reader,
                            gsize       offset,
                            gpointer    data)
{
  return ottie_parser_parse_array (reader, "chars",
                                   0, G_MAXUINT, NULL,
                                   offset, 0,
                                   ottie_creation_parse_char,
                                   data);
}

static gboolean
ottie_creation_load_from_reader (OttieCreation *self,
                                 JsonReader    *reader)
{
  OttieParserOption options[] = {
    { "fr", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCreation, frame_rate) },
    { "w", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCreation, width) },
    { "h", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCreation, height) },
    { "nm", ottie_parser_option_string, G_STRUCT_OFFSET (OttieCreation, name) },
    { "ip", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCreation, start_frame) },
    { "op", ottie_parser_option_double, G_STRUCT_OFFSET (OttieCreation, end_frame) },
    { "ddd", ottie_parser_option_3d, 0 },
    { "v", ottie_parser_option_skip, 0 },
    { "layers", ottie_composition_parse_layers, G_STRUCT_OFFSET (OttieCreation, layers) },
    { "assets", ottie_creation_parse_assets, 0 },
    { "markers", ottie_creation_parse_markers, 0 },
    { "fonts", ottie_creation_parse_fonts, 0 },
    { "chars", ottie_creation_parse_chars, 0 },
  };

  return ottie_parser_parse_object (reader, "toplevel", options, G_N_ELEMENTS (options), self);
}

static void
ottie_creation_update_layers (OttieCreation *self)
{
  GHashTableIter iter;
  gpointer layer;

  g_hash_table_iter_init (&iter, self->composition_assets);

  while (g_hash_table_iter_next (&iter, NULL, &layer))
    ottie_layer_update (layer, self->composition_assets, self->fonts, self->chars);

  ottie_layer_update (OTTIE_LAYER (self->layers), self->composition_assets, self->fonts, self->chars);
}

static void
ottie_creation_notify_prepared (OttieCreation *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PREPARED]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FRAME_RATE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WIDTH]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEIGHT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_START_FRAME]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_END_FRAME]);
}

static gboolean
ottie_creation_load_from_node (OttieCreation *self, 
                               JsonNode      *root)
{
  JsonReader *reader = json_reader_new (root);
  gboolean result;

  result = ottie_creation_load_from_reader (self, reader);

  g_object_unref (reader);

  return result;
}

static void
ottie_creation_load_file_parsed (GObject      *parser,
                                 GAsyncResult *res,
                                 gpointer      data)
{
  OttieCreation *self = data;
  GError *error = NULL;

  if (!json_parser_load_from_stream_finish (JSON_PARSER (parser), res, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      ottie_creation_emit_error (self, error);
      g_error_free (error);
      ottie_creation_stop_loading (self, TRUE);
      return;
    }

  g_object_freeze_notify (G_OBJECT (self));

  if (ottie_creation_load_from_node (self, json_parser_get_root (JSON_PARSER (parser))))
    {
      ottie_creation_update_layers (self);
    }
  else
    {
      ottie_creation_reset (self);
    }

  ottie_creation_stop_loading (self, TRUE);
  ottie_creation_notify_prepared (self);

  g_object_thaw_notify (G_OBJECT (self));
}

static void
ottie_creation_load_file_open (GObject      *file,
                               GAsyncResult *res,
                               gpointer      data)
{
  OttieCreation *self = data;
  GFileInputStream *stream;
  GError *error = NULL;
  JsonParser *parser;

  stream = g_file_read_finish (G_FILE (file), res, &error);
  if (stream == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      ottie_creation_emit_error (self, error);
      g_error_free (error);
      ottie_creation_stop_loading (self, TRUE);
      return;
    }

  parser = json_parser_new ();
  json_parser_load_from_stream_async (parser,
                                      G_INPUT_STREAM (stream), 
                                      self->cancellable,
                                      ottie_creation_load_file_parsed,
                                      self);
  g_object_unref (parser);
}

void
ottie_creation_load_bytes (OttieCreation *self,
                           GBytes        *bytes)
{
  GError *error = NULL;
  JsonParser *parser;

  g_return_if_fail (OTTIE_IS_CREATION (self));
  g_return_if_fail (bytes != NULL);

  g_object_freeze_notify (G_OBJECT (self));

  ottie_creation_stop_loading (self, FALSE);
  ottie_creation_reset (self);

  parser = json_parser_new ();
  if (json_parser_load_from_data (parser,
                                  g_bytes_get_data (bytes, NULL),
                                  g_bytes_get_size (bytes),
                                  &error))
    {
      if (ottie_creation_load_from_node (self, json_parser_get_root (JSON_PARSER (parser))))
        {
          ottie_creation_update_layers (self);
        }
      else
        {
          ottie_creation_reset (self);
        }
    }
  else
    {
      ottie_creation_emit_error (self, error);
      g_error_free (error);
    }

  g_object_unref (parser);

  ottie_creation_notify_prepared (self);

  g_object_thaw_notify (G_OBJECT (self));
}

void
ottie_creation_load_file (OttieCreation *self,
                          GFile         *file)
{
  g_return_if_fail (OTTIE_IS_CREATION (self));
  g_return_if_fail (G_IS_FILE (file));

  g_object_freeze_notify (G_OBJECT (self));

  ottie_creation_stop_loading (self, FALSE);
  if (self->frame_rate)
    {
      ottie_creation_reset (self);
      ottie_creation_notify_prepared (self);
    }

  self->cancellable = g_cancellable_new ();

  g_file_read_async (file,
                     G_PRIORITY_DEFAULT,
                     self->cancellable,
                     ottie_creation_load_file_open,
                     self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LOADING]);

  g_object_thaw_notify (G_OBJECT (self));
}

void
ottie_creation_load_filename (OttieCreation *self,
                              const char    *filename)
{
  GFile *file;

  g_return_if_fail (OTTIE_IS_CREATION (self));
  g_return_if_fail (filename != NULL);

  file = g_file_new_for_path (filename);

  ottie_creation_load_file (self, file);

  g_clear_object (&file);
}

OttieCreation *
ottie_creation_new (void)
{
  return g_object_new (OTTIE_TYPE_CREATION, NULL);
}

OttieCreation *
ottie_creation_new_for_file (GFile *file)
{
  OttieCreation *self;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  self = g_object_new (OTTIE_TYPE_CREATION, NULL);

  ottie_creation_load_file (self, file);

  return self;
}

OttieCreation *
ottie_creation_new_for_filename (const char *filename)
{
  OttieCreation *self;
  GFile *file;

  g_return_val_if_fail (filename != NULL, NULL);

  file = g_file_new_for_path (filename);

  self = ottie_creation_new_for_file (file);

  g_clear_object (&file);

  return self;
}

double
ottie_creation_get_frame_rate (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), 0);

  return self->frame_rate;
}

double
ottie_creation_get_start_frame (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), 0);

  return self->start_frame;
}

double
ottie_creation_get_end_frame (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), 0);

  return self->end_frame;
}

double
ottie_creation_get_width (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), 0);

  return self->width;
}

double
ottie_creation_get_height (OttieCreation *self)
{
  g_return_val_if_fail (OTTIE_IS_CREATION (self), 0);

  return self->height;
}

GskRenderNode *
ottie_creation_snapshot (OttieCreation       *self,
                         OttieRenderObserver *observer,
                         double               timestamp)
{
  GskRenderNode *node;
  OttieRender render;

  if (self->layers == NULL)
    return NULL;

  timestamp = timestamp * self->frame_rate;

  ottie_render_init (&render, observer);

  ottie_render_start (&render, timestamp);

  ottie_layer_render (OTTIE_LAYER (self->layers), &render, timestamp);

  node = ottie_render_end (&render);

  ottie_render_clear (&render);

  return node;
}

OttieComposition *
ottie_creation_get_composition (OttieCreation *self)
{
  return self->layers;
}

static void
ottie_creation_print (OttiePrinter  *printer,
                      OttieCreation *self)
{
  const char *id;
  OttieComposition *composition;
  GHashTableIter iter;
  const char *name;
  OttieFont *font;
  OttieChar *ch;

  ottie_printer_start_object (printer, NULL);

  ottie_printer_add_double (printer, "fr", self->frame_rate);
  ottie_printer_add_double (printer, "w", self->width);
  ottie_printer_add_double (printer, "h", self->height);
  ottie_printer_add_string (printer, "nm", self->name);
  ottie_printer_add_double (printer, "ip", self->start_frame);
  ottie_printer_add_double (printer, "op", self->end_frame);
  ottie_printer_add_int (printer, "ddd", 0);

  ottie_printer_start_array (printer, "assets");
  g_hash_table_iter_init (&iter, self->composition_assets);
  while (g_hash_table_iter_next (&iter, (gpointer *)&id, (gpointer *)&composition))
    {
      ottie_printer_start_object (printer, NULL);
      ottie_printer_add_string (printer, "id", id);
      ottie_printer_start_array (printer, "layers");
      ottie_composition_print (printer, composition);
      ottie_printer_end_array (printer);
      ottie_printer_end_object (printer);
    }
  ottie_printer_end_array (printer);

  ottie_printer_start_array (printer, "layers");
  ottie_composition_print (printer, self->layers);
  ottie_printer_end_array (printer);

  if (g_hash_table_size (self->fonts) > 0)
    {
      ottie_printer_start_object (printer, "fonts");
      ottie_printer_start_array (printer, "list");
      g_hash_table_iter_init (&iter, self->fonts);
      while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&font))
        {
          ottie_printer_start_object (printer, NULL);
          ottie_printer_add_string (printer, "fName", font->name);
          ottie_printer_add_string (printer, "fFamily", font->family);
          ottie_printer_add_string (printer, "fStyle", font->style);
          ottie_printer_add_double (printer, "ascent", font->ascent);
          ottie_printer_end_object (printer);
        }
      ottie_printer_end_array (printer);
      ottie_printer_end_object (printer);
    }

  if (g_hash_table_size (self->chars) > 0)
    {
      ottie_printer_start_array (printer, "chars");
      g_hash_table_iter_init (&iter, self->chars);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&ch))
        {
          ottie_printer_start_object (printer, NULL);
          ottie_printer_add_string (printer, "ch", ch->key.ch);
          ottie_printer_add_string (printer, "fFamily", ch->key.family);
          ottie_printer_add_string (printer, "style", ch->key.style);
          ottie_printer_add_double (printer, "size", ch->size);
          ottie_printer_add_double (printer, "w", ch->width);
          ottie_printer_start_object (printer, "data");
          ottie_group_shape_print_shapes (ch->shapes, "shapes", printer);
          ottie_printer_end_object (printer);
          ottie_printer_end_object (printer);
        }
      ottie_printer_end_array (printer);
    }

  ottie_printer_end_object (printer);
}

GBytes *
ottie_creation_to_bytes (OttieCreation *self)
{
  OttiePrinter p;

  ottie_printer_init (&p);

  ottie_creation_print (&p, self);

  return g_string_free_to_bytes (p.str);
}
