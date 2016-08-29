/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "gtkcolorsel.h"

#include <math.h>
#include <string.h>

#include "gdk/gdk.h"
#include "gtkadjustment.h"
#include "gtkorientable.h"
#include "gtkhsv.h"
#include "gtkwindowgroup.h"
#include "gtkselection.h"
#include "gtkcolorutils.h"
#include "gtkdnd.h"
#include "gtkdragsource.h"
#include "gtkdragdest.h"
#include "gtkdrawingarea.h"
#include "gtkframe.h"
#include "gtkgrid.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkspinbutton.h"
#include "gtkrange.h"
#include "gtkscale.h"
#include "gtkentry.h"
#include "gtkbutton.h"
#include "gtkmenuitem.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtkstock.h"
#include "gtkaccessible.h"
#include "gtksizerequest.h"
#include "gtkseparator.h"
#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkcolorsel
 * @Short_description: Deprecated widget used to select a color
 * @Title: GtkColorSelection
 *
 * The #GtkColorSelection is a widget that is used to select
 * a color.  It consists of a color wheel and number of sliders
 * and entry boxes for color parameters such as hue, saturation,
 * value, red, green, blue, and opacity.  It is found on the standard
 * color selection dialog box #GtkColorSelectionDialog.
 */


/* Keep it in sync with gtksettings.c:default_color_palette */
#define DEFAULT_COLOR_PALETTE   "black:white:gray50:red:purple:blue:light blue:green:yellow:orange:lavender:brown:goldenrod4:dodger blue:pink:light green:gray10:gray30:gray75:gray90"

/* Number of elements in the custom palatte */
#define GTK_CUSTOM_PALETTE_WIDTH 10
#define GTK_CUSTOM_PALETTE_HEIGHT 2

#define CUSTOM_PALETTE_ENTRY_WIDTH   20
#define CUSTOM_PALETTE_ENTRY_HEIGHT  20

/* The cursor for the dropper */
#define DROPPER_WIDTH 17
#define DROPPER_HEIGHT 17
#define DROPPER_STRIDE (DROPPER_WIDTH * 4)
#define DROPPER_X_HOT 2
#define DROPPER_Y_HOT 16

#define SAMPLE_WIDTH  64
#define SAMPLE_HEIGHT 28
#define CHECK_SIZE 16
#define BIG_STEP 20

/* Conversion between 0->1 double and and guint16. See
 * scale_round() below for more general conversions
 */
#define SCALE(i) (i / 65535.)
#define UNSCALE(d) ((guint16)(d * 65535 + 0.5))
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

enum {
  COLOR_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_HAS_PALETTE,
  PROP_HAS_OPACITY_CONTROL,
  PROP_CURRENT_COLOR,
  PROP_CURRENT_ALPHA,
  PROP_CURRENT_RGBA
};

enum {
  COLORSEL_RED = 0,
  COLORSEL_GREEN = 1,
  COLORSEL_BLUE = 2,
  COLORSEL_OPACITY = 3,
  COLORSEL_HUE,
  COLORSEL_SATURATION,
  COLORSEL_VALUE,
  COLORSEL_NUM_CHANNELS
};


struct _GtkColorSelectionPrivate
{
  guint has_opacity       : 1;
  guint has_palette       : 1;
  guint changing          : 1;
  guint default_set       : 1;
  guint default_alpha_set : 1;
  guint has_grab          : 1;

  gdouble color[COLORSEL_NUM_CHANNELS];
  gdouble old_color[COLORSEL_NUM_CHANNELS];

  GtkWidget *triangle_colorsel;
  GtkWidget *hue_spinbutton;
  GtkWidget *sat_spinbutton;
  GtkWidget *val_spinbutton;
  GtkWidget *red_spinbutton;
  GtkWidget *green_spinbutton;
  GtkWidget *blue_spinbutton;
  GtkWidget *opacity_slider;
  GtkWidget *opacity_label;
  GtkWidget *opacity_entry;
  GtkWidget *palette_frame;
  GtkWidget *hex_entry;

  /* The Palette code */
  GtkWidget *custom_palette [GTK_CUSTOM_PALETTE_WIDTH][GTK_CUSTOM_PALETTE_HEIGHT];

  /* The color_sample stuff */
  GtkWidget *sample_area;
  GtkWidget *old_sample;
  GtkWidget *cur_sample;
  GtkWidget *colorsel;

  /* Window for grabbing on */
  GtkWidget *dropper_grab_widget;
  guint32    grab_time;
  GdkDevice *keyboard_device;
  GdkDevice *pointer_device;

  /* Connection to settings */
  gulong settings_connection;
};


static void gtk_color_selection_destroy         (GtkWidget               *widget);
static void gtk_color_selection_finalize        (GObject                 *object);
static void update_color                        (GtkColorSelection       *colorsel);
static void gtk_color_selection_set_property    (GObject                 *object,
                                                 guint                    prop_id,
                                                 const GValue            *value,
                                                 GParamSpec              *pspec);
static void gtk_color_selection_get_property    (GObject                 *object,
                                                 guint                    prop_id,
                                                 GValue                  *value,
                                                 GParamSpec              *pspec);

static void gtk_color_selection_realize         (GtkWidget               *widget);
static void gtk_color_selection_unrealize       (GtkWidget               *widget);
static void gtk_color_selection_show_all        (GtkWidget               *widget);
static gboolean gtk_color_selection_grab_broken (GtkWidget               *widget,
                                                 GdkEventGrabBroken      *event);

static void     gtk_color_selection_set_palette_color   (GtkColorSelection *colorsel,
                                                         gint               index,
                                                         GdkColor          *color);
static void     set_focus_line_attributes               (GtkWidget         *drawing_area,
                                                         cairo_t           *cr,
                                                         gint              *focus_width);
static void     default_noscreen_change_palette_func    (const GdkColor    *colors,
                                                         gint               n_colors);
static void     default_change_palette_func             (GdkScreen         *screen,
                                                         const GdkColor    *colors,
                                                         gint               n_colors);
static void     make_control_relations                  (AtkObject         *atk_obj,
                                                         GtkWidget         *widget);
static void     make_all_relations                      (AtkObject         *atk_obj,
                                                         GtkColorSelectionPrivate *priv);

static void     hsv_changed                             (GtkWidget         *hsv,
                                                         gpointer           data);
static void     get_screen_color                        (GtkWidget         *button);
static void     adjustment_changed                      (GtkAdjustment     *adjustment,
                                                         gpointer           data);
static void     opacity_entry_changed                   (GtkWidget         *opacity_entry,
                                                         gpointer           data);
static void     hex_changed                             (GtkWidget         *hex_entry,
                                                         gpointer           data);
static gboolean hex_focus_out                           (GtkWidget         *hex_entry,
                                                         GdkEventFocus     *event,
                                                         gpointer           data);
static void     color_sample_new                        (GtkColorSelection *colorsel);
static void     make_label_spinbutton                   (GtkColorSelection *colorsel,
                                                         GtkWidget        **spinbutton,
                                                         gchar             *text,
                                                         GtkWidget         *table,
                                                         gint               i,
                                                         gint               j,
                                                         gint               channel_type,
                                                         const gchar       *tooltip);
static void     make_palette_frame                      (GtkColorSelection *colorsel,
                                                         GtkWidget         *table,
                                                         gint               i,
                                                         gint               j);
static void     set_selected_palette                    (GtkColorSelection *colorsel,
                                                         int                x,
                                                         int                y);
static void     set_focus_line_attributes               (GtkWidget         *drawing_area,
                                                         cairo_t           *cr,
                                                         gint              *focus_width);
static gboolean mouse_press                             (GtkWidget         *invisible,
                                                         GdkEventButton    *event,
                                                         gpointer           data);
static void  palette_change_notify_instance (GObject    *object,
                                             GParamSpec *pspec,
                                             gpointer    data);
static void update_palette (GtkColorSelection *colorsel);
static void shutdown_eyedropper (GtkWidget *widget);

static guint color_selection_signals[LAST_SIGNAL] = { 0 };

static GtkColorSelectionChangePaletteFunc noscreen_change_palette_hook = default_noscreen_change_palette_func;
static GtkColorSelectionChangePaletteWithScreenFunc change_palette_hook = default_change_palette_func;

static const guchar dropper_bits[] = {
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\377\377\377\377\377\377"
  "\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\0\0\0\377"
  "\0\0\0\377\0\0\0\377\377\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377"
  "\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\377\377\377\377"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377"
  "\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\377\0\0"
  "\0\377\0\0\0\377\0\0\0\377\377\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\0\0\0\377\0\0\0\377\0"
  "\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\377\377\377"
  "\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\377\377\377\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0\0\0\377\0"
  "\0\0\377\377\377\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\377\0\0\0\377\0\0"
  "\0\377\0\0\0\377\377\377\377\377\377\377\377\377\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377"
  "\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\377\377\377"
  "\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\377\377\377\377\377\377\377\377\377\377\377\377\377"
  "\0\0\0\377\377\377\377\377\0\0\0\377\377\377\377\377\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377"
  "\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\0\0\0\0\0\377\377"
  "\377\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\377\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0"
  "\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\377\377\377\377\377\377"
  "\377\377\377\0\0\0\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\377"
  "\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\377\377\377\377\377\377\377\377\377\377\377\377\377\0\0\0\377\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\377\377\377\377\377\377\377\377\0\0"
  "\0\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\0\0\0\0\0\0\0\377\0\0\0"
  "\377\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\377\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
  "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"};

G_DEFINE_TYPE_WITH_PRIVATE (GtkColorSelection, gtk_color_selection, GTK_TYPE_BOX)

static void
gtk_color_selection_class_init (GtkColorSelectionClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gtk_color_selection_finalize;
  gobject_class->set_property = gtk_color_selection_set_property;
  gobject_class->get_property = gtk_color_selection_get_property;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->destroy = gtk_color_selection_destroy;
  widget_class->realize = gtk_color_selection_realize;
  widget_class->unrealize = gtk_color_selection_unrealize;
  widget_class->show_all = gtk_color_selection_show_all;
  widget_class->grab_broken_event = gtk_color_selection_grab_broken;

  g_object_class_install_property (gobject_class,
                                   PROP_HAS_OPACITY_CONTROL,
                                   g_param_spec_boolean ("has-opacity-control",
                                                         P_("Has Opacity Control"),
                                                         P_("Whether the color selector should allow setting opacity"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
                                   PROP_HAS_PALETTE,
                                   g_param_spec_boolean ("has-palette",
                                                         P_("Has palette"),
                                                         P_("Whether a palette should be used"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkColorSelection:current-color:
   *
   * The current GdkColor color.
   *
   * Deprecated: 3.4: Use #GtkColorSelection:current-rgba instead.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_COLOR,
                                   g_param_spec_boxed ("current-color",
                                                       P_("Current Color"),
                                                       P_("The current color"),
                                                       GDK_TYPE_COLOR,
                                                       GTK_PARAM_READWRITE | G_PARAM_DEPRECATED));
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_ALPHA,
                                   g_param_spec_uint ("current-alpha",
                                                      P_("Current Alpha"),
                                                      P_("The current opacity value (0 fully transparent, 65535 fully opaque)"),
                                                      0, 65535, 65535,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkColorSelection:current-rgba:
   *
   * The current RGBA color.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CURRENT_RGBA,
                                   g_param_spec_boxed ("current-rgba",
                                                       P_("Current RGBA"),
                                                       P_("The current RGBA color"),
                                                       GDK_TYPE_RGBA,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkColorSelection::color-changed:
   * @colorselection: the object which received the signal.
   *
   * This signal is emitted when the color changes in the #GtkColorSelection
   * according to its update policy.
   */
  color_selection_signals[COLOR_CHANGED] =
    g_signal_new (I_("color-changed"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkColorSelectionClass, color_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_color_selection_init (GtkColorSelection *colorsel)
{
  GtkWidget *top_hbox;
  GtkWidget *top_right_vbox;
  GtkWidget *table, *label, *hbox, *frame, *vbox, *button;
  GtkAdjustment *adjust;
  GtkWidget *picker_image;
  gint i, j;
  GtkColorSelectionPrivate *priv;
  AtkObject *atk_obj;
  GList *focus_chain = NULL;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (colorsel),
                                  GTK_ORIENTATION_VERTICAL);

  gtk_widget_push_composite_child ();

  priv = colorsel->private_data = gtk_color_selection_get_instance_private (colorsel);
  priv->changing = FALSE;
  priv->default_set = FALSE;
  priv->default_alpha_set = FALSE;

  top_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start (GTK_BOX (colorsel), top_hbox, FALSE, FALSE, 0);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  priv->triangle_colorsel = gtk_hsv_new ();
  g_signal_connect (priv->triangle_colorsel, "changed",
                    G_CALLBACK (hsv_changed), colorsel);
  gtk_hsv_set_metrics (GTK_HSV (priv->triangle_colorsel), 174, 15);
  gtk_box_pack_start (GTK_BOX (top_hbox), vbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), priv->triangle_colorsel, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (priv->triangle_colorsel,
                        _("Select the color you want from the outer ring. "
                          "Select the darkness or lightness of that color "
                          "using the inner triangle."));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (frame, -1, 30);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  color_sample_new (colorsel);
  gtk_container_add (GTK_CONTAINER (frame), priv->sample_area);
  gtk_box_pack_start (GTK_BOX (hbox), frame, TRUE, TRUE, 0);

  button = gtk_button_new ();

  gtk_widget_set_events (button, GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
  g_object_set_data (G_OBJECT (button), I_("COLORSEL"), colorsel);
  g_signal_connect (button, "clicked",
                    G_CALLBACK (get_screen_color), NULL);
  picker_image = gtk_image_new_from_stock (GTK_STOCK_COLOR_PICKER, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), picker_image);
  gtk_widget_show (GTK_WIDGET (picker_image));
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gtk_widget_set_tooltip_text (button,
                        _("Click the eyedropper, then click a color "
                          "anywhere on your screen to select that color."));

  top_right_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start (GTK_BOX (top_hbox), top_right_vbox, FALSE, FALSE, 0);
  table = gtk_grid_new ();
  gtk_box_pack_start (GTK_BOX (top_right_vbox), table, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing (GTK_GRID (table), 6);
  gtk_grid_set_column_spacing (GTK_GRID (table), 12);

  make_label_spinbutton (colorsel, &priv->hue_spinbutton, _("_Hue:"), table, 0, 0, COLORSEL_HUE,
                         _("Position on the color wheel."));
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (priv->hue_spinbutton), TRUE);
  make_label_spinbutton (colorsel, &priv->sat_spinbutton, _("S_aturation:"), table, 0, 1, COLORSEL_SATURATION,
                         _("Intensity of the color."));
  make_label_spinbutton (colorsel, &priv->val_spinbutton, _("_Value:"), table, 0, 2, COLORSEL_VALUE,
                         _("Brightness of the color."));
  make_label_spinbutton (colorsel, &priv->red_spinbutton, _("_Red:"), table, 6, 0, COLORSEL_RED,
                         _("Amount of red light in the color."));
  make_label_spinbutton (colorsel, &priv->green_spinbutton, _("_Green:"), table, 6, 1, COLORSEL_GREEN,
                         _("Amount of green light in the color."));
  make_label_spinbutton (colorsel, &priv->blue_spinbutton, _("_Blue:"), table, 6, 2, COLORSEL_BLUE,
                         _("Amount of blue light in the color."));
  gtk_grid_attach (GTK_GRID (table), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), 0, 3, 8, 1);

  priv->opacity_label = gtk_label_new_with_mnemonic (_("Op_acity:"));
  gtk_widget_set_halign (priv->opacity_label, GTK_ALIGN_START);
  gtk_widget_set_valign (priv->opacity_label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), priv->opacity_label, 0, 4, 1, 1);
  adjust = gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 0.0);
  g_object_set_data (G_OBJECT (adjust), I_("COLORSEL"), colorsel);
  priv->opacity_slider = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjust);
  gtk_widget_set_tooltip_text (priv->opacity_slider,
                        _("Transparency of the color."));
  gtk_label_set_mnemonic_widget (GTK_LABEL (priv->opacity_label),
                                 priv->opacity_slider);
  gtk_scale_set_draw_value (GTK_SCALE (priv->opacity_slider), FALSE);
  g_signal_connect (adjust, "value-changed",
                    G_CALLBACK (adjustment_changed),
                    GINT_TO_POINTER (COLORSEL_OPACITY));
  gtk_grid_attach (GTK_GRID (table), priv->opacity_slider, 1, 4, 6, 1);
  priv->opacity_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (priv->opacity_entry,
                        _("Transparency of the color."));
  gtk_widget_set_size_request (priv->opacity_entry, 40, -1);

  g_signal_connect (priv->opacity_entry, "activate",
                    G_CALLBACK (opacity_entry_changed), colorsel);
  gtk_grid_attach (GTK_GRID (table), priv->opacity_entry, 7, 4, 1, 1);

  label = gtk_label_new_with_mnemonic (_("Color _name:"));
  gtk_grid_attach (GTK_GRID (table), label, 0, 5, 1, 1);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  priv->hex_entry = gtk_entry_new ();

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->hex_entry);

  g_signal_connect (priv->hex_entry, "activate",
                    G_CALLBACK (hex_changed), colorsel);

  g_signal_connect (priv->hex_entry, "focus-out-event",
                    G_CALLBACK (hex_focus_out), colorsel);

  gtk_widget_set_tooltip_text (priv->hex_entry,
                        _("You can enter an HTML-style hexadecimal color "
                          "value, or simply a color name such as “orange” "
                          "in this entry."));

  gtk_entry_set_width_chars (GTK_ENTRY (priv->hex_entry), 7);
  gtk_grid_attach (GTK_GRID (table), priv->hex_entry, 1, 5, 4, 1);

  focus_chain = g_list_append (focus_chain, priv->hue_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->sat_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->val_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->red_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->green_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->blue_spinbutton);
  focus_chain = g_list_append (focus_chain, priv->opacity_slider);
  focus_chain = g_list_append (focus_chain, priv->opacity_entry);
  focus_chain = g_list_append (focus_chain, priv->hex_entry);
  gtk_container_set_focus_chain (GTK_CONTAINER (table), focus_chain);
  g_list_free (focus_chain);

  /* Set up the palette */
  table = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (table), 1);
  gtk_grid_set_column_spacing (GTK_GRID (table), 1);
  for (i = 0; i < GTK_CUSTOM_PALETTE_WIDTH; i++)
    {
      for (j = 0; j < GTK_CUSTOM_PALETTE_HEIGHT; j++)
        {
          make_palette_frame (colorsel, table, i, j);
        }
    }
  set_selected_palette (colorsel, 0, 0);
  priv->palette_frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  label = gtk_label_new_with_mnemonic (_("_Palette:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (priv->palette_frame), label, FALSE, FALSE, 0);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 priv->custom_palette[0][0]);

  gtk_box_pack_end (GTK_BOX (top_right_vbox), priv->palette_frame, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (priv->palette_frame), table, FALSE, FALSE, 0);

  gtk_widget_show_all (top_hbox);

  /* hide unused stuff */

  if (priv->has_opacity == FALSE)
    {
      gtk_widget_hide (priv->opacity_label);
      gtk_widget_hide (priv->opacity_slider);
      gtk_widget_hide (priv->opacity_entry);
    }

  if (priv->has_palette == FALSE)
    {
      gtk_widget_hide (priv->palette_frame);
    }

  atk_obj = gtk_widget_get_accessible (priv->triangle_colorsel);
  if (GTK_IS_ACCESSIBLE (atk_obj))
    {
      atk_object_set_name (atk_obj, _("Color Wheel"));
      atk_object_set_role (gtk_widget_get_accessible (GTK_WIDGET (colorsel)), ATK_ROLE_COLOR_CHOOSER);
      make_all_relations (atk_obj, priv);
    }

  gtk_widget_pop_composite_child ();
}

/* GObject methods */
static void
gtk_color_selection_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_color_selection_parent_class)->finalize (object);
}

static void
gtk_color_selection_set_property (GObject         *object,
                                  guint            prop_id,
                                  const GValue    *value,
                                  GParamSpec      *pspec)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (object);

  switch (prop_id)
    {
    case PROP_HAS_OPACITY_CONTROL:
      gtk_color_selection_set_has_opacity_control (colorsel,
                                                   g_value_get_boolean (value));
      break;
    case PROP_HAS_PALETTE:
      gtk_color_selection_set_has_palette (colorsel,
                                           g_value_get_boolean (value));
      break;
    case PROP_CURRENT_COLOR:
      {
        GdkColor *color;
        GdkRGBA rgba;

        color = g_value_get_boxed (value);

        rgba.red = SCALE (color->red);
        rgba.green = SCALE (color->green);;
        rgba.blue = SCALE (color->blue);
        rgba.alpha = 1.0;

        gtk_color_selection_set_current_rgba (colorsel, &rgba);
      }
      break;
    case PROP_CURRENT_ALPHA:
      gtk_color_selection_set_current_alpha (colorsel, g_value_get_uint (value));
      break;
    case PROP_CURRENT_RGBA:
      gtk_color_selection_set_current_rgba (colorsel, g_value_get_boxed (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
gtk_color_selection_get_property (GObject     *object,
                                  guint        prop_id,
                                  GValue      *value,
                                  GParamSpec  *pspec)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (object);

  switch (prop_id)
    {
    case PROP_HAS_OPACITY_CONTROL:
      g_value_set_boolean (value, gtk_color_selection_get_has_opacity_control (colorsel));
      break;
    case PROP_HAS_PALETTE:
      g_value_set_boolean (value, gtk_color_selection_get_has_palette (colorsel));
      break;
    case PROP_CURRENT_COLOR:
      {
        GdkColor color;
        GdkRGBA rgba;

        gtk_color_selection_get_current_rgba (colorsel, &rgba);

        color.red = UNSCALE (rgba.red);
        color.green = UNSCALE (rgba.green);
        color.blue = UNSCALE (rgba.blue);

        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_CURRENT_ALPHA:
      g_value_set_uint (value, gtk_color_selection_get_current_alpha (colorsel));
      break;
    case PROP_CURRENT_RGBA:
      {
        GdkRGBA rgba;

        gtk_color_selection_get_current_rgba (colorsel, &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/* GtkWidget methods */

static void
gtk_color_selection_destroy (GtkWidget *widget)
{
  GtkColorSelection *cselection = GTK_COLOR_SELECTION (widget);
  GtkColorSelectionPrivate *priv = cselection->private_data;

  if (priv->dropper_grab_widget)
    {
      gtk_widget_destroy (priv->dropper_grab_widget);
      priv->dropper_grab_widget = NULL;
    }

  GTK_WIDGET_CLASS (gtk_color_selection_parent_class)->destroy (widget);
}

static void
gtk_color_selection_realize (GtkWidget *widget)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (widget);
  GtkColorSelectionPrivate *priv = colorsel->private_data;
  GtkSettings *settings = gtk_widget_get_settings (widget);

  priv->settings_connection =  g_signal_connect (settings,
                                                 "notify::gtk-color-palette",
                                                 G_CALLBACK (palette_change_notify_instance),
                                                 widget);
  update_palette (colorsel);

  GTK_WIDGET_CLASS (gtk_color_selection_parent_class)->realize (widget);
}

static void
gtk_color_selection_unrealize (GtkWidget *widget)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (widget);
  GtkColorSelectionPrivate *priv = colorsel->private_data;
  GtkSettings *settings = gtk_widget_get_settings (widget);

  g_signal_handler_disconnect (settings, priv->settings_connection);

  GTK_WIDGET_CLASS (gtk_color_selection_parent_class)->unrealize (widget);
}

/* We override show-all since we have internal widgets that
 * shouldn’t be shown when you call show_all(), like the
 * palette and opacity sliders.
 */
static void
gtk_color_selection_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

static gboolean
gtk_color_selection_grab_broken (GtkWidget          *widget,
                                 GdkEventGrabBroken *event)
{
  shutdown_eyedropper (widget);

  return TRUE;
}

/*
 *
 * The Sample Color
 *
 */

static void color_sample_draw_sample (GtkColorSelection *colorsel,
                                      int                which,
                                      cairo_t *          cr);
static void color_sample_update_samples (GtkColorSelection *colorsel);

static void
set_color_internal (GtkColorSelection *colorsel,
                    gdouble           *color)
{
  GtkColorSelectionPrivate *priv;
  gint i;

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_RED] = color[0];
  priv->color[COLORSEL_GREEN] = color[1];
  priv->color[COLORSEL_BLUE] = color[2];
  priv->color[COLORSEL_OPACITY] = color[3];
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                  priv->color[COLORSEL_GREEN],
                  priv->color[COLORSEL_BLUE],
                  &priv->color[COLORSEL_HUE],
                  &priv->color[COLORSEL_SATURATION],
                  &priv->color[COLORSEL_VALUE]);
  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
        priv->old_color[i] = priv->color[i];
    }
  priv->default_set = TRUE;
  priv->default_alpha_set = TRUE;
  update_color (colorsel);
}

static void
set_color_icon (GdkDragContext *context,
                gdouble        *colors)
{
  GdkPixbuf *pixbuf;
  guint32 pixel;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE,
                           8, 48, 32);

  pixel = (((UNSCALE (colors[COLORSEL_RED])   & 0xff00) << 16) |
           ((UNSCALE (colors[COLORSEL_GREEN]) & 0xff00) << 8) |
           ((UNSCALE (colors[COLORSEL_BLUE])  & 0xff00)));

  gdk_pixbuf_fill (pixbuf, pixel);

  gtk_drag_set_icon_pixbuf (context, pixbuf, -2, -2);
  g_object_unref (pixbuf);
}

static void
color_sample_drag_begin (GtkWidget      *widget,
                         GdkDragContext *context,
                         gpointer        data)
{
  GtkColorSelection *colorsel = data;
  GtkColorSelectionPrivate *priv;
  gdouble *colsrc;

  priv = colorsel->private_data;

  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;

  set_color_icon (context, colsrc);
}

static void
color_sample_drag_end (GtkWidget      *widget,
                       GdkDragContext *context,
                       gpointer        data)
{
  g_object_set_data (G_OBJECT (widget), I_("gtk-color-selection-drag-window"), NULL);
}

static void
color_sample_drop_handle (GtkWidget        *widget,
                          GdkDragContext   *context,
                          gint              x,
                          gint              y,
                          GtkSelectionData *selection_data,
                          guint             info,
                          guint             time,
                          gpointer          data)
{
  GtkColorSelection *colorsel = data;
  GtkColorSelectionPrivate *priv;
  gint length;
  guint16 *vals;
  gdouble color[4];
  priv = colorsel->private_data;

  /* This is currently a guint16 array of the format:
   * R
   * G
   * B
   * opacity
   */

  length = gtk_selection_data_get_length (selection_data);

  if (length < 0)
    return;

  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (length != 8)
    {
      g_warning ("Received invalid color data");
      return;
    }

  vals = (guint16 *) gtk_selection_data_get_data (selection_data);

  if (widget == priv->cur_sample)
    {
      color[0] = (gdouble)vals[0] / 0xffff;
      color[1] = (gdouble)vals[1] / 0xffff;
      color[2] = (gdouble)vals[2] / 0xffff;
      color[3] = (gdouble)vals[3] / 0xffff;

      set_color_internal (colorsel, color);
    }
}

static void
color_sample_drag_handle (GtkWidget        *widget,
                          GdkDragContext   *context,
                          GtkSelectionData *selection_data,
                          guint             info,
                          guint             time,
                          gpointer          data)
{
  GtkColorSelection *colorsel = data;
  GtkColorSelectionPrivate *priv;
  guint16 vals[4];
  gdouble *colsrc;

  priv = colorsel->private_data;

  if (widget == priv->old_sample)
    colsrc = priv->old_color;
  else
    colsrc = priv->color;

  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = priv->has_opacity ? colsrc[COLORSEL_OPACITY] * 0xffff : 0xffff;

  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("application/x-color"),
                          16, (guchar *)vals, 8);
}

/* which = 0 means draw old sample, which = 1 means draw new */
static void
color_sample_draw_sample (GtkColorSelection *colorsel,
                          int                which,
                          cairo_t           *cr)
{
  GtkWidget *da;
  gint x, y, goff;
  GtkColorSelectionPrivate *priv;
  int width, height;

  g_return_if_fail (colorsel != NULL);
  priv = colorsel->private_data;

  g_return_if_fail (priv->sample_area != NULL);
  if (!gtk_widget_is_drawable (priv->sample_area))
    return;

  if (which == 0)
    {
      da = priv->old_sample;
      goff = 0;
    }
  else
    {
      GtkAllocation old_sample_allocation;

      da = priv->cur_sample;
      gtk_widget_get_allocation (priv->old_sample, &old_sample_allocation);
      goff =  old_sample_allocation.width % 32;
    }

  /* Below needs tweaking for non-power-of-two */
  width = gtk_widget_get_allocated_width (da);
  height = gtk_widget_get_allocated_height (da);

  if (priv->has_opacity)
    {
      /* Draw checks in background */

      cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
      cairo_rectangle (cr, 0, 0, width, height);
      cairo_fill (cr);

      cairo_set_source_rgb (cr, 0.75, 0.75, 0.75);
      for (x = goff & -CHECK_SIZE; x < goff + width; x += CHECK_SIZE)
        for (y = 0; y < height; y += CHECK_SIZE)
          if ((x / CHECK_SIZE + y / CHECK_SIZE) % 2 == 0)
            cairo_rectangle (cr, x - goff, y, CHECK_SIZE, CHECK_SIZE);
      cairo_fill (cr);
    }

  if (which == 0)
    {
      cairo_set_source_rgba (cr,
                             priv->old_color[COLORSEL_RED],
                             priv->old_color[COLORSEL_GREEN],
                             priv->old_color[COLORSEL_BLUE],
                             priv->has_opacity ?
                                priv->old_color[COLORSEL_OPACITY] : 1.0);
    }
  else
    {
      cairo_set_source_rgba (cr,
                             priv->color[COLORSEL_RED],
                             priv->color[COLORSEL_GREEN],
                             priv->color[COLORSEL_BLUE],
                             priv->has_opacity ?
                               priv->color[COLORSEL_OPACITY] : 1.0);
    }

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);
}


static void
color_sample_update_samples (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv = colorsel->private_data;
  gtk_widget_queue_draw (priv->old_sample);
  gtk_widget_queue_draw (priv->cur_sample);
}

static gboolean
color_old_sample_draw (GtkWidget         *da,
                       cairo_t           *cr,
                       GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 0, cr);
  return FALSE;
}


static gboolean
color_cur_sample_draw (GtkWidget         *da,
                       cairo_t           *cr,
                       GtkColorSelection *colorsel)
{
  color_sample_draw_sample (colorsel, 1, cr);
  return FALSE;
}

static void
color_sample_setup_dnd (GtkColorSelection *colorsel, GtkWidget *sample)
{
  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };
  GtkColorSelectionPrivate *priv;
  priv = colorsel->private_data;

  gtk_drag_source_set (sample,
                       GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                       targets, 1,
                       GDK_ACTION_COPY | GDK_ACTION_MOVE);

  g_signal_connect (sample, "drag-begin",
                    G_CALLBACK (color_sample_drag_begin),
                    colorsel);
  if (sample == priv->cur_sample)
    {

      gtk_drag_dest_set (sample,
                         GTK_DEST_DEFAULT_HIGHLIGHT |
                         GTK_DEST_DEFAULT_MOTION |
                         GTK_DEST_DEFAULT_DROP,
                         targets, 1,
                         GDK_ACTION_COPY);

      g_signal_connect (sample, "drag-end",
                        G_CALLBACK (color_sample_drag_end),
                        colorsel);
    }

  g_signal_connect (sample, "drag-data-get",
                    G_CALLBACK (color_sample_drag_handle),
                    colorsel);
  g_signal_connect (sample, "drag-data-received",
                    G_CALLBACK (color_sample_drop_handle),
                    colorsel);

}

static void
update_tooltips (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  priv = colorsel->private_data;

  if (priv->has_palette == TRUE)
    {
      gtk_widget_set_tooltip_text (priv->old_sample,
        _("The previously-selected color, for comparison to the color "
          "you’re selecting now. You can drag this color to a palette "
          "entry, or select this color as current by dragging it to the "
          "other color swatch alongside."));

      gtk_widget_set_tooltip_text (priv->cur_sample,
        _("The color you’ve chosen. You can drag this color to a palette "
          "entry to save it for use in the future."));
    }
  else
    {
      gtk_widget_set_tooltip_text (priv->old_sample,
        _("The previously-selected color, for comparison to the color "
          "you’re selecting now."));

      gtk_widget_set_tooltip_text (priv->cur_sample,
        _("The color you’ve chosen."));
    }
}

static void
color_sample_new (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  priv = colorsel->private_data;

  priv->sample_area = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  priv->old_sample = gtk_drawing_area_new ();
  priv->cur_sample = gtk_drawing_area_new ();

  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->old_sample,
                      TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->sample_area), priv->cur_sample,
                      TRUE, TRUE, 0);

  g_signal_connect (priv->old_sample, "draw",
                    G_CALLBACK (color_old_sample_draw),
                    colorsel);
  g_signal_connect (priv->cur_sample, "draw",
                    G_CALLBACK (color_cur_sample_draw),
                    colorsel);

  color_sample_setup_dnd (colorsel, priv->old_sample);
  color_sample_setup_dnd (colorsel, priv->cur_sample);

  update_tooltips (colorsel);

  gtk_widget_show_all (priv->sample_area);
}


/* The palette area code */

static void
palette_get_color (GtkWidget *drawing_area, gdouble *color)
{
  gdouble *color_val;

  g_return_if_fail (color != NULL);

  color_val = g_object_get_data (G_OBJECT (drawing_area), "color_val");
  if (color_val == NULL)
    {
      /* Default to white for no good reason */
      color[0] = 1.0;
      color[1] = 1.0;
      color[2] = 1.0;
      color[3] = 1.0;
      return;
    }

  color[0] = color_val[0];
  color[1] = color_val[1];
  color[2] = color_val[2];
  color[3] = 1.0;
}

static gboolean
palette_draw (GtkWidget *drawing_area,
               cairo_t   *cr,
               gpointer   data)
{
  GtkStyleContext *context;
  gint focus_width;
  GdkRGBA color;

  context = gtk_widget_get_style_context (drawing_area);
  gtk_style_context_get_background_color (context, 0, &color);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_paint (cr);

  if (gtk_widget_has_visible_focus (drawing_area))
    {
      set_focus_line_attributes (drawing_area, cr, &focus_width);

      cairo_rectangle (cr,
                       focus_width / 2., focus_width / 2.,
                       gtk_widget_get_allocated_width (drawing_area) - focus_width,
                       gtk_widget_get_allocated_height (drawing_area) - focus_width);
      cairo_stroke (cr);
    }

  return FALSE;
}

static void
set_focus_line_attributes (GtkWidget *drawing_area,
                           cairo_t   *cr,
                           gint      *focus_width)
{
  gdouble color[4];
  gint8 *dash_list;

  gtk_widget_style_get (drawing_area,
                        "focus-line-width", focus_width,
                        "focus-line-pattern", (gchar *)&dash_list,
                        NULL);

  palette_get_color (drawing_area, color);

  if (INTENSITY (color[0], color[1], color[2]) > 0.5)
    cairo_set_source_rgb (cr, 0., 0., 0.);
  else
    cairo_set_source_rgb (cr, 1., 1., 1.);

  cairo_set_line_width (cr, *focus_width);

  if (dash_list[0])
    {
      gint n_dashes = strlen ((gchar *)dash_list);
      gdouble *dashes = g_new (gdouble, n_dashes);
      gdouble total_length = 0;
      gdouble dash_offset;
      gint i;

      for (i = 0; i < n_dashes; i++)
        {
          dashes[i] = dash_list[i];
          total_length += dash_list[i];
        }

      /* The dash offset here aligns the pattern to integer pixels
       * by starting the dash at the right side of the left border
       * Negative dash offsets in cairo don't work
       * (https://bugs.freedesktop.org/show_bug.cgi?id=2729)
       */
      dash_offset = - *focus_width / 2.;
      while (dash_offset < 0)
        dash_offset += total_length;

      cairo_set_dash (cr, dashes, n_dashes, dash_offset);
      g_free (dashes);
    }

  g_free (dash_list);
}

static void
palette_drag_begin (GtkWidget      *widget,
                    GdkDragContext *context,
                    gpointer        data)
{
  gdouble colors[4];

  palette_get_color (widget, colors);
  set_color_icon (context, colors);
}

static void
palette_drag_handle (GtkWidget        *widget,
                     GdkDragContext   *context,
                     GtkSelectionData *selection_data,
                     guint             info,
                     guint             time,
                     gpointer          data)
{
  guint16 vals[4];
  gdouble colsrc[4];

  palette_get_color (widget, colsrc);

  vals[0] = colsrc[COLORSEL_RED] * 0xffff;
  vals[1] = colsrc[COLORSEL_GREEN] * 0xffff;
  vals[2] = colsrc[COLORSEL_BLUE] * 0xffff;
  vals[3] = 0xffff;

  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("application/x-color"),
                          16, (guchar *)vals, 8);
}

static void
palette_drag_end (GtkWidget      *widget,
                  GdkDragContext *context,
                  gpointer        data)
{
  g_object_set_data (G_OBJECT (widget), I_("gtk-color-selection-drag-window"), NULL);
}

static GdkColor *
get_current_colors (GtkColorSelection *colorsel)
{
  GtkSettings *settings;
  GdkColor *colors = NULL;
  gint n_colors = 0;
  gchar *palette;

  settings = gtk_widget_get_settings (GTK_WIDGET (colorsel));
  g_object_get (settings, "gtk-color-palette", &palette, NULL);

  if (!gtk_color_selection_palette_from_string (palette, &colors, &n_colors))
    {
      gtk_color_selection_palette_from_string (DEFAULT_COLOR_PALETTE,
                                               &colors,
                                               &n_colors);
    }
  else
    {
      /* If there are less colors provided than the number of slots in the
       * color selection, we fill in the rest from the defaults.
       */
      if (n_colors < (GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT))
        {
          GdkColor *tmp_colors = colors;
          gint tmp_n_colors = n_colors;

          gtk_color_selection_palette_from_string (DEFAULT_COLOR_PALETTE,
                                                   &colors,
                                                   &n_colors);
          memcpy (colors, tmp_colors, sizeof (GdkColor) * tmp_n_colors);

          g_free (tmp_colors);
        }
    }

  /* make sure that we fill every slot */
  g_assert (n_colors == GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
  g_free (palette);

  return colors;
}

/* Changes the model color */
static void
palette_change_color (GtkWidget         *drawing_area,
                      GtkColorSelection *colorsel,
                      gdouble           *color)
{
  gint x, y;
  GtkColorSelectionPrivate *priv;
  GdkColor gdk_color;
  GdkColor *current_colors;
  GdkScreen *screen;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (GTK_IS_DRAWING_AREA (drawing_area));

  priv = colorsel->private_data;

  gdk_color.red = UNSCALE (color[0]);
  gdk_color.green = UNSCALE (color[1]);
  gdk_color.blue = UNSCALE (color[2]);
  gdk_color.pixel = 0;

  x = 0;
  y = 0;                        /* Quiet GCC */
  while (x < GTK_CUSTOM_PALETTE_WIDTH)
    {
      y = 0;
      while (y < GTK_CUSTOM_PALETTE_HEIGHT)
        {
          if (priv->custom_palette[x][y] == drawing_area)
            goto out;

          ++y;
        }

      ++x;
    }

 out:

  g_assert (x < GTK_CUSTOM_PALETTE_WIDTH || y < GTK_CUSTOM_PALETTE_HEIGHT);

  current_colors = get_current_colors (colorsel);
  current_colors[y * GTK_CUSTOM_PALETTE_WIDTH + x] = gdk_color;

  screen = gtk_widget_get_screen (GTK_WIDGET (colorsel));
  if (change_palette_hook != default_change_palette_func)
    (* change_palette_hook) (screen, current_colors,
                             GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
  else if (noscreen_change_palette_hook != default_noscreen_change_palette_func)
    {
      if (screen != gdk_screen_get_default ())
        g_warning ("gtk_color_selection_set_change_palette_hook used by "
                   "widget is not on the default screen.");
      (* noscreen_change_palette_hook) (current_colors,
                                        GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);
    }
  else
    (* change_palette_hook) (screen, current_colors,
                             GTK_CUSTOM_PALETTE_WIDTH * GTK_CUSTOM_PALETTE_HEIGHT);

  g_free (current_colors);
}

/* Changes the view color */
static void
palette_set_color (GtkWidget         *drawing_area,
                   GtkColorSelection *colorsel,
                   gdouble           *color)
{
  gdouble *new_color = g_new (double, 4);
  GdkRGBA rgba;

  rgba.red = color[0];
  rgba.green = color[1];
  rgba.blue = color[2];
  rgba.alpha = 1;

  gtk_widget_override_background_color (drawing_area, GTK_STATE_FLAG_NORMAL, &rgba);

  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drawing_area), "color_set")) == 0)
    {
      static const GtkTargetEntry targets[] = {
        { "application/x-color", 0 }
      };
      gtk_drag_source_set (drawing_area,
                           GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
                           targets, 1,
                           GDK_ACTION_COPY | GDK_ACTION_MOVE);

      g_signal_connect (drawing_area, "drag-begin",
                        G_CALLBACK (palette_drag_begin),
                        colorsel);
      g_signal_connect (drawing_area, "drag-data-get",
                        G_CALLBACK (palette_drag_handle),
                        colorsel);

      g_object_set_data (G_OBJECT (drawing_area), I_("color_set"),
                         GINT_TO_POINTER (1));
    }

  new_color[0] = color[0];
  new_color[1] = color[1];
  new_color[2] = color[2];
  new_color[3] = 1.0;

  g_object_set_data_full (G_OBJECT (drawing_area),
                          I_("color_val"), new_color, (GDestroyNotify)g_free);
}

static void
save_color_selected (GtkWidget *menuitem,
                     gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkWidget *drawing_area;
  GtkColorSelectionPrivate *priv;

  drawing_area = GTK_WIDGET (data);

  colorsel = GTK_COLOR_SELECTION (g_object_get_data (G_OBJECT (drawing_area),
                                                     "gtk-color-sel"));

  priv = colorsel->private_data;

  palette_change_color (drawing_area, colorsel, priv->color);
}

static void
do_popup (GtkColorSelection *colorsel,
          GtkWidget         *drawing_area,
          const GdkEvent    *trigger_event)
{
  GtkWidget *menu;
  GtkWidget *mi;

  g_object_set_data (G_OBJECT (drawing_area),
                     I_("gtk-color-sel"),
                     colorsel);

  menu = gtk_menu_new ();
  g_signal_connect (menu, "hide", G_CALLBACK (gtk_widget_destroy), NULL);

  mi = gtk_menu_item_new_with_mnemonic (_("_Save color here"));

  g_signal_connect (mi, "activate",
                    G_CALLBACK (save_color_selected),
                    drawing_area);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);

  gtk_widget_show_all (mi);

  if (trigger_event && gdk_event_triggers_context_menu (trigger_event))
    gtk_menu_popup_at_pointer (GTK_MENU (menu), trigger_event);
  else
    gtk_menu_popup_at_widget (GTK_MENU (menu),
                              drawing_area,
                              GDK_GRAVITY_CENTER,
                              GDK_GRAVITY_NORTH_WEST,
                              trigger_event);
}


static gboolean
palette_enter (GtkWidget        *drawing_area,
               GdkEventCrossing *event,
               gpointer        data)
{
  g_object_set_data (G_OBJECT (drawing_area),
                     I_("gtk-colorsel-have-pointer"),
                     GUINT_TO_POINTER (TRUE));

  return FALSE;
}

static gboolean
palette_leave (GtkWidget        *drawing_area,
               GdkEventCrossing *event,
               gpointer        data)
{
  g_object_set_data (G_OBJECT (drawing_area),
                     I_("gtk-colorsel-have-pointer"),
                     NULL);

  return FALSE;
}

static gboolean
palette_press (GtkWidget      *drawing_area,
               GdkEventButton *event,
               gpointer        data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);

  gtk_widget_grab_focus (drawing_area);

  if (gdk_event_triggers_context_menu ((GdkEvent *) event))
    {
      do_popup (colorsel, drawing_area, (GdkEvent *) event);
      return TRUE;
    }

  return FALSE;
}

static gboolean
palette_release (GtkWidget      *drawing_area,
                 GdkEventButton *event,
                 gpointer        data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);

  gtk_widget_grab_focus (drawing_area);

  if (event->button == GDK_BUTTON_PRIMARY &&
      g_object_get_data (G_OBJECT (drawing_area),
                         "gtk-colorsel-have-pointer") != NULL)
    {
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drawing_area), "color_set")) != 0)
        {
          gdouble color[4];
          palette_get_color (drawing_area, color);
          set_color_internal (colorsel, color);
        }
    }

  return FALSE;
}

static void
palette_drop_handle (GtkWidget        *widget,
                     GdkDragContext   *context,
                     gint              x,
                     gint              y,
                     GtkSelectionData *selection_data,
                     guint             info,
                     guint             time,
                     gpointer          data)
{
  GtkColorSelection *colorsel = GTK_COLOR_SELECTION (data);
  gint length;
  guint16 *vals;
  gdouble color[4];

  length = gtk_selection_data_get_length (selection_data);

  if (length < 0)
    return;

  /* We accept drops with the wrong format, since the KDE color
   * chooser incorrectly drops application/x-color with format 8.
   */
  if (length != 8)
    {
      g_warning ("Received invalid color data");
      return;
    }

  vals = (guint16 *) gtk_selection_data_get_data (selection_data);

  color[0] = (gdouble)vals[0] / 0xffff;
  color[1] = (gdouble)vals[1] / 0xffff;
  color[2] = (gdouble)vals[2] / 0xffff;
  color[3] = (gdouble)vals[3] / 0xffff;
  palette_change_color (widget, colorsel, color);
  set_color_internal (colorsel, color);
}

static gint
palette_activate (GtkWidget   *widget,
                  GdkEventKey *event,
                  gpointer     data)
{
  /* should have a drawing area subclass with an activate signal */
  if ((event->keyval == GDK_KEY_space) ||
      (event->keyval == GDK_KEY_Return) ||
      (event->keyval == GDK_KEY_ISO_Enter) ||
      (event->keyval == GDK_KEY_KP_Enter) ||
      (event->keyval == GDK_KEY_KP_Space))
    {
      if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "color_set")) != 0)
        {
          gdouble color[4];
          palette_get_color (widget, color);
          set_color_internal (GTK_COLOR_SELECTION (data), color);
        }
      return TRUE;
    }

  return FALSE;
}

static gboolean
palette_popup (GtkWidget *widget,
               gpointer   data)
{
  do_popup (data, widget, NULL);
  return TRUE;
}


static GtkWidget*
palette_new (GtkColorSelection *colorsel)
{
  GtkWidget *retval;

  static const GtkTargetEntry targets[] = {
    { "application/x-color", 0 }
  };

  retval = gtk_drawing_area_new ();

  gtk_widget_set_can_focus (retval, TRUE);

  g_object_set_data (G_OBJECT (retval), I_("color_set"), GINT_TO_POINTER (0));
  gtk_widget_set_events (retval, GDK_BUTTON_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_ENTER_NOTIFY_MASK
                         | GDK_LEAVE_NOTIFY_MASK);

  g_signal_connect (retval, "draw",
                    G_CALLBACK (palette_draw), colorsel);
  g_signal_connect (retval, "button-press-event",
                    G_CALLBACK (palette_press), colorsel);
  g_signal_connect (retval, "button-release-event",
                    G_CALLBACK (palette_release), colorsel);
  g_signal_connect (retval, "enter-notify-event",
                    G_CALLBACK (palette_enter), colorsel);
  g_signal_connect (retval, "leave-notify-event",
                    G_CALLBACK (palette_leave), colorsel);
  g_signal_connect (retval, "key-press-event",
                    G_CALLBACK (palette_activate), colorsel);
  g_signal_connect (retval, "popup-menu",
                    G_CALLBACK (palette_popup), colorsel);

  gtk_drag_dest_set (retval,
                     GTK_DEST_DEFAULT_HIGHLIGHT |
                     GTK_DEST_DEFAULT_MOTION |
                     GTK_DEST_DEFAULT_DROP,
                     targets, 1,
                     GDK_ACTION_COPY);

  g_signal_connect (retval, "drag-end",
                    G_CALLBACK (palette_drag_end), NULL);
  g_signal_connect (retval, "drag-data-received",
                    G_CALLBACK (palette_drop_handle), colorsel);

  gtk_widget_set_tooltip_text (retval,
    _("Click this palette entry to make it the current color. "
      "To change this entry, drag a color swatch here or right-click "
      "it and select “Save color here.”"));
  return retval;
}


/* The actual GtkColorSelection widget */

static GdkCursor *
make_picker_cursor (GdkScreen *screen)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_from_name (gdk_screen_get_display (screen),
                                     "color-picker");

  if (!cursor)
    {
      GdkPixbuf *pixbuf;

      pixbuf = gdk_pixbuf_new_from_data (dropper_bits,
                                         GDK_COLORSPACE_RGB, TRUE, 8,
                                         DROPPER_WIDTH, DROPPER_HEIGHT,
                                         DROPPER_STRIDE,
                                         NULL, NULL);

      cursor = gdk_cursor_new_from_pixbuf (gdk_screen_get_display (screen),
                                           pixbuf,
                                           DROPPER_X_HOT, DROPPER_Y_HOT);

      g_object_unref (pixbuf);
    }

  return cursor;
}

static void
grab_color_at_pointer (GdkScreen *screen,
                       GdkDevice *device,
                       gint       x_root,
                       gint       y_root,
                       gpointer   data)
{
  GdkPixbuf *pixbuf;
  guchar *pixels;
  GtkColorSelection *colorsel = data;
  GtkColorSelectionPrivate *priv;
  GdkColor color;
  GdkWindow *root_window = gdk_screen_get_root_window (screen);

  priv = colorsel->private_data;

  pixbuf = gdk_pixbuf_get_from_window (root_window,
                                       x_root, y_root,
                                       1, 1);
  if (!pixbuf)
    {
      gint x, y;
      GdkWindow *window = gdk_device_get_window_at_position (device, &x, &y);
      if (!window)
        return;
      pixbuf = gdk_pixbuf_get_from_window (window,
                                           x, y,
                                           1, 1);
      if (!pixbuf)
        return;
    }
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  color.red = pixels[0] * 0x101;
  color.green = pixels[1] * 0x101;
  color.blue = pixels[2] * 0x101;
  g_object_unref (pixbuf);

  priv->color[COLORSEL_RED] = SCALE (color.red);
  priv->color[COLORSEL_GREEN] = SCALE (color.green);
  priv->color[COLORSEL_BLUE] = SCALE (color.blue);

  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                  priv->color[COLORSEL_GREEN],
                  priv->color[COLORSEL_BLUE],
                  &priv->color[COLORSEL_HUE],
                  &priv->color[COLORSEL_SATURATION],
                  &priv->color[COLORSEL_VALUE]);

  update_color (colorsel);
}

static void
shutdown_eyedropper (GtkWidget *widget)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;

  colorsel = GTK_COLOR_SELECTION (widget);
  priv = colorsel->private_data;

  if (priv->has_grab)
    {
      gdk_device_ungrab (priv->keyboard_device, priv->grab_time);
      gdk_device_ungrab (priv->pointer_device, priv->grab_time);
      gtk_device_grab_remove (priv->dropper_grab_widget, priv->pointer_device);

      priv->has_grab = FALSE;
      priv->keyboard_device = NULL;
      priv->pointer_device = NULL;
    }
}

static void
mouse_motion (GtkWidget      *invisible,
              GdkEventMotion *event,
              gpointer        data)
{
  grab_color_at_pointer (gdk_event_get_screen ((GdkEvent *) event),
                         gdk_event_get_device ((GdkEvent *) event),
                         event->x_root, event->y_root, data);
}

static gboolean
mouse_release (GtkWidget      *invisible,
               GdkEventButton *event,
               gpointer        data)
{
  /* GtkColorSelection *colorsel = data; */

  if (event->button != GDK_BUTTON_PRIMARY)
    return FALSE;

  grab_color_at_pointer (gdk_event_get_screen ((GdkEvent *) event),
                         gdk_event_get_device ((GdkEvent *) event),
                         event->x_root, event->y_root, data);

  shutdown_eyedropper (GTK_WIDGET (data));

  g_signal_handlers_disconnect_by_func (invisible,
                                        mouse_motion,
                                        data);
  g_signal_handlers_disconnect_by_func (invisible,
                                        mouse_release,
                                        data);

  return TRUE;
}

/* Helper Functions */

static gboolean
key_press (GtkWidget   *invisible,
           GdkEventKey *event,
           gpointer     data)
{
  GdkScreen *screen = gdk_event_get_screen ((GdkEvent *) event);
  GdkDevice *device, *pointer_device;
  guint state = event->state & gtk_accelerator_get_default_mod_mask ();
  gint x, y;
  gint dx, dy;

  device = gdk_event_get_device ((GdkEvent * ) event);
  pointer_device = gdk_device_get_associated_device (device);
  gdk_device_get_position (pointer_device, NULL, &x, &y);

  dx = 0;
  dy = 0;

  switch (event->keyval)
    {
    case GDK_KEY_space:
    case GDK_KEY_Return:
    case GDK_KEY_ISO_Enter:
    case GDK_KEY_KP_Enter:
    case GDK_KEY_KP_Space:
      grab_color_at_pointer (screen, pointer_device, x, y, data);
      /* fall through */

    case GDK_KEY_Escape:
      shutdown_eyedropper (data);

      g_signal_handlers_disconnect_by_func (invisible,
                                            mouse_press,
                                            data);
      g_signal_handlers_disconnect_by_func (invisible,
                                            key_press,
                                            data);

      return TRUE;

    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      dy = state == GDK_MOD1_MASK ? -BIG_STEP : -1;
      break;

    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      dy = state == GDK_MOD1_MASK ? BIG_STEP : 1;
      break;

    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
      dx = state == GDK_MOD1_MASK ? -BIG_STEP : -1;
      break;

    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
      dx = state == GDK_MOD1_MASK ? BIG_STEP : 1;
      break;

    default:
      return FALSE;
    }

  gdk_device_warp (pointer_device, screen, x + dx, y + dy);

  return TRUE;

}

static gboolean
mouse_press (GtkWidget      *invisible,
             GdkEventButton *event,
             gpointer        data)
{
  if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY)
    {
      g_signal_connect (invisible, "motion-notify-event",
                        G_CALLBACK (mouse_motion), data);
      g_signal_connect (invisible, "button-release-event",
                        G_CALLBACK (mouse_release), data);
      g_signal_handlers_disconnect_by_func (invisible,
                                            mouse_press,
                                            data);
      g_signal_handlers_disconnect_by_func (invisible,
                                            key_press,
                                            data);
      return TRUE;
    }

  return FALSE;
}

/* when the button is clicked */
static void
get_screen_color (GtkWidget *button)
{
  GtkColorSelection *colorsel = g_object_get_data (G_OBJECT (button), "COLORSEL");
  GtkColorSelectionPrivate *priv = colorsel->private_data;
  GdkScreen *screen = gtk_widget_get_screen (GTK_WIDGET (button));
  GdkDevice *device, *keyb_device, *pointer_device;
  GdkCursor *picker_cursor;
  GdkGrabStatus grab_status;
  GdkWindow *window;
  GtkWidget *grab_widget, *toplevel;

  guint32 time = gtk_get_current_event_time ();

  device = gtk_get_current_event_device ();

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      keyb_device = device;
      pointer_device = gdk_device_get_associated_device (device);
    }
  else
    {
      pointer_device = device;
      keyb_device = gdk_device_get_associated_device (device);
    }

  if (priv->dropper_grab_widget == NULL)
    {
      grab_widget = gtk_window_new (GTK_WINDOW_POPUP);
      gtk_window_set_screen (GTK_WINDOW (grab_widget), screen);
      gtk_window_resize (GTK_WINDOW (grab_widget), 1, 1);
      gtk_window_move (GTK_WINDOW (grab_widget), -100, -100);
      gtk_widget_show (grab_widget);

      gtk_widget_add_events (grab_widget,
                             GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (colorsel));

      if (GTK_IS_WINDOW (toplevel))
        {
          if (gtk_window_has_group (GTK_WINDOW (toplevel)))
            gtk_window_group_add_window (gtk_window_get_group (GTK_WINDOW (toplevel)),
                                         GTK_WINDOW (grab_widget));
        }

      priv->dropper_grab_widget = grab_widget;
    }

  window = gtk_widget_get_window (priv->dropper_grab_widget);

  if (gdk_device_grab (keyb_device,
                       window,
                       GDK_OWNERSHIP_APPLICATION, FALSE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL, time) != GDK_GRAB_SUCCESS)
    return;

  picker_cursor = make_picker_cursor (screen);
  grab_status = gdk_device_grab (pointer_device,
                                 window,
                                 GDK_OWNERSHIP_APPLICATION,
                                 FALSE,
                                 GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK,
                                 picker_cursor,
                                 time);
  g_object_unref (picker_cursor);

  if (grab_status != GDK_GRAB_SUCCESS)
    {
      gdk_device_ungrab (keyb_device, time);
      return;
    }

  gtk_device_grab_add (priv->dropper_grab_widget,
                       pointer_device,
                       TRUE);

  priv->grab_time = time;
  priv->has_grab = TRUE;
  priv->keyboard_device = keyb_device;
  priv->pointer_device = pointer_device;

  g_signal_connect (priv->dropper_grab_widget, "button-press-event",
                    G_CALLBACK (mouse_press), colorsel);
  g_signal_connect (priv->dropper_grab_widget, "key-press-event",
                    G_CALLBACK (key_press), colorsel);
}

static void
hex_changed (GtkWidget *hex_entry,
             gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;
  GdkRGBA color;
  gchar *text;

  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;

  if (priv->changing)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (priv->hex_entry), 0, -1);
  if (gdk_rgba_parse (&color, text))
    {
      priv->color[COLORSEL_RED]   = color.red;
      priv->color[COLORSEL_GREEN] = color.green;
      priv->color[COLORSEL_BLUE]  = color.blue;
      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                      priv->color[COLORSEL_GREEN],
                      priv->color[COLORSEL_BLUE],
                      &priv->color[COLORSEL_HUE],
                      &priv->color[COLORSEL_SATURATION],
                      &priv->color[COLORSEL_VALUE]);
      update_color (colorsel);
    }
  g_free (text);
}

static gboolean
hex_focus_out (GtkWidget     *hex_entry,
               GdkEventFocus *event,
               gpointer       data)
{
  hex_changed (hex_entry, data);

  return FALSE;
}

static void
hsv_changed (GtkWidget *hsv,
             gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;

  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;

  if (priv->changing)
    return;

  gtk_hsv_get_color (GTK_HSV (hsv),
                     &priv->color[COLORSEL_HUE],
                     &priv->color[COLORSEL_SATURATION],
                     &priv->color[COLORSEL_VALUE]);
  gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
                  priv->color[COLORSEL_SATURATION],
                  priv->color[COLORSEL_VALUE],
                  &priv->color[COLORSEL_RED],
                  &priv->color[COLORSEL_GREEN],
                  &priv->color[COLORSEL_BLUE]);
  update_color (colorsel);
}

static void
adjustment_changed (GtkAdjustment *adjustment,
                    gpointer       data)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;

  colorsel = GTK_COLOR_SELECTION (g_object_get_data (G_OBJECT (adjustment), "COLORSEL"));
  priv = colorsel->private_data;

  if (priv->changing)
    return;

  switch (GPOINTER_TO_INT (data))
    {
    case COLORSEL_SATURATION:
    case COLORSEL_VALUE:
      priv->color[GPOINTER_TO_INT (data)] = gtk_adjustment_get_value (adjustment) / 100;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
                      priv->color[COLORSEL_SATURATION],
                      priv->color[COLORSEL_VALUE],
                      &priv->color[COLORSEL_RED],
                      &priv->color[COLORSEL_GREEN],
                      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_HUE:
      priv->color[GPOINTER_TO_INT (data)] = gtk_adjustment_get_value (adjustment) / 360;
      gtk_hsv_to_rgb (priv->color[COLORSEL_HUE],
                      priv->color[COLORSEL_SATURATION],
                      priv->color[COLORSEL_VALUE],
                      &priv->color[COLORSEL_RED],
                      &priv->color[COLORSEL_GREEN],
                      &priv->color[COLORSEL_BLUE]);
      break;
    case COLORSEL_RED:
    case COLORSEL_GREEN:
    case COLORSEL_BLUE:
      priv->color[GPOINTER_TO_INT (data)] = gtk_adjustment_get_value (adjustment) / 255;

      gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                      priv->color[COLORSEL_GREEN],
                      priv->color[COLORSEL_BLUE],
                      &priv->color[COLORSEL_HUE],
                      &priv->color[COLORSEL_SATURATION],
                      &priv->color[COLORSEL_VALUE]);
      break;
    default:
      priv->color[GPOINTER_TO_INT (data)] = gtk_adjustment_get_value (adjustment) / 255;
      break;
    }
  update_color (colorsel);
}

static void
opacity_entry_changed (GtkWidget *opacity_entry,
                       gpointer   data)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;
  GtkAdjustment *adj;
  gchar *text;

  colorsel = GTK_COLOR_SELECTION (data);
  priv = colorsel->private_data;

  if (priv->changing)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (priv->opacity_entry), 0, -1);
  adj = gtk_range_get_adjustment (GTK_RANGE (priv->opacity_slider));
  gtk_adjustment_set_value (adj, g_strtod (text, NULL));

  update_color (colorsel);

  g_free (text);
}

static void
make_label_spinbutton (GtkColorSelection *colorsel,
                       GtkWidget        **spinbutton,
                       gchar             *text,
                       GtkWidget         *table,
                       gint               i,
                       gint               j,
                       gint               channel_type,
                       const gchar       *tooltip)
{
  GtkWidget *label;
  GtkAdjustment *adjust;

  if (channel_type == COLORSEL_HUE)
    {
      adjust = gtk_adjustment_new (0.0, 0.0, 360.0, 1.0, 1.0, 0.0);
    }
  else if (channel_type == COLORSEL_SATURATION ||
           channel_type == COLORSEL_VALUE)
    {
      adjust = gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 1.0, 0.0);
    }
  else
    {
      adjust = gtk_adjustment_new (0.0, 0.0, 255.0, 1.0, 1.0, 0.0);
    }
  g_object_set_data (G_OBJECT (adjust), I_("COLORSEL"), colorsel);
  *spinbutton = gtk_spin_button_new (adjust, 10.0, 0);

  gtk_widget_set_tooltip_text (*spinbutton, tooltip);

  g_signal_connect (adjust, "value-changed",
                    G_CALLBACK (adjustment_changed),
                    GINT_TO_POINTER (channel_type));
  label = gtk_label_new_with_mnemonic (text);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), *spinbutton);

  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_grid_attach (GTK_GRID (table), label, i, j, 1, 1);
  gtk_grid_attach (GTK_GRID (table), *spinbutton, i+1, j, 1, 1);
}

static void
make_palette_frame (GtkColorSelection *colorsel,
                    GtkWidget         *table,
                    gint               i,
                    gint               j)
{
  GtkWidget *frame;
  GtkColorSelectionPrivate *priv;

  priv = colorsel->private_data;
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  priv->custom_palette[i][j] = palette_new (colorsel);
  gtk_widget_set_size_request (priv->custom_palette[i][j], CUSTOM_PALETTE_ENTRY_WIDTH, CUSTOM_PALETTE_ENTRY_HEIGHT);
  gtk_container_add (GTK_CONTAINER (frame), priv->custom_palette[i][j]);
  gtk_grid_attach (GTK_GRID (table), frame, i, j, 1, 1);
}

/* Set the palette entry [x][y] to be the currently selected one. */
static void
set_selected_palette (GtkColorSelection *colorsel, int x, int y)
{
  GtkColorSelectionPrivate *priv = colorsel->private_data;

  gtk_widget_grab_focus (priv->custom_palette[x][y]);
}

static double
scale_round (double val, double factor)
{
  val = floor (val * factor + 0.5);
  val = MAX (val, 0);
  val = MIN (val, factor);
  return val;
}

static void
update_color (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv = colorsel->private_data;
  gchar entryval[12];
  gchar opacity_text[32];
  gchar *ptr;

  priv->changing = TRUE;
  color_sample_update_samples (colorsel);

  gtk_hsv_set_color (GTK_HSV (priv->triangle_colorsel),
                     priv->color[COLORSEL_HUE],
                     priv->color[COLORSEL_SATURATION],
                     priv->color[COLORSEL_VALUE]);
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->hue_spinbutton)),
                            scale_round (priv->color[COLORSEL_HUE], 360));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->sat_spinbutton)),
                            scale_round (priv->color[COLORSEL_SATURATION], 100));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->val_spinbutton)),
                            scale_round (priv->color[COLORSEL_VALUE], 100));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->red_spinbutton)),
                            scale_round (priv->color[COLORSEL_RED], 255));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->green_spinbutton)),
                            scale_round (priv->color[COLORSEL_GREEN], 255));
  gtk_adjustment_set_value (gtk_spin_button_get_adjustment
                            (GTK_SPIN_BUTTON (priv->blue_spinbutton)),
                            scale_round (priv->color[COLORSEL_BLUE], 255));
  gtk_adjustment_set_value (gtk_range_get_adjustment
                            (GTK_RANGE (priv->opacity_slider)),
                            scale_round (priv->color[COLORSEL_OPACITY], 255));

  g_snprintf (opacity_text, 32, "%.0f", scale_round (priv->color[COLORSEL_OPACITY], 255));
  gtk_entry_set_text (GTK_ENTRY (priv->opacity_entry), opacity_text);

  g_snprintf (entryval, 11, "#%2X%2X%2X",
              (guint) (scale_round (priv->color[COLORSEL_RED], 255)),
              (guint) (scale_round (priv->color[COLORSEL_GREEN], 255)),
              (guint) (scale_round (priv->color[COLORSEL_BLUE], 255)));

  for (ptr = entryval; *ptr; ptr++)
    if (*ptr == ' ')
      *ptr = '0';
  gtk_entry_set_text (GTK_ENTRY (priv->hex_entry), entryval);
  priv->changing = FALSE;

  g_object_ref (colorsel);

  g_signal_emit (colorsel, color_selection_signals[COLOR_CHANGED], 0);

  g_object_freeze_notify (G_OBJECT (colorsel));
  g_object_notify (G_OBJECT (colorsel), "current-color");
  g_object_notify (G_OBJECT (colorsel), "current-alpha");
  g_object_thaw_notify (G_OBJECT (colorsel));

  g_object_unref (colorsel);
}

static void
update_palette (GtkColorSelection *colorsel)
{
  GdkColor *current_colors;
  gint i, j;

  current_colors = get_current_colors (colorsel);

  for (i = 0; i < GTK_CUSTOM_PALETTE_HEIGHT; i++)
    {
      for (j = 0; j < GTK_CUSTOM_PALETTE_WIDTH; j++)
        {
          gint index;

          index = i * GTK_CUSTOM_PALETTE_WIDTH + j;

          gtk_color_selection_set_palette_color (colorsel,
                                                 index,
                                                 &current_colors[index]);
        }
    }

  g_free (current_colors);
}

static void
palette_change_notify_instance (GObject    *object,
                                GParamSpec *pspec,
                                gpointer    data)
{
  update_palette (GTK_COLOR_SELECTION (data));
}

static void
default_noscreen_change_palette_func (const GdkColor *colors,
                                      gint            n_colors)
{
  default_change_palette_func (gdk_screen_get_default (), colors, n_colors);
}

static void
default_change_palette_func (GdkScreen      *screen,
                             const GdkColor *colors,
                             gint            n_colors)
{
  gchar *str;

  str = gtk_color_selection_palette_to_string (colors, n_colors);

  gtk_settings_set_string_property (gtk_settings_get_for_screen (screen),
                                    "gtk-color-palette",
                                    str,
                                    "gtk_color_selection_palette_to_string");

  g_free (str);
}

/**
 * gtk_color_selection_new:
 *
 * Creates a new GtkColorSelection.
 *
 * Returns: a new #GtkColorSelection
 */
GtkWidget *
gtk_color_selection_new (void)
{
  GtkColorSelection *colorsel;
  GtkColorSelectionPrivate *priv;
  gdouble color[4];
  color[0] = 1.0;
  color[1] = 1.0;
  color[2] = 1.0;
  color[3] = 1.0;

  colorsel = g_object_new (GTK_TYPE_COLOR_SELECTION, NULL);
  priv = colorsel->private_data;
  set_color_internal (colorsel, color);
  gtk_color_selection_set_has_opacity_control (colorsel, TRUE);

  /* We want to make sure that default_set is FALSE.
   * This way the user can still set it.
   */
  priv->default_set = FALSE;
  priv->default_alpha_set = FALSE;

  return GTK_WIDGET (colorsel);
}

/**
 * gtk_color_selection_get_has_opacity_control:
 * @colorsel: a #GtkColorSelection
 *
 * Determines whether the colorsel has an opacity control.
 *
 * Returns: %TRUE if the @colorsel has an opacity control,
 *     %FALSE if it does't
 */
gboolean
gtk_color_selection_get_has_opacity_control (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);

  priv = colorsel->private_data;

  return priv->has_opacity;
}

/**
 * gtk_color_selection_set_has_opacity_control:
 * @colorsel: a #GtkColorSelection
 * @has_opacity: %TRUE if @colorsel can set the opacity, %FALSE otherwise
 *
 * Sets the @colorsel to use or not use opacity.
 */
void
gtk_color_selection_set_has_opacity_control (GtkColorSelection *colorsel,
                                             gboolean           has_opacity)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  priv = colorsel->private_data;
  has_opacity = has_opacity != FALSE;

  if (priv->has_opacity != has_opacity)
    {
      priv->has_opacity = has_opacity;
      if (has_opacity)
        {
          gtk_widget_show (priv->opacity_slider);
          gtk_widget_show (priv->opacity_label);
          gtk_widget_show (priv->opacity_entry);
        }
      else
        {
          gtk_widget_hide (priv->opacity_slider);
          gtk_widget_hide (priv->opacity_label);
          gtk_widget_hide (priv->opacity_entry);
        }
      color_sample_update_samples (colorsel);

      g_object_notify (G_OBJECT (colorsel), "has-opacity-control");
    }
}

/**
 * gtk_color_selection_get_has_palette:
 * @colorsel: a #GtkColorSelection
 *
 * Determines whether the color selector has a color palette.
 *
 * Returns: %TRUE if the selector has a palette, %FALSE if it hasn't
 */
gboolean
gtk_color_selection_get_has_palette (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);

  priv = colorsel->private_data;

  return priv->has_palette;
}

/**
 * gtk_color_selection_set_has_palette:
 * @colorsel: a #GtkColorSelection
 * @has_palette: %TRUE if palette is to be visible, %FALSE otherwise
 *
 * Shows and hides the palette based upon the value of @has_palette.
 */
void
gtk_color_selection_set_has_palette (GtkColorSelection *colorsel,
                                     gboolean           has_palette)
{
  GtkColorSelectionPrivate *priv;
  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  priv = colorsel->private_data;
  has_palette = has_palette != FALSE;

  if (priv->has_palette != has_palette)
    {
      priv->has_palette = has_palette;
      if (has_palette)
        gtk_widget_show (priv->palette_frame);
      else
        gtk_widget_hide (priv->palette_frame);

      update_tooltips (colorsel);

      g_object_notify (G_OBJECT (colorsel), "has-palette");
    }
}

/**
 * gtk_color_selection_set_current_color:
 * @colorsel: a #GtkColorSelection
 * @color: a #GdkColor to set the current color with
 *
 * Sets the current color to be @color.
 *
 * The first time this is called, it will also set
 * the original color to be @color too.
 *
 * Deprecated: 3.4: Use gtk_color_selection_set_current_rgba() instead.
 */
void
gtk_color_selection_set_current_color (GtkColorSelection *colorsel,
                                       const GdkColor    *color)
{
  GtkColorSelectionPrivate *priv;
  gint i;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_RED] = SCALE (color->red);
  priv->color[COLORSEL_GREEN] = SCALE (color->green);
  priv->color[COLORSEL_BLUE] = SCALE (color->blue);
  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                  priv->color[COLORSEL_GREEN],
                  priv->color[COLORSEL_BLUE],
                  &priv->color[COLORSEL_HUE],
                  &priv->color[COLORSEL_SATURATION],
                  &priv->color[COLORSEL_VALUE]);
  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
        priv->old_color[i] = priv->color[i];
    }
  priv->default_set = TRUE;
  update_color (colorsel);
}

/**
 * gtk_color_selection_set_current_alpha:
 * @colorsel: a #GtkColorSelection
 * @alpha: an integer between 0 and 65535
 *
 * Sets the current opacity to be @alpha.
 *
 * The first time this is called, it will also set
 * the original opacity to be @alpha too.
 */
void
gtk_color_selection_set_current_alpha (GtkColorSelection *colorsel,
                                       guint16            alpha)
{
  GtkColorSelectionPrivate *priv;
  gint i;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->color[COLORSEL_OPACITY] = SCALE (alpha);
  if (priv->default_alpha_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
        priv->old_color[i] = priv->color[i];
    }
  priv->default_alpha_set = TRUE;
  update_color (colorsel);
}

/**
 * gtk_color_selection_get_current_color:
 * @colorsel: a #GtkColorSelection
 * @color: (out): a #GdkColor to fill in with the current color
 *
 * Sets @color to be the current color in the GtkColorSelection widget.
 *
 * Deprecated: 3.4: Use gtk_color_selection_get_current_rgba() instead.
 */
void
gtk_color_selection_get_current_color (GtkColorSelection *colorsel,
                                       GdkColor          *color)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);

  priv = colorsel->private_data;
  color->red = UNSCALE (priv->color[COLORSEL_RED]);
  color->green = UNSCALE (priv->color[COLORSEL_GREEN]);
  color->blue = UNSCALE (priv->color[COLORSEL_BLUE]);
}

/**
 * gtk_color_selection_get_current_alpha:
 * @colorsel: a #GtkColorSelection
 *
 * Returns the current alpha value.
 *
 * Returns: an integer between 0 and 65535
 */
guint16
gtk_color_selection_get_current_alpha (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), 0);

  priv = colorsel->private_data;
  return priv->has_opacity ? UNSCALE (priv->color[COLORSEL_OPACITY]) : 65535;
}

/**
 * gtk_color_selection_set_previous_color:
 * @colorsel: a #GtkColorSelection
 * @color: a #GdkColor to set the previous color with
 *
 * Sets the “previous” color to be @color.
 *
 * This function should be called with some hesitations,
 * as it might seem confusing to have that color change.
 * Calling gtk_color_selection_set_current_color() will also
 * set this color the first time it is called.
 *
 * Deprecated: 3.4: Use gtk_color_selection_set_previous_rgba() instead.
 */
void
gtk_color_selection_set_previous_color (GtkColorSelection *colorsel,
                                        const GdkColor    *color)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->old_color[COLORSEL_RED] = SCALE (color->red);
  priv->old_color[COLORSEL_GREEN] = SCALE (color->green);
  priv->old_color[COLORSEL_BLUE] = SCALE (color->blue);
  gtk_rgb_to_hsv (priv->old_color[COLORSEL_RED],
                  priv->old_color[COLORSEL_GREEN],
                  priv->old_color[COLORSEL_BLUE],
                  &priv->old_color[COLORSEL_HUE],
                  &priv->old_color[COLORSEL_SATURATION],
                  &priv->old_color[COLORSEL_VALUE]);
  color_sample_update_samples (colorsel);
  priv->default_set = TRUE;
  priv->changing = FALSE;
}

/**
 * gtk_color_selection_set_previous_alpha:
 * @colorsel: a #GtkColorSelection
 * @alpha: an integer between 0 and 65535
 *
 * Sets the “previous” alpha to be @alpha.
 *
 * This function should be called with some hesitations,
 * as it might seem confusing to have that alpha change.
 */
void
gtk_color_selection_set_previous_alpha (GtkColorSelection *colorsel,
                                        guint16            alpha)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));

  priv = colorsel->private_data;
  priv->changing = TRUE;
  priv->old_color[COLORSEL_OPACITY] = SCALE (alpha);
  color_sample_update_samples (colorsel);
  priv->default_alpha_set = TRUE;
  priv->changing = FALSE;
}


/**
 * gtk_color_selection_get_previous_color:
 * @colorsel: a #GtkColorSelection
 * @color: (out): a #GdkColor to fill in with the original color value
 *
 * Fills @color in with the original color value.
 *
 * Deprecated: 3.4: Use gtk_color_selection_get_previous_rgba() instead.
 */
void
gtk_color_selection_get_previous_color (GtkColorSelection *colorsel,
                                        GdkColor           *color)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (color != NULL);

  priv = colorsel->private_data;
  color->red = UNSCALE (priv->old_color[COLORSEL_RED]);
  color->green = UNSCALE (priv->old_color[COLORSEL_GREEN]);
  color->blue = UNSCALE (priv->old_color[COLORSEL_BLUE]);
}

/**
 * gtk_color_selection_get_previous_alpha:
 * @colorsel: a #GtkColorSelection
 *
 * Returns the previous alpha value.
 *
 * Returns: an integer between 0 and 65535
 */
guint16
gtk_color_selection_get_previous_alpha (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), 0);

  priv = colorsel->private_data;
  return priv->has_opacity ? UNSCALE (priv->old_color[COLORSEL_OPACITY]) : 65535;
}

/**
 * gtk_color_selection_set_current_rgba:
 * @colorsel: a #GtkColorSelection
 * @rgba: A #GdkRGBA to set the current color with
 *
 * Sets the current color to be @rgba.
 *
 * The first time this is called, it will also set
 * the original color to be @rgba too.
 *
 * Since: 3.0
 */
void
gtk_color_selection_set_current_rgba (GtkColorSelection *colorsel,
                                      const GdkRGBA     *rgba)
{
  GtkColorSelectionPrivate *priv;
  gint i;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (rgba != NULL);

  priv = colorsel->private_data;
  priv->changing = TRUE;

  priv->color[COLORSEL_RED] = CLAMP (rgba->red, 0, 1);
  priv->color[COLORSEL_GREEN] = CLAMP (rgba->green, 0, 1);
  priv->color[COLORSEL_BLUE] = CLAMP (rgba->blue, 0, 1);
  priv->color[COLORSEL_OPACITY] = CLAMP (rgba->alpha, 0, 1);

  gtk_rgb_to_hsv (priv->color[COLORSEL_RED],
                  priv->color[COLORSEL_GREEN],
                  priv->color[COLORSEL_BLUE],
                  &priv->color[COLORSEL_HUE],
                  &priv->color[COLORSEL_SATURATION],
                  &priv->color[COLORSEL_VALUE]);

  if (priv->default_set == FALSE)
    {
      for (i = 0; i < COLORSEL_NUM_CHANNELS; i++)
        priv->old_color[i] = priv->color[i];
    }

  priv->default_set = TRUE;
  update_color (colorsel);
}

/**
 * gtk_color_selection_get_current_rgba:
 * @colorsel: a #GtkColorSelection
 * @rgba: (out): a #GdkRGBA to fill in with the current color
 *
 * Sets @rgba to be the current color in the GtkColorSelection widget.
 *
 * Since: 3.0
 */
void
gtk_color_selection_get_current_rgba (GtkColorSelection *colorsel,
                                      GdkRGBA           *rgba)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (rgba != NULL);

  priv = colorsel->private_data;
  rgba->red = priv->color[COLORSEL_RED];
  rgba->green = priv->color[COLORSEL_GREEN];
  rgba->blue = priv->color[COLORSEL_BLUE];
  rgba->alpha = (priv->has_opacity) ? priv->color[COLORSEL_OPACITY] : 1;
}

/**
 * gtk_color_selection_set_previous_rgba:
 * @colorsel: a #GtkColorSelection
 * @rgba: a #GdkRGBA to set the previous color with
 *
 * Sets the “previous” color to be @rgba.
 *
 * This function should be called with some hesitations,
 * as it might seem confusing to have that color change.
 * Calling gtk_color_selection_set_current_rgba() will also
 * set this color the first time it is called.
 *
 * Since: 3.0
 */
void
gtk_color_selection_set_previous_rgba (GtkColorSelection *colorsel,
                                       const GdkRGBA     *rgba)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (rgba != NULL);

  priv = colorsel->private_data;
  priv->changing = TRUE;

  priv->old_color[COLORSEL_RED] = CLAMP (rgba->red, 0, 1);
  priv->old_color[COLORSEL_GREEN] = CLAMP (rgba->green, 0, 1);
  priv->old_color[COLORSEL_BLUE] = CLAMP (rgba->blue, 0, 1);
  priv->old_color[COLORSEL_OPACITY] = CLAMP (rgba->alpha, 0, 1);

  gtk_rgb_to_hsv (priv->old_color[COLORSEL_RED],
                  priv->old_color[COLORSEL_GREEN],
                  priv->old_color[COLORSEL_BLUE],
                  &priv->old_color[COLORSEL_HUE],
                  &priv->old_color[COLORSEL_SATURATION],
                  &priv->old_color[COLORSEL_VALUE]);

  color_sample_update_samples (colorsel);
  priv->default_set = TRUE;
  priv->changing = FALSE;
}

/**
 * gtk_color_selection_get_previous_rgba:
 * @colorsel: a #GtkColorSelection
 * @rgba: (out): a #GdkRGBA to fill in with the original color value
 *
 * Fills @rgba in with the original color value.
 *
 * Since: 3.0
 */
void
gtk_color_selection_get_previous_rgba (GtkColorSelection *colorsel,
                                       GdkRGBA           *rgba)
{
  GtkColorSelectionPrivate *priv;

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (rgba != NULL);

  priv = colorsel->private_data;
  rgba->red = priv->old_color[COLORSEL_RED];
  rgba->green = priv->old_color[COLORSEL_GREEN];
  rgba->blue = priv->old_color[COLORSEL_BLUE];
  rgba->alpha = (priv->has_opacity) ? priv->old_color[COLORSEL_OPACITY] : 1;
}

/**
 * gtk_color_selection_set_palette_color:
 * @colorsel: a #GtkColorSelection
 * @index: the color index of the palette
 * @color: A #GdkColor to set the palette with
 *
 * Sets the palette located at @index to have @color as its color.
 */
static void
gtk_color_selection_set_palette_color (GtkColorSelection *colorsel,
                                       gint               index,
                                       GdkColor          *color)
{
  GtkColorSelectionPrivate *priv;
  gint x, y;
  gdouble col[3];

  g_return_if_fail (GTK_IS_COLOR_SELECTION (colorsel));
  g_return_if_fail (index >= 0  && index < GTK_CUSTOM_PALETTE_WIDTH*GTK_CUSTOM_PALETTE_HEIGHT);

  x = index % GTK_CUSTOM_PALETTE_WIDTH;
  y = index / GTK_CUSTOM_PALETTE_WIDTH;

  priv = colorsel->private_data;
  col[0] = SCALE (color->red);
  col[1] = SCALE (color->green);
  col[2] = SCALE (color->blue);

  palette_set_color (priv->custom_palette[x][y], colorsel, col);
}

/**
 * gtk_color_selection_is_adjusting:
 * @colorsel: a #GtkColorSelection
 *
 * Gets the current state of the @colorsel.
 *
 * Returns: %TRUE if the user is currently dragging
 *     a color around, and %FALSE if the selection has stopped
 */
gboolean
gtk_color_selection_is_adjusting (GtkColorSelection *colorsel)
{
  GtkColorSelectionPrivate *priv;

  g_return_val_if_fail (GTK_IS_COLOR_SELECTION (colorsel), FALSE);

  priv = colorsel->private_data;

  return (gtk_hsv_is_adjusting (GTK_HSV (priv->triangle_colorsel)));
}


/**
 * gtk_color_selection_palette_from_string:
 * @str: a string encoding a color palette
 * @colors: (out) (array length=n_colors): return location for
 *     allocated array of #GdkColor
 * @n_colors: return location for length of array
 *
 * Parses a color palette string; the string is a colon-separated
 * list of color names readable by gdk_color_parse().
 *
 * Returns: %TRUE if a palette was successfully parsed
 */
gboolean
gtk_color_selection_palette_from_string (const gchar  *str,
                                         GdkColor    **colors,
                                         gint         *n_colors)
{
  GdkColor *retval;
  gint count;
  gchar *p;
  gchar *start;
  gchar *copy;

  count = 0;
  retval = NULL;
  copy = g_strdup (str);

  start = copy;
  p = copy;
  while (TRUE)
    {
      if (*p == ':' || *p == '\0')
        {
          gboolean done = TRUE;

          if (start == p)
            {
              goto failed; /* empty entry */
            }

          if (*p)
            {
              *p = '\0';
              done = FALSE;
            }

          retval = g_renew (GdkColor, retval, count + 1);
          if (!gdk_color_parse (start, retval + count))
            {
              goto failed;
            }

          ++count;

          if (done)
            break;
          else
            start = p + 1;
        }

      ++p;
    }

  g_free (copy);

  if (colors)
    *colors = retval;
  else
    g_free (retval);

  if (n_colors)
    *n_colors = count;

  return TRUE;

 failed:
  g_free (copy);
  g_free (retval);

  if (colors)
    *colors = NULL;
  if (n_colors)
    *n_colors = 0;

  return FALSE;
}

/**
 * gtk_color_selection_palette_to_string:
 * @colors: (array length=n_colors): an array of colors
 * @n_colors: length of the array
 *
 * Encodes a palette as a string, useful for persistent storage.
 *
 * Returns: allocated string encoding the palette
 */
gchar*
gtk_color_selection_palette_to_string (const GdkColor *colors,
                                       gint            n_colors)
{
  gint i;
  gchar **strs = NULL;
  gchar *retval;

  if (n_colors == 0)
    return g_strdup ("");

  strs = g_new0 (gchar*, n_colors + 1);

  i = 0;
  while (i < n_colors)
    {
      gchar *ptr;

      strs[i] =
        g_strdup_printf ("#%2X%2X%2X",
                         colors[i].red / 256,
                         colors[i].green / 256,
                         colors[i].blue / 256);

      for (ptr = strs[i]; *ptr; ptr++)
        if (*ptr == ' ')
          *ptr = '0';

      ++i;
    }

  retval = g_strjoinv (":", strs);

  g_strfreev (strs);

  return retval;
}

/**
 * gtk_color_selection_set_change_palette_with_screen_hook: (skip)
 * @func: a function to call when the custom palette needs saving
 *
 * Installs a global function to be called whenever the user
 * tries to modify the palette in a color selection.
 *
 * This function should save the new palette contents, and update
 * the #GtkSettings:gtk-color-palette GtkSettings property so all
 * GtkColorSelection widgets will be modified.
 *
 * Returns: the previous change palette hook (that was replaced)
 *
 * Since: 2.2
 */
GtkColorSelectionChangePaletteWithScreenFunc
gtk_color_selection_set_change_palette_with_screen_hook (GtkColorSelectionChangePaletteWithScreenFunc func)
{
  GtkColorSelectionChangePaletteWithScreenFunc old;

  old = change_palette_hook;

  change_palette_hook = func;

  return old;
}

static void
make_control_relations (AtkObject *atk_obj,
                        GtkWidget *widget)
{
  AtkObject *obj;

  obj = gtk_widget_get_accessible (widget);
  atk_object_add_relationship (atk_obj, ATK_RELATION_CONTROLLED_BY, obj);
  atk_object_add_relationship (obj, ATK_RELATION_CONTROLLER_FOR, atk_obj);
}

static void
make_all_relations (AtkObject                *atk_obj,
                    GtkColorSelectionPrivate *priv)
{
  make_control_relations (atk_obj, priv->hue_spinbutton);
  make_control_relations (atk_obj, priv->sat_spinbutton);
  make_control_relations (atk_obj, priv->val_spinbutton);
  make_control_relations (atk_obj, priv->red_spinbutton);
  make_control_relations (atk_obj, priv->green_spinbutton);
  make_control_relations (atk_obj, priv->blue_spinbutton);
}
