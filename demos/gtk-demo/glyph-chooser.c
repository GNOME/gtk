#include "config.h"

#include "glyph-chooser.h"

#include <gtk/gtk.h>

#include <hb-ot.h>

#define MIN_WEIGHT 100
#define MAX_WEIGHT 1000
#define DEFAULT_WEIGHT 400

struct _GlyphChooser
{
  GtkWidget parent_instance;
  GtkWidget *box;

  GskPath *path1;
  GskPath *path2;

  char *font_file;
  char *text;
  double weight;

  GtkTextBuffer *buffer;
};

struct _GlyphChooserClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_TEXT = 1,
  PROP_FONT_FILE,
  PROP_FONT_BASENAME,
  PROP_WEIGHT,
  PROP_PATH1,
  PROP_PATH2,
  NUM_PROPERTIES
};

static GParamSpec *pspecs[NUM_PROPERTIES] = { 0, };

static float glyph_x0, glyph_y0;

static void
move_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_move_to (builder, glyph_x0 + x, glyph_y0 - y);
}

static void
line_to (hb_draw_funcs_t *dfuncs,
         GskPathBuilder  *builder,
         hb_draw_state_t *st,
         float            x,
         float            y,
         void            *data)
{
  gsk_path_builder_line_to (builder, glyph_x0 + x, glyph_y0 - y);
}

static void
cubic_to (hb_draw_funcs_t *dfuncs,
          GskPathBuilder  *builder,
          hb_draw_state_t *st,
          float            x1,
          float            y1,
          float            x2,
          float            y2,
          float            x3,
          float            y3,
          void            *data)
{
  gsk_path_builder_cubic_to (builder,
                             glyph_x0 + x1, glyph_y0 - y1,
                             glyph_x0 + x2, glyph_y0 - y2,
                             glyph_x0 + x3, glyph_y0 - y3);
}

static void
close_path (hb_draw_funcs_t *dfuncs,
            GskPathBuilder  *builder,
            hb_draw_state_t *st,
            void            *data)
{
  gsk_path_builder_close (builder);
}

static GskPath *
char_to_path (hb_font_t *font,
              gunichar   ch)
{
  hb_codepoint_t glyph;
  hb_glyph_extents_t extents;
  GskPathBuilder *builder;
  hb_draw_funcs_t *funcs;

  builder = gsk_path_builder_new ();

  hb_font_get_nominal_glyph (font, ch, &glyph);
  hb_font_get_glyph_extents (font, glyph, &extents);

  glyph_x0 = 10 + extents.x_bearing;
  glyph_y0 = 10 + extents.y_bearing;

  gsk_path_builder_move_to (builder, extents.x_bearing, - extents.height);

  funcs = hb_draw_funcs_create ();

  hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to, NULL, NULL);
  hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to, NULL, NULL);
  hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to, NULL, NULL);
  hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path, NULL, NULL);

#if HB_VERSION_ATLEAST (7, 0, 0)
  hb_font_draw_glyph (font, glyph, funcs, builder);
#else
  hb_font_get_glyph_shape (font, glyph, func, builder);
#endif

  hb_draw_funcs_destroy (funcs);

  return gsk_path_builder_free_to_path (builder);
}

static char *
newlineify (char *s)
{
  if (*s == '\0')
    return s;

  for (char *p = s + 1; *p; p++)
    {
      if (strchr ("XZzMmLlHhVvCcSsQqTtOoAaa", *p))
        p[-1] = '\n';
    }

  return s;
}

static void text_changed (GtkTextBuffer *buffer, GlyphChooser *self);

static void
update_from_font (GlyphChooser *self)
{
  hb_blob_t *blob;
  hb_face_t *face;
  hb_font_t *font;
  const char *p;
  hb_variation_t wght;
  char *s1, *s2, *text;

  blob = hb_blob_create_from_file (self->font_file);
  face = hb_face_create (blob, 0);
  font = hb_font_create (face);
  wght.tag = HB_OT_TAG_VAR_AXIS_WEIGHT;
  wght.value = self->weight;
  hb_font_set_variations (font, &wght, 1);

  g_clear_pointer (&self->path1, gsk_path_unref);
  g_clear_pointer (&self->path2, gsk_path_unref);

  self->path1 = char_to_path (font, g_utf8_get_char (self->text));
  p = g_utf8_next_char (self->text);
  if (*p)
    self->path2 = char_to_path (font, g_utf8_get_char (p));
  else
    self->path2 = gsk_path_ref (self->path1);

  s1 = newlineify (gsk_path_to_string (self->path1));
  s2 = newlineify (gsk_path_to_string (self->path2));
  text = g_strconcat (s1, "\n\n", s2, NULL);

  g_signal_handlers_block_by_func (self->buffer, text_changed, self);
  gtk_text_buffer_set_text (self->buffer, text, -1);
  g_signal_handlers_unblock_by_func (self->buffer, text_changed, self);

  g_free (s1);
  g_free (s2);
  g_free (text);

  hb_font_destroy (font);
  hb_face_destroy (face);
  hb_blob_destroy (blob);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH1]);
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH2]);
}

static void
glyph_chooser_set_text (GlyphChooser *self,
                        const char   *text)
{
  if (!g_set_str (&self->text, text))
    return;

  update_from_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_TEXT]);
}

static void
glyph_chooser_set_font_file (GlyphChooser *self,
                             const char   *path)
{
  if (!g_set_str (&self->font_file, path))
    return;

  update_from_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FONT_FILE]);
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_FONT_BASENAME]);
}

static void
glyph_chooser_set_weight (GlyphChooser *self,
                          double        weight)
{
  weight = CLAMP (weight, MIN_WEIGHT, MAX_WEIGHT);

  if (self->weight == weight)
    return;

  self->weight = weight;

  update_from_font (self);

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_WEIGHT]);
}

static void
response_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GlyphChooser *self = GLYPH_CHOOSER (data);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, &error);

  if (file)
    {
      glyph_chooser_set_font_file (self, g_file_peek_path (file));
      g_object_unref (file);
    }

  g_object_unref (dialog);
}

static void
filechooser_cb (GtkButton    *button,
                GlyphChooser *self)
{
  GtkFileDialog *dialog;
  GtkRoot *root;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Font");

  root = gtk_widget_get_root (GTK_WIDGET (self));
  gtk_file_dialog_open (dialog, GTK_WINDOW (root), NULL, response_cb, self);
}

static void
text_changed (GtkTextBuffer *buffer,
              GlyphChooser  *self)
{
  GtkTextIter start, end;
  char *text;
  char *p;
  GskPath *path1, *path2;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  text = g_strstrip (text);

  p = strstr (text, "\n\n");
  if (p)
    {
      char *text1, *text2;

      text1 = g_strstrip (g_strndup (text, p - text));
      text2 = g_strstrip (g_strdup (p + 2));

      path1 = gsk_path_parse (text1);
      path2 = gsk_path_parse (text2);

      g_free (text1);
      g_free (text2);
    }
  else
    {
      path1 = gsk_path_parse (text);
      path2 = NULL;
    }

  g_free (text);

  if (!path1)
    path1 = gsk_path_parse ("");
  if (!path2)
    path2 = gsk_path_ref (path1);

  g_clear_pointer (&self->path1, gsk_path_unref);
  g_clear_pointer (&self->path2, gsk_path_unref);

  self->path1 = path1;
  self->path2 = path2;

  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH1]);
  g_object_notify_by_pspec (G_OBJECT (self), pspecs[PROP_PATH2]);
}

G_DEFINE_TYPE (GlyphChooser, glyph_chooser, GTK_TYPE_WIDGET)

static void
glyph_chooser_init (GlyphChooser *self)
{
  self->font_file = g_strdup ("/usr/share/fonts/abattis-cantarell-vf-fonts/Cantarell-VF.otf");
  self->text = g_strdup ("KP");
  self->weight = DEFAULT_WEIGHT;

  gtk_widget_init_template (GTK_WIDGET (self));

  update_from_font (self);
}

static void
glyph_chooser_dispose (GObject *object)
{
  GlyphChooser *self = GLYPH_CHOOSER (object);

  g_clear_pointer (&self->path1, gsk_path_unref);
  g_clear_pointer (&self->path2, gsk_path_unref);
  g_free (self->font_file);
  g_free (self->text);

  gtk_widget_dispose_template (GTK_WIDGET (self), glyph_chooser_get_type ());

  G_OBJECT_CLASS (glyph_chooser_parent_class)->dispose (object);
}

static void
glyph_chooser_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GlyphChooser *self = GLYPH_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, self->text);
      break;

    case PROP_FONT_FILE:
      g_value_set_string (value, self->font_file);
      break;

    case PROP_FONT_BASENAME:
      g_value_take_string (value, g_path_get_basename (self->font_file));
      break;

    case PROP_PATH1:
      g_value_set_boxed (value, self->path1);
      break;

    case PROP_PATH2:
      g_value_set_boxed (value, self->path2);
      break;

    case PROP_WEIGHT:
      g_value_set_double (value, self->weight);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glyph_chooser_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GlyphChooser *self = GLYPH_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      glyph_chooser_set_text (self, g_value_get_string (value));
      break;

    case PROP_FONT_FILE:
      glyph_chooser_set_font_file (self, g_value_get_string (value));
      break;

    case PROP_WEIGHT:
      glyph_chooser_set_weight (self, g_value_get_double (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glyph_chooser_class_init (GlyphChooserClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = glyph_chooser_dispose;
  object_class->set_property = glyph_chooser_set_property;
  object_class->get_property = glyph_chooser_get_property;

  pspecs[PROP_TEXT] = g_param_spec_string ("text", NULL, NULL,
                                           NULL, G_PARAM_READWRITE);
  pspecs[PROP_FONT_FILE] = g_param_spec_string ("font-file", NULL, NULL,
                                                NULL, G_PARAM_READWRITE);
  pspecs[PROP_FONT_BASENAME] = g_param_spec_string ("font-basename", NULL, NULL,
                                                    NULL, G_PARAM_READABLE);

  pspecs[PROP_PATH1] = g_param_spec_boxed ("path1", NULL, NULL,
                                           GSK_TYPE_PATH, G_PARAM_READABLE);
  pspecs[PROP_PATH2] = g_param_spec_boxed ("path2", NULL, NULL,
                                           GSK_TYPE_PATH, G_PARAM_READABLE);

  pspecs[PROP_WEIGHT] = g_param_spec_double ("weight", NULL, NULL,
                                             MIN_WEIGHT, MAX_WEIGHT, DEFAULT_WEIGHT,
                                             G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, pspecs);

  gtk_widget_class_set_template_from_resource (widget_class, "/glyphs/glyph-chooser.ui");
  gtk_widget_class_bind_template_callback (widget_class, filechooser_cb);
  gtk_widget_class_bind_template_callback (widget_class, text_changed);
  gtk_widget_class_bind_template_child (widget_class, GlyphChooser, buffer);
  gtk_widget_class_bind_template_child (widget_class, GlyphChooser, box);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

GtkWidget *
glyph_chooser_new (void)
{
  return g_object_new (glyph_chooser_get_type (), NULL);
}
