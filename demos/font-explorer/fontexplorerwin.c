#include "fontexplorerwin.h"

#include "fontcolors.h"
#include "fontcontrols.h"
#include "fontexplorerapp.h"
#include "fontfeatures.h"
#include "fontvariations.h"
#include "glyphsview.h"
#include "infoview.h"
#include "plainview.h"
#include "samplechooser.h"
#include "sampleeditor.h"
#include "styleview.h"
#include "waterfallview.h"

#include <gtk/gtk.h>
#include <string.h>


struct _FontExplorerWindow
{
  GtkApplicationWindow parent;

  Pango2FontMap *font_map;

  GtkFontButton *fontbutton;
  GtkLabel *path;
  FontControls *controls;
  FontFeatures *features;
  FontVariations *variations;
  FontColors *colors;
  GtkStack *stack;
  GtkToggleButton *plain_toggle;
  GtkToggleButton *waterfall_toggle;
  GtkToggleButton *style_toggle;
  GtkToggleButton *glyphs_toggle;
  GtkToggleButton *info_toggle;
  GtkToggleButton *edit_toggle;
};

struct _FontExplorerWindowClass
{
  GtkApplicationWindowClass parent_class;
};

enum {
  PROP_FONT_MAP = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(FontExplorerWindow, font_explorer_window, GTK_TYPE_APPLICATION_WINDOW);

static void
reset (GSimpleAction      *action,
       GVariant           *parameter,
       FontExplorerWindow *self)
{
  g_action_activate (font_controls_get_reset_action (self->controls), NULL);
  g_action_activate (font_features_get_reset_action (self->features), NULL);
  g_action_activate (font_variations_get_reset_action (self->variations), NULL);
  g_action_activate (font_colors_get_reset_action (self->colors), NULL);
}

static void
update_reset (GSimpleAction      *action,
              GParamSpec         *pspec,
              FontExplorerWindow *self)
{
  gboolean enabled;
  GAction *reset_action;

  enabled = g_action_get_enabled (font_controls_get_reset_action (self->controls)) ||
            g_action_get_enabled (font_features_get_reset_action (self->features)) ||
            g_action_get_enabled (font_variations_get_reset_action (self->variations)) ||
            g_action_get_enabled (font_colors_get_reset_action (self->colors));

  reset_action = g_action_map_lookup_action (G_ACTION_MAP (self), "reset");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (reset_action), enabled);
}

static void
font_explorer_window_init (FontExplorerWindow *self)
{
  GSimpleAction *reset_action;

  self->font_map = g_object_ref (pango2_font_map_get_default ());

  gtk_widget_init_template (GTK_WIDGET (self));

  reset_action = g_simple_action_new ("reset", NULL);
  g_signal_connect (reset_action, "activate", G_CALLBACK (reset), self);
  g_signal_connect (font_controls_get_reset_action (self->controls),
                    "notify::enabled", G_CALLBACK (update_reset), self);
  g_signal_connect (font_variations_get_reset_action (self->variations),
                    "notify::enabled", G_CALLBACK (update_reset), self);
  g_signal_connect (font_colors_get_reset_action (self->colors),
                    "notify::enabled", G_CALLBACK (update_reset), self);
  g_signal_connect (font_features_get_reset_action (self->features),
                    "notify::enabled", G_CALLBACK (update_reset), self);

  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (reset_action));
  update_reset (NULL, NULL, self);
}

static void
update_view (GtkToggleButton    *button,
             FontExplorerWindow *self)
{
  if (gtk_toggle_button_get_active (self->edit_toggle))
    gtk_stack_set_visible_child_name (self->stack, "edit");
  else if (gtk_toggle_button_get_active (self->plain_toggle))
    gtk_stack_set_visible_child_name (self->stack, "plain");
  else if (gtk_toggle_button_get_active (self->waterfall_toggle))
    gtk_stack_set_visible_child_name (self->stack, "waterfall");
  else if (gtk_toggle_button_get_active (self->style_toggle))
    gtk_stack_set_visible_child_name (self->stack, "style");
  else if (gtk_toggle_button_get_active (self->glyphs_toggle))
    gtk_stack_set_visible_child_name (self->stack, "glyphs");
  else if (gtk_toggle_button_get_active (self->info_toggle))
    gtk_stack_set_visible_child_name (self->stack, "info");
}

static void
font_explorer_window_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  FontExplorerWindow *self = FONT_EXPLORER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_set_object (&self->font_map, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_explorer_window_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  FontExplorerWindow *self = FONT_EXPLORER_WINDOW (object);

  switch (prop_id)
    {
    case PROP_FONT_MAP:
      g_value_set_object (value, self->font_map);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
font_explorer_window_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), FONT_EXPLORER_WINDOW_TYPE);

  G_OBJECT_CLASS (font_explorer_window_parent_class)->dispose (object);
}

static void
font_explorer_window_finalize (GObject *object)
{
  FontExplorerWindow *self = FONT_EXPLORER_WINDOW (object);

  g_clear_object (&self->font_map);

  G_OBJECT_CLASS (font_explorer_window_parent_class)->finalize (object);
}

static void
font_explorer_window_class_init (FontExplorerWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (FONT_COLORS_TYPE);
  g_type_ensure (FONT_CONTROLS_TYPE);
  g_type_ensure (FONT_FEATURES_TYPE);
  g_type_ensure (FONT_VARIATIONS_TYPE);
  g_type_ensure (GLYPHS_VIEW_TYPE);
  g_type_ensure (INFO_VIEW_TYPE);
  g_type_ensure (PLAIN_VIEW_TYPE);
  g_type_ensure (SAMPLE_CHOOSER_TYPE);
  g_type_ensure (SAMPLE_EDITOR_TYPE);
  g_type_ensure (STYLE_VIEW_TYPE);
  g_type_ensure (WATERFALL_VIEW_TYPE);

  object_class->set_property = font_explorer_window_set_property;
  object_class->get_property = font_explorer_window_get_property;
  object_class->dispose = font_explorer_window_dispose;
  object_class->finalize = font_explorer_window_finalize;

  properties[PROP_FONT_MAP] =
      g_param_spec_object ("font-map", "", "",
                           PANGO2_TYPE_FONT_MAP,
                           G_PARAM_READWRITE);

  g_object_class_install_properties (G_OBJECT_CLASS (class), NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontexplorerwin.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, fontbutton);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, path);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, controls);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, variations);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, colors);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, features);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, stack);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, plain_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, waterfall_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, style_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, glyphs_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, info_toggle);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, edit_toggle);

  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), update_view);
}

FontExplorerWindow *
font_explorer_window_new (FontExplorerApp *app)
{
  return g_object_new (FONT_EXPLORER_WINDOW_TYPE, "application", app, NULL);
}

void
font_explorer_window_load (FontExplorerWindow *self,
                           GFile              *file)
{
  const char *path;
  Pango2FontMap *map;
  Pango2HbFace *face;
  Pango2FontDescription *desc;
  char *basename;

  path = g_file_peek_path (file);
  basename = g_path_get_basename (path);

  face = pango2_hb_face_new_from_file (path, 0, -2, NULL, NULL);
  desc = pango2_font_face_describe (PANGO2_FONT_FACE (face));

  map = pango2_font_map_new ();
  pango2_font_map_add_face (map, PANGO2_FONT_FACE (face));
  pango2_font_map_set_fallback (map, pango2_font_map_get_default ());

  g_set_object (&self->font_map, map);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FONT_MAP]);

  g_object_unref (map);

  gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (self->fontbutton), desc);

  gtk_widget_hide (GTK_WIDGET (self->fontbutton));
  gtk_widget_show (GTK_WIDGET (self->path));
  gtk_label_set_label (self->path, basename);

  pango2_font_description_free (desc);
  g_free (basename);
}
