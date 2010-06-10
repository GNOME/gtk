/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity Theme Rendering */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef META_THEME_H
#define META_THEME_H

#include "boxes.h"
#include "gradient.h"
#include "common.h"
#include <gtk/gtk.h>

typedef struct _MetaFrameStyle MetaFrameStyle;
typedef struct _MetaFrameStyleSet MetaFrameStyleSet;
typedef struct _MetaDrawOp MetaDrawOp;
typedef struct _MetaDrawOpList MetaDrawOpList;
typedef struct _MetaGradientSpec MetaGradientSpec;
typedef struct _MetaAlphaGradientSpec MetaAlphaGradientSpec; 
typedef struct _MetaColorSpec MetaColorSpec;
typedef struct _MetaFrameLayout MetaFrameLayout;
typedef struct _MetaButtonSpace MetaButtonSpace;
typedef struct _MetaFrameGeometry MetaFrameGeometry;
typedef struct _MetaTheme MetaTheme;
typedef struct _MetaPositionExprEnv MetaPositionExprEnv;
typedef struct _MetaDrawInfo MetaDrawInfo;

#define META_THEME_ERROR (g_quark_from_static_string ("meta-theme-error"))

typedef enum
{
  META_THEME_ERROR_FRAME_GEOMETRY,
  META_THEME_ERROR_BAD_CHARACTER,
  META_THEME_ERROR_BAD_PARENS,
  META_THEME_ERROR_UNKNOWN_VARIABLE,
  META_THEME_ERROR_DIVIDE_BY_ZERO,
  META_THEME_ERROR_MOD_ON_FLOAT,
  META_THEME_ERROR_FAILED
} MetaThemeError;

/**
 * Whether a button's size is calculated from the area around it (aspect
 * sizing) or is given as a fixed height and width in pixels (fixed sizing).
 *
 * \bug This could be done away with; see the comment at the top of
 * MetaFrameLayout.
 */
typedef enum
{
  META_BUTTON_SIZING_ASPECT,
  META_BUTTON_SIZING_FIXED,
  META_BUTTON_SIZING_LAST
} MetaButtonSizing;

/**
 * Various parameters used to calculate the geometry of a frame.
 * They are used inside a MetaFrameStyle.
 * This corresponds closely to the <frame_geometry> tag in a theme file.
 *
 * \bug button_sizing isn't really necessary, because we could easily say
 * that if button_aspect is zero, the height and width are fixed values.
 * This would also mean that MetaButtonSizing didn't need to exist, and
 * save code.
 **/
struct _MetaFrameLayout
{
  /** Reference count. */
  int refcount;
  
  /** Size of left side */
  int left_width;
  /** Size of right side */
  int right_width;
  /** Size of bottom side */
  int bottom_height;
  
  /** Border of blue title region
   * \bug (blue?!)
   **/
  GtkBorder title_border;

  /** Extra height for inside of title region, above the font height */
  int title_vertical_pad;
  
  /** Right indent of buttons from edges of frame */
  int right_titlebar_edge;
  /** Left indent of buttons from edges of frame */
  int left_titlebar_edge;
  
  /**
   * Sizing rule of buttons, either META_BUTTON_SIZING_ASPECT
   * (in which case button_aspect will be honoured, and
   * button_width and button_height set from it), or
   * META_BUTTON_SIZING_FIXED (in which case we read the width
   * and height directly).
   */
  MetaButtonSizing button_sizing;

  /**
   * Ratio of height/width. Honoured only if
   * button_sizing==META_BUTTON_SIZING_ASPECT.
   * Otherwise we figure out the height from the button_border.
   */
  double button_aspect;
  
  /** Width of a button; set even when we are using aspect sizing */
  int button_width;

  /** Height of a button; set even when we are using aspect sizing */
  int button_height;

  /** Space around buttons */
  GtkBorder button_border;

  /** scale factor for title text */
  double title_scale;
  
  /** Whether title text will be displayed */
  guint has_title : 1;

  /** Whether we should hide the buttons */
  guint hide_buttons : 1;

  /** Radius of the top left-hand corner; 0 if not rounded */
  guint top_left_corner_rounded_radius;
  /** Radius of the top right-hand corner; 0 if not rounded */
  guint top_right_corner_rounded_radius;
  /** Radius of the bottom left-hand corner; 0 if not rounded */
  guint bottom_left_corner_rounded_radius;
  /** Radius of the bottom right-hand corner; 0 if not rounded */
  guint bottom_right_corner_rounded_radius;
};

/**
 * The computed size of a button (really just a way of tying its
 * visible and clickable areas together).
 * The reason for two different rectangles here is Fitts' law & maximized
 * windows; see bug #97703 for more details.
 */
struct _MetaButtonSpace
{
  /** The screen area where the button's image is drawn */
  GdkRectangle visible;
  /** The screen area where the button can be activated by clicking */
  GdkRectangle clickable;
};

/**
 * Calculated actual geometry of the frame
 */
struct _MetaFrameGeometry
{
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;

  int width;
  int height;  

  GdkRectangle title_rect;

  int left_titlebar_edge;
  int right_titlebar_edge;
  int top_titlebar_edge;
  int bottom_titlebar_edge;

  /* used for a memset hack */
#define ADDRESS_OF_BUTTON_RECTS(fgeom) (((char*)(fgeom)) + G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
#define LENGTH_OF_BUTTON_RECTS (G_STRUCT_OFFSET (MetaFrameGeometry, right_right_background) + sizeof (GdkRectangle) - G_STRUCT_OFFSET (MetaFrameGeometry, close_rect))
  
  /* The button rects (if changed adjust memset hack) */
  MetaButtonSpace close_rect;
  MetaButtonSpace max_rect;
  MetaButtonSpace min_rect;
  MetaButtonSpace menu_rect;
  MetaButtonSpace shade_rect;
  MetaButtonSpace above_rect;
  MetaButtonSpace stick_rect;
  MetaButtonSpace unshade_rect;
  MetaButtonSpace unabove_rect;
  MetaButtonSpace unstick_rect;

#define MAX_MIDDLE_BACKGROUNDS (MAX_BUTTONS_PER_CORNER - 2)
  GdkRectangle left_left_background;
  GdkRectangle left_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle left_right_background;
  GdkRectangle right_left_background;
  GdkRectangle right_middle_backgrounds[MAX_MIDDLE_BACKGROUNDS];
  GdkRectangle right_right_background;
  /* End of button rects (if changed adjust memset hack) */
  
  /* Round corners */
  guint top_left_corner_rounded_radius;
  guint top_right_corner_rounded_radius;
  guint bottom_left_corner_rounded_radius;
  guint bottom_right_corner_rounded_radius;
};

typedef enum
{
  META_IMAGE_FILL_SCALE, /* default, needs to be all-bits-zero for g_new0 */
  META_IMAGE_FILL_TILE
} MetaImageFillType;

typedef enum
{
  META_COLOR_SPEC_BASIC,
  META_COLOR_SPEC_GTK,
  META_COLOR_SPEC_BLEND,
  META_COLOR_SPEC_SHADE
} MetaColorSpecType;

typedef enum
{
  META_GTK_COLOR_FG,
  META_GTK_COLOR_BG,
  META_GTK_COLOR_LIGHT,
  META_GTK_COLOR_DARK,
  META_GTK_COLOR_MID,
  META_GTK_COLOR_TEXT,
  META_GTK_COLOR_BASE,
  META_GTK_COLOR_TEXT_AA,
  META_GTK_COLOR_LAST
} MetaGtkColorComponent;

struct _MetaColorSpec
{
  MetaColorSpecType type;
  union
  {
    struct {
      GdkColor color;
    } basic;
    struct {
      MetaGtkColorComponent component;
      GtkStateType state;
    } gtk;
    struct {
      MetaColorSpec *foreground;
      MetaColorSpec *background;
      double alpha;

      GdkColor color;
    } blend;
    struct {
      MetaColorSpec *base;
      double factor;

      GdkColor color;
    } shade;
  } data;
};

struct _MetaGradientSpec
{
  MetaGradientType type;
  GSList *color_specs;
};

struct _MetaAlphaGradientSpec
{
  MetaGradientType type;
  unsigned char *alphas;
  int n_alphas;
};

struct _MetaDrawInfo
{
  GdkPixbuf   *mini_icon;
  GdkPixbuf   *icon;
  PangoLayout *title_layout;
  int title_layout_width;
  int title_layout_height;
  const MetaFrameGeometry *fgeom;
};

/**
 * A drawing operation in our simple vector drawing language.
 */
typedef enum
{
  /** Basic drawing-- line */
  META_DRAW_LINE,
  /** Basic drawing-- rectangle */
  META_DRAW_RECTANGLE,
  /** Basic drawing-- arc */
  META_DRAW_ARC,

  /** Clip to a rectangle */
  META_DRAW_CLIP,
  
  /* Texture thingies */

  /** Just a filled rectangle with alpha */
  META_DRAW_TINT,
  META_DRAW_GRADIENT,
  META_DRAW_IMAGE,
  
  /** GTK theme engine stuff */
  META_DRAW_GTK_ARROW,
  META_DRAW_GTK_BOX,
  META_DRAW_GTK_VLINE,

  /** App's window icon */
  META_DRAW_ICON,
  /** App's window title */
  META_DRAW_TITLE,
  /** a draw op list */
  META_DRAW_OP_LIST,
  /** tiled draw op list */
  META_DRAW_TILE
} MetaDrawType;

typedef enum
{
  POS_TOKEN_INT,
  POS_TOKEN_DOUBLE,
  POS_TOKEN_OPERATOR,
  POS_TOKEN_VARIABLE,
  POS_TOKEN_OPEN_PAREN,
  POS_TOKEN_CLOSE_PAREN
} PosTokenType;

typedef enum
{
  POS_OP_NONE,
  POS_OP_ADD,
  POS_OP_SUBTRACT,
  POS_OP_MULTIPLY,
  POS_OP_DIVIDE,
  POS_OP_MOD,
  POS_OP_MAX,
  POS_OP_MIN
} PosOperatorType;

/**
 * A token, as output by the tokeniser.
 *
 * \ingroup tokenizer
 */
typedef struct
{
  PosTokenType type;

  union
  {
    struct {
      int val;
    } i;

    struct {
      double val;
    } d;

    struct {
      PosOperatorType op;
    } o;

    struct {
      char *name;
      GQuark name_quark;
    } v;

  } d;
} PosToken;

/**
 * A computed expression in our simple vector drawing language.
 * While it appears to take the form of a tree, this is actually
 * merely a list; concerns such as precedence of operators are
 * currently recomputed on every recalculation.
 *
 * Created by meta_draw_spec_new(), destroyed by meta_draw_spec_free().
 * pos_eval() fills this with ...FIXME. Are tokens a tree or a list?
 * \ingroup parser
 */
typedef struct _MetaDrawSpec
{
  /**
   * If this spec is constant, this is the value of the constant;
   * otherwise it is zero.
   */
  int value;
  
  /** A list of tokens in the expression. */
  PosToken *tokens;

  /** How many tokens are in the tokens list. */
  int n_tokens;

  /** Does the expression contain any variables? */
  gboolean constant : 1;
} MetaDrawSpec;

/**
 * A single drawing operation in our simple vector drawing language.
 */
struct _MetaDrawOp
{
  MetaDrawType type;

  /* Positions are strings because they can be expressions */
  union
  {
    struct {
      MetaColorSpec *color_spec;
      int dash_on_length;
      int dash_off_length;
      int width;
      MetaDrawSpec *x1;
      MetaDrawSpec *y1;
      MetaDrawSpec *x2;
      MetaDrawSpec *y2;
    } line;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } rectangle;

    struct {
      MetaColorSpec *color_spec;
      gboolean filled;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      double start_angle;
      double extent_angle;
    } arc;

    struct {
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } clip;
    
    struct {
      MetaColorSpec *color_spec;
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } tint;

    struct {
      MetaGradientSpec *gradient_spec;
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gradient;

    struct {
      MetaColorSpec *colorize_spec;
      MetaAlphaGradientSpec *alpha_spec;
      GdkPixbuf *pixbuf;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;

      guint32 colorize_cache_pixel;
      GdkPixbuf *colorize_cache_pixbuf;
      MetaImageFillType fill_type;
      unsigned int vertical_stripes : 1;
      unsigned int horizontal_stripes : 1;
    } image;
    
    struct {
      GtkStateType state;
      GtkShadowType shadow;
      GtkArrowType arrow;
      gboolean filled;

      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gtk_arrow;

    struct {
      GtkStateType state;
      GtkShadowType shadow;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } gtk_box;

    struct {
      GtkStateType state;
      MetaDrawSpec *x;
      MetaDrawSpec *y1;
      MetaDrawSpec *y2;  
    } gtk_vline;

    struct {
      MetaAlphaGradientSpec *alpha_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      MetaImageFillType fill_type;
    } icon;

    struct {
      MetaColorSpec *color_spec;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *ellipsize_width;
    } title;

    struct {
      MetaDrawOpList *op_list;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
    } op_list;

    struct {
      MetaDrawOpList *op_list;
      MetaDrawSpec *x;
      MetaDrawSpec *y;
      MetaDrawSpec *width;
      MetaDrawSpec *height;
      MetaDrawSpec *tile_xoffset;
      MetaDrawSpec *tile_yoffset;
      MetaDrawSpec *tile_width;
      MetaDrawSpec *tile_height;
    } tile;
    
  } data;
};

/**
 * A list of MetaDrawOp objects. Maintains a reference count.
 * Grows as necessary and allows the allocation of unused spaces
 * to keep reallocations to a minimum.
 *
 * \bug Do we really win anything from not using the equivalent
 * GLib structures?
 */
struct _MetaDrawOpList
{
  int refcount;
  MetaDrawOp **ops;
  int n_ops;
  int n_allocated;
};

typedef enum
{
  META_BUTTON_STATE_NORMAL,
  META_BUTTON_STATE_PRESSED,
  META_BUTTON_STATE_PRELIGHT,
  META_BUTTON_STATE_LAST
} MetaButtonState;

typedef enum
{
  /* Ordered so that background is drawn first */
  META_BUTTON_TYPE_LEFT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_LEFT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_LEFT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_LEFT_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_MIDDLE_BACKGROUND,
  META_BUTTON_TYPE_RIGHT_RIGHT_BACKGROUND,
  META_BUTTON_TYPE_CLOSE,
  META_BUTTON_TYPE_MAXIMIZE,
  META_BUTTON_TYPE_MINIMIZE,
  META_BUTTON_TYPE_MENU,
  META_BUTTON_TYPE_SHADE,
  META_BUTTON_TYPE_ABOVE,
  META_BUTTON_TYPE_STICK,
  META_BUTTON_TYPE_UNSHADE,
  META_BUTTON_TYPE_UNABOVE,
  META_BUTTON_TYPE_UNSTICK,
  META_BUTTON_TYPE_LAST
} MetaButtonType;

typedef enum
{
  META_MENU_ICON_TYPE_CLOSE,
  META_MENU_ICON_TYPE_MAXIMIZE,
  META_MENU_ICON_TYPE_UNMAXIMIZE,
  META_MENU_ICON_TYPE_MINIMIZE,
  META_MENU_ICON_TYPE_LAST
} MetaMenuIconType;

typedef enum
{
  /* Listed in the order in which the textures are drawn.
   * (though this only matters for overlaps of course.)
   * Buttons are drawn after the frame textures.
   *
   * On the corners, horizontal pieces are arbitrarily given the
   * corner area:
   *
   *   =====                 |====
   *   |                     |
   *   |       rather than   |
   *
   */
  
  /* entire frame */
  META_FRAME_PIECE_ENTIRE_BACKGROUND,
  /* entire titlebar background */
  META_FRAME_PIECE_TITLEBAR,
  /* portion of the titlebar background inside the titlebar
   * background edges
   */
  META_FRAME_PIECE_TITLEBAR_MIDDLE,
  /* left end of titlebar */
  META_FRAME_PIECE_LEFT_TITLEBAR_EDGE,
  /* right end of titlebar */
  META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE,
  /* top edge of titlebar */
  META_FRAME_PIECE_TOP_TITLEBAR_EDGE,
  /* bottom edge of titlebar */
  META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE,
  /* render over title background (text area) */
  META_FRAME_PIECE_TITLE,
  /* left edge of the frame */
  META_FRAME_PIECE_LEFT_EDGE,
  /* right edge of the frame */
  META_FRAME_PIECE_RIGHT_EDGE,
  /* bottom edge of the frame */
  META_FRAME_PIECE_BOTTOM_EDGE,
  /* place over entire frame, after drawing everything else */
  META_FRAME_PIECE_OVERLAY,
  /* Used to get size of the enum */
  META_FRAME_PIECE_LAST
} MetaFramePiece;

#define N_GTK_STATES 5

/**
 * How to draw a frame in a particular state (say, a focussed, non-maximised,
 * resizable frame). This corresponds closely to the <frame_style> tag
 * in a theme file.
 */
struct _MetaFrameStyle
{
  /** Reference count. */
  int refcount;
  /**
   * Parent style.
   * Settings which are unspecified here will be taken from there.
   */
  MetaFrameStyle *parent;
  /** Operations for drawing each kind of button in each state. */
  MetaDrawOpList *buttons[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST];
  /** Operations for drawing each piece of the frame. */
  MetaDrawOpList *pieces[META_FRAME_PIECE_LAST];
  /**
   * Details such as the height and width of each edge, the corner rounding,
   * and the aspect ratio of the buttons.
   */
  MetaFrameLayout *layout;
  /**
   * Background colour of the window. Only present in theme formats
   * 2 and above. Can be NULL to use the standard GTK theme engine.
   */
  MetaColorSpec *window_background_color;
  /**
   * Transparency of the window background. 0=transparent; 255=opaque.
   */
  guint8 window_background_alpha;
};

/* Kinds of frame...
 * 
 *  normal ->   noresize / vert only / horz only / both
 *              focused / unfocused
 *  max    ->   focused / unfocused
 *  shaded ->   focused / unfocused
 *  max/shaded -> focused / unfocused
 *
 *  so 4 states with 8 sub-states in one, 2 sub-states in the other 3,
 *  meaning 14 total
 *
 * 14 window states times 7 or 8 window types. Except some
 * window types never get a frame so that narrows it down a bit.
 * 
 */
typedef enum
{
  META_FRAME_STATE_NORMAL,
  META_FRAME_STATE_MAXIMIZED,
  META_FRAME_STATE_SHADED,
  META_FRAME_STATE_MAXIMIZED_AND_SHADED,
  META_FRAME_STATE_LAST
} MetaFrameState;

typedef enum
{
  META_FRAME_RESIZE_NONE,
  META_FRAME_RESIZE_VERTICAL,
  META_FRAME_RESIZE_HORIZONTAL,
  META_FRAME_RESIZE_BOTH,
  META_FRAME_RESIZE_LAST
} MetaFrameResize;

typedef enum
{
  META_FRAME_FOCUS_NO,
  META_FRAME_FOCUS_YES,
  META_FRAME_FOCUS_LAST
} MetaFrameFocus;

/**
 * How to draw frames at different times: when it's maximised or not, shaded
 * or not, when it's focussed or not, and (for non-maximised windows), when
 * it can be horizontally or vertically resized, both, or neither.
 * Not all window types actually get a frame.
 *
 * A theme contains one of these objects for each type of window (each
 * MetaFrameType), that is, normal, dialogue (modal and non-modal), etc.
 *
 * This corresponds closely to the <frame_style_set> tag in a theme file.
 */
struct _MetaFrameStyleSet
{
  int refcount;
  MetaFrameStyleSet *parent;
  MetaFrameStyle *normal_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_styles[META_FRAME_FOCUS_LAST];
  MetaFrameStyle *shaded_styles[META_FRAME_RESIZE_LAST][META_FRAME_FOCUS_LAST];
  MetaFrameStyle *maximized_and_shaded_styles[META_FRAME_FOCUS_LAST];
};

/**
 * A theme. This is a singleton class which groups all settings from a theme
 * on disk together.
 *
 * \bug It is rather useless to keep the metadata fields in core, I think.
 */
struct _MetaTheme
{
  /** Name of the theme (on disk), e.g. "Crux" */
  char *name;
  /** Path to the files associated with the theme */
  char *dirname;
  /**
   * Filename of the XML theme file.
   * \bug Kept lying around for no discernable reason.
   */
  char *filename;
  /** Metadata: Human-readable name of the theme. */
  char *readable_name;
  /** Metadata: Author of the theme. */
  char *author;
  /** Metadata: Copyright holder. */
  char *copyright;
  /** Metadata: Date of the theme. */
  char *date;
  /** Metadata: Description of the theme. */
  char *description;
  /** Version of the theme format. Older versions cannot use the features
   * of newer versions even if they think they can (this is to allow forward
   * and backward compatibility.
   */
  guint format_version;

  /** Symbol table of integer constants. */
  GHashTable *integer_constants;
  /** Symbol table of float constants. */
  GHashTable *float_constants;
  /**
   * Symbol table of colour constants (hex triples, and triples
   * plus alpha).
   * */
  GHashTable *color_constants;
  GHashTable *images_by_filename;
  GHashTable *layouts_by_name;
  GHashTable *draw_op_lists_by_name;
  GHashTable *styles_by_name;
  GHashTable *style_sets_by_name;
  MetaFrameStyleSet *style_sets_by_type[META_FRAME_TYPE_LAST];

  GQuark quark_width;
  GQuark quark_height;
  GQuark quark_object_width;
  GQuark quark_object_height;
  GQuark quark_left_width;
  GQuark quark_right_width;
  GQuark quark_top_height;
  GQuark quark_bottom_height;
  GQuark quark_mini_icon_width;
  GQuark quark_mini_icon_height;
  GQuark quark_icon_width;
  GQuark quark_icon_height;
  GQuark quark_title_width;
  GQuark quark_title_height;
  GQuark quark_frame_x_center;
  GQuark quark_frame_y_center;
};

struct _MetaPositionExprEnv
{
  MetaRectangle rect;
  /* size of an object being drawn, if it has a natural size */
  int object_width;
  int object_height;
  /* global object sizes, always available */
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;
  int title_width;
  int title_height;
  int frame_x_center;
  int frame_y_center;
  int mini_icon_width;
  int mini_icon_height;
  int icon_width;
  int icon_height;
  /* Theme so we can look up constants */
  MetaTheme *theme;
};

MetaFrameLayout* meta_frame_layout_new           (void);
MetaFrameLayout* meta_frame_layout_copy          (const MetaFrameLayout *src);
void             meta_frame_layout_ref           (MetaFrameLayout       *layout);
void             meta_frame_layout_unref         (MetaFrameLayout       *layout);
void             meta_frame_layout_get_borders   (const MetaFrameLayout *layout,
                                                  int                    text_height,
                                                  MetaFrameFlags         flags,
                                                  int                   *top_height,
                                                  int                   *bottom_height,
                                                  int                   *left_width,
                                                  int                   *right_width);
void             meta_frame_layout_calc_geometry (const MetaFrameLayout  *layout,
                                                  int                     text_height,
                                                  MetaFrameFlags          flags,
                                                  int                     client_width,
                                                  int                     client_height,
                                                  const MetaButtonLayout *button_layout,
                                                  MetaFrameGeometry      *fgeom,
                                                  MetaTheme              *theme);

gboolean         meta_frame_layout_validate      (const MetaFrameLayout *layout,
                                                  GError               **error);

gboolean meta_parse_position_expression (MetaDrawSpec               *spec,
                                         const MetaPositionExprEnv  *env,
                                         int                        *x_return,
                                         int                        *y_return,
                                         GError                    **err);
gboolean meta_parse_size_expression     (MetaDrawSpec               *spec,
                                         const MetaPositionExprEnv  *env,
                                         int                        *val_return,
                                         GError                    **err);

MetaDrawSpec* meta_draw_spec_new (MetaTheme  *theme,
                                  const char *expr,
                                  GError    **error);
void          meta_draw_spec_free (MetaDrawSpec *spec);

MetaColorSpec* meta_color_spec_new             (MetaColorSpecType  type);
MetaColorSpec* meta_color_spec_new_from_string (const char        *str,
                                                GError           **err);
MetaColorSpec* meta_color_spec_new_gtk         (MetaGtkColorComponent component,
                                                GtkStateType          state);
void           meta_color_spec_free            (MetaColorSpec     *spec);
void           meta_color_spec_render          (MetaColorSpec     *spec,
                                                GtkWidget         *widget,
                                                GdkColor          *color);


MetaDrawOp*    meta_draw_op_new  (MetaDrawType        type);
void           meta_draw_op_free (MetaDrawOp          *op);
void           meta_draw_op_draw (const MetaDrawOp    *op,
                                  GtkWidget           *widget,
                                  GdkDrawable         *drawable,
                                  const GdkRectangle  *clip,
                                  const MetaDrawInfo  *info,
                                  /* logical region being drawn */
                                  MetaRectangle        logical_region);

void           meta_draw_op_draw_with_style (const MetaDrawOp    *op,
                                             GtkStyle            *style_gtk,
                                             GtkWidget           *widget,
                                             GdkDrawable         *drawable,
                                             const GdkRectangle  *clip,
                                             const MetaDrawInfo  *info,
                                             /* logical region being drawn */
                                             MetaRectangle        logical_region);

MetaDrawOpList* meta_draw_op_list_new   (int                   n_preallocs);
void            meta_draw_op_list_ref   (MetaDrawOpList       *op_list);
void            meta_draw_op_list_unref (MetaDrawOpList       *op_list);
void            meta_draw_op_list_draw  (const MetaDrawOpList *op_list,
                                         GtkWidget            *widget,
                                         GdkDrawable          *drawable,
                                         const GdkRectangle   *clip,
                                         const MetaDrawInfo   *info,
                                         MetaRectangle         rect);
void            meta_draw_op_list_draw_with_style  (const MetaDrawOpList *op_list,
                                                    GtkStyle             *style_gtk,
                                                    GtkWidget            *widget,
                                                    GdkDrawable          *drawable,
                                                    const GdkRectangle   *clip,
                                                    const MetaDrawInfo   *info,
                                                    MetaRectangle         rect);
void           meta_draw_op_list_append (MetaDrawOpList       *op_list,
                                         MetaDrawOp           *op);
gboolean       meta_draw_op_list_validate (MetaDrawOpList    *op_list,
                                           GError           **error);
gboolean       meta_draw_op_list_contains (MetaDrawOpList    *op_list,
                                           MetaDrawOpList    *child);

MetaGradientSpec* meta_gradient_spec_new    (MetaGradientType        type);
void              meta_gradient_spec_free   (MetaGradientSpec       *desc);
GdkPixbuf*        meta_gradient_spec_render (const MetaGradientSpec *desc,
                                             GtkWidget              *widget,
                                             int                     width,
                                             int                     height);
gboolean          meta_gradient_spec_validate (MetaGradientSpec     *spec,
                                               GError              **error);

MetaAlphaGradientSpec* meta_alpha_gradient_spec_new  (MetaGradientType       type,
                                                      int                    n_alphas);
void                   meta_alpha_gradient_spec_free (MetaAlphaGradientSpec *spec);


MetaFrameStyle* meta_frame_style_new   (MetaFrameStyle *parent);
void            meta_frame_style_ref   (MetaFrameStyle *style);
void            meta_frame_style_unref (MetaFrameStyle *style);

void meta_frame_style_draw (MetaFrameStyle          *style,
                            GtkWidget               *widget,
                            GdkDrawable             *drawable,
                            int                      x_offset,
                            int                      y_offset,
                            const GdkRectangle      *clip,
                            const MetaFrameGeometry *fgeom,
                            int                      client_width,
                            int                      client_height,
                            PangoLayout             *title_layout,
                            int                      text_height,
                            MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf               *mini_icon,
                            GdkPixbuf               *icon);


void meta_frame_style_draw_with_style (MetaFrameStyle          *style,
                                       GtkStyle                *style_gtk,
                                       GtkWidget               *widget,
                                       GdkDrawable             *drawable,
                                       int                      x_offset,
                                       int                      y_offset,
                                       const GdkRectangle      *clip,
                                       const MetaFrameGeometry *fgeom,
                                       int                      client_width,
                                       int                      client_height,
                                       PangoLayout             *title_layout,
                                       int                      text_height,
                                       MetaButtonState          button_states[META_BUTTON_TYPE_LAST],
                                       GdkPixbuf               *mini_icon,
                                       GdkPixbuf               *icon);


gboolean       meta_frame_style_validate (MetaFrameStyle    *style,
                                          guint              current_theme_version,
                                          GError           **error);

MetaFrameStyleSet* meta_frame_style_set_new   (MetaFrameStyleSet *parent);
void               meta_frame_style_set_ref   (MetaFrameStyleSet *style_set);
void               meta_frame_style_set_unref (MetaFrameStyleSet *style_set);

gboolean       meta_frame_style_set_validate  (MetaFrameStyleSet *style_set,
                                               GError           **error);

MetaTheme* meta_theme_get_current (void);
void       meta_theme_set_current (const char *name,
                                   gboolean    force_reload);

MetaTheme* meta_theme_new      (void);
void       meta_theme_free     (MetaTheme *theme);
gboolean   meta_theme_validate (MetaTheme *theme,
                                GError   **error);
GdkPixbuf* meta_theme_load_image (MetaTheme  *theme,
                                  const char *filename,
                                  guint       size_of_theme_icons,
                                  GError    **error);

MetaFrameStyle* meta_theme_get_frame_style (MetaTheme     *theme,
                                            MetaFrameType  type,
                                            MetaFrameFlags flags);

double meta_theme_get_title_scale (MetaTheme     *theme,
                                   MetaFrameType  type,
                                   MetaFrameFlags flags);

void meta_theme_draw_frame (MetaTheme              *theme,
                            GtkWidget              *widget,
                            GdkDrawable            *drawable,
                            const GdkRectangle     *clip,
                            int                     x_offset,
                            int                     y_offset,
                            MetaFrameType           type,
                            MetaFrameFlags          flags,
                            int                     client_width,
                            int                     client_height,
                            PangoLayout            *title_layout,
                            int                     text_height,
                            const MetaButtonLayout *button_layout,
                            MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                            GdkPixbuf              *mini_icon,
                            GdkPixbuf              *icon);

void meta_theme_draw_frame_by_name (MetaTheme              *theme,
                                    GtkWidget              *widget,
                                    GdkDrawable            *drawable,
                                    const GdkRectangle     *clip,
                                    int                     x_offset,
                                    int                     y_offset,
                                    const gchar             *style_name,
                                    MetaFrameFlags          flags,
                                    int                     client_width,
                                    int                     client_height,
                                    PangoLayout            *title_layout,
                                    int                     text_height,
                                    const MetaButtonLayout *button_layout,
                                    MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                                    GdkPixbuf              *mini_icon,
                                    GdkPixbuf              *icon);

void meta_theme_draw_frame_with_style (MetaTheme              *theme,
                                       GtkStyle               *style_gtk,
                                       GtkWidget              *widget,
                                       GdkDrawable            *drawable,
                                       const GdkRectangle     *clip,
                                       int                     x_offset,
                                       int                     y_offset,
                                       MetaFrameType           type,
                                       MetaFrameFlags          flags,
                                       int                     client_width,
                                       int                     client_height,
                                       PangoLayout            *title_layout,
                                       int                     text_height,
                                       const MetaButtonLayout *button_layout,
                                       MetaButtonState         button_states[META_BUTTON_TYPE_LAST],
                                       GdkPixbuf              *mini_icon,
                                       GdkPixbuf              *icon);

void meta_theme_get_frame_borders (MetaTheme         *theme,
                                   MetaFrameType      type,
                                   int                text_height,
                                   MetaFrameFlags     flags,
                                   int               *top_height,
                                   int               *bottom_height,
                                   int               *left_width,
                                   int               *right_width);
void meta_theme_calc_geometry (MetaTheme              *theme,
                               MetaFrameType           type,
                               int                     text_height,
                               MetaFrameFlags          flags,
                               int                     client_width,
                               int                     client_height,
                               const MetaButtonLayout *button_layout,
                               MetaFrameGeometry      *fgeom);
                                   
MetaFrameLayout*   meta_theme_lookup_layout       (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_layout       (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameLayout   *layout);
MetaDrawOpList*    meta_theme_lookup_draw_op_list (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_draw_op_list (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaDrawOpList    *op_list);
MetaFrameStyle*    meta_theme_lookup_style        (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_style        (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameStyle    *style);
MetaFrameStyleSet* meta_theme_lookup_style_set    (MetaTheme         *theme,
                                                   const char        *name);
void               meta_theme_insert_style_set    (MetaTheme         *theme,
                                                   const char        *name,
                                                   MetaFrameStyleSet *style_set);
gboolean meta_theme_define_int_constant   (MetaTheme   *theme,
                                           const char  *name,
                                           int          value,
                                           GError     **error);
gboolean meta_theme_lookup_int_constant   (MetaTheme   *theme,
                                           const char  *name,
                                           int         *value);
gboolean meta_theme_define_float_constant (MetaTheme   *theme,
                                           const char  *name,
                                           double       value,
                                           GError     **error);
gboolean meta_theme_lookup_float_constant (MetaTheme   *theme,
                                           const char  *name,
                                           double      *value);

gboolean meta_theme_define_color_constant (MetaTheme   *theme,
                                           const char  *name,
                                           const char  *value,
                                           GError     **error);
gboolean meta_theme_lookup_color_constant (MetaTheme   *theme,
                                           const char  *name,
                                           char       **value);

gboolean     meta_theme_replace_constants     (MetaTheme    *theme,
                                               PosToken     *tokens,
                                               int           n_tokens,
                                               GError      **err);

/* random stuff */

PangoFontDescription* meta_gtk_widget_get_font_desc        (GtkWidget            *widget,
                                                            double                scale,
							    const PangoFontDescription *override);
int                   meta_pango_font_desc_get_text_height (const PangoFontDescription *font_desc,
                                                            PangoContext         *context);


/* Enum converters */
MetaGtkColorComponent meta_color_component_from_string (const char            *str);
const char*           meta_color_component_to_string   (MetaGtkColorComponent  component);
MetaButtonState       meta_button_state_from_string    (const char            *str);
const char*           meta_button_state_to_string      (MetaButtonState        state);
MetaButtonType        meta_button_type_from_string     (const char            *str,
                                                        MetaTheme             *theme);
const char*           meta_button_type_to_string       (MetaButtonType         type);
MetaFramePiece        meta_frame_piece_from_string     (const char            *str);
const char*           meta_frame_piece_to_string       (MetaFramePiece         piece);
MetaFrameState        meta_frame_state_from_string     (const char            *str);
const char*           meta_frame_state_to_string       (MetaFrameState         state);
MetaFrameResize       meta_frame_resize_from_string    (const char            *str);
const char*           meta_frame_resize_to_string      (MetaFrameResize        resize);
MetaFrameFocus        meta_frame_focus_from_string     (const char            *str);
const char*           meta_frame_focus_to_string       (MetaFrameFocus         focus);
MetaFrameType         meta_frame_type_from_string      (const char            *str);
const char*           meta_frame_type_to_string        (MetaFrameType          type);
MetaGradientType      meta_gradient_type_from_string   (const char            *str);
const char*           meta_gradient_type_to_string     (MetaGradientType       type);
GtkStateType          meta_gtk_state_from_string       (const char            *str);
const char*           meta_gtk_state_to_string         (GtkStateType           state);
GtkShadowType         meta_gtk_shadow_from_string      (const char            *str);
const char*           meta_gtk_shadow_to_string        (GtkShadowType          shadow);
GtkArrowType          meta_gtk_arrow_from_string       (const char            *str);
const char*           meta_gtk_arrow_to_string         (GtkArrowType           arrow);
MetaImageFillType     meta_image_fill_type_from_string (const char            *str);
const char*           meta_image_fill_type_to_string   (MetaImageFillType      fill_type);

guint meta_theme_earliest_version_with_button (MetaButtonType type);


#define META_THEME_ALLOWS(theme, feature) (theme->format_version >= feature)

/* What version of the theme file format were various features introduced in? */
#define META_THEME_SHADE_STICK_ABOVE_BUTTONS 2
#define META_THEME_UBIQUITOUS_CONSTANTS 2
#define META_THEME_VARIED_ROUND_CORNERS 2
#define META_THEME_IMAGES_FROM_ICON_THEMES 2
#define META_THEME_UNRESIZABLE_SHADED_STYLES 2
#define META_THEME_DEGREES_IN_ARCS 2
#define META_THEME_HIDDEN_BUTTONS 2
#define META_THEME_COLOR_CONSTANTS 2
#define META_THEME_FRAME_BACKGROUNDS 2

#endif
