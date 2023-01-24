#include "fontpicker.h"

#include <gtk/gtk.h>
#include <hb-ot.h>
#include <hb-gobject.h>

enum {
  PROP_FACE = 1,
  PROP_FAMILY_NAME,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct _FontPicker
{
  GtkWidget parent;

  GtkWidget *button;
  hb_face_t *face;
};

struct _FontPickerClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (FontPicker, font_picker, GTK_TYPE_WIDGET);

static char *
get_family_name (FontPicker *self)
{
  char *name;
  unsigned int len;

  len = hb_ot_name_get_utf8 (self->face, HB_OT_NAME_ID_FONT_FAMILY, HB_LANGUAGE_INVALID, NULL, NULL);
  len++;
  name = g_new (char, len);
  hb_ot_name_get_utf8 (self->face, HB_OT_NAME_ID_FONT_FAMILY, HB_LANGUAGE_INVALID, &len, name);

  return name;
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  FontPicker *self = FONT_PICKER (data);
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      font_picker_set_from_file (self, g_file_peek_path (file));
      g_object_unref (file);
    }
}

static void
show_file_open (FontPicker *self)
{
  GtkFileFilter *filter;
  GtkFileDialog *dialog;
  GListStore *filters;
  GFile *folder;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open Font");

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Font Files");
  gtk_file_filter_add_suffix (filter, "ttf");
  gtk_file_filter_add_suffix (filter, "otf");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);
  folder = g_file_new_for_path ("/usr/share/fonts");
  gtk_file_dialog_set_initial_folder (dialog, folder);
  g_object_unref (folder);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))),
                        NULL,
                        open_response_cb, self);
}

static void
font_picker_init (FontPicker *self)
{
  self->button = gtk_button_new_with_label ("Font");
  gtk_widget_set_hexpand (self->button, TRUE);
  gtk_widget_set_parent (GTK_WIDGET (self->button), GTK_WIDGET (self));

  g_signal_connect_swapped (self->button, "clicked", G_CALLBACK (show_file_open), self);
}

static void
font_picker_dispose (GObject *object)
{
  FontPicker *self = FONT_PICKER (object);

  g_clear_pointer ((GtkWidget **)&self->button, gtk_widget_unparent);

  G_OBJECT_CLASS (font_picker_parent_class)->dispose (object);
}

static void
font_picker_finalize (GObject *object)
{
  FontPicker *self = FONT_PICKER (object);

  g_clear_pointer (&self->face, hb_face_destroy);

  G_OBJECT_CLASS (font_picker_parent_class)->finalize (object);
}

static void
font_picker_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  FontPicker *self = FONT_PICKER (object);

  switch (prop_id)
    {
    case PROP_FACE:
      font_picker_set_face (self, (hb_face_t *) g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_picker_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  FontPicker *self = FONT_PICKER (object);

  switch (prop_id)
    {
    case PROP_FACE:
      g_value_set_boxed (value, self->face);
      break;

    case PROP_FAMILY_NAME:
      g_value_take_string (value, get_family_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_picker_class_init (FontPickerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = font_picker_dispose;
  object_class->finalize = font_picker_finalize;
  object_class->get_property = font_picker_get_property;
  object_class->set_property = font_picker_set_property;

  properties[PROP_FACE] =
      g_param_spec_boxed ("face", "", "",
                          HB_GOBJECT_TYPE_FACE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_FAMILY_NAME] =
      g_param_spec_string ("family-name", "", "",
                           NULL,
                           G_PARAM_READABLE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_layout_manager_type (GTK_WIDGET_CLASS (class), GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "fontpicker");
}

void
font_picker_set_face (FontPicker *self,
                       hb_face_t   *face)
{
  if (self->face == face)
    return;

  if (self->face)
    hb_face_destroy (self->face);
  self->face = face;
  if (self->face)
    hb_face_reference (self->face);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACE]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FAMILY_NAME]);
}

void
font_picker_set_from_file (FontPicker *self,
                           const char *path)
{
  hb_blob_t *blob;
  hb_face_t *face;

  blob = hb_blob_create_from_file (path);
  face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);

  font_picker_set_face (self, face);

  hb_face_destroy (face);
}
