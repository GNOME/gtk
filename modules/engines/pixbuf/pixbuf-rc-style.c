/* GTK+ Pixbuf Engine
 * Copyright (C) 1998-2000 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Owen Taylor <otaylor@redhat.com>, based on code by
 * Carsten Haitzler <raster@rasterman.com>
 */

#include "pixbuf.h"
#include "pixbuf-style.h"
#include "pixbuf-rc-style.h"

static void      pixbuf_rc_style_init         (PixbufRcStyle      *style);
static void      pixbuf_rc_style_class_init   (PixbufRcStyleClass *klass);
static void      pixbuf_rc_style_finalize     (GObject            *object);
static guint     pixbuf_rc_style_parse        (GtkRcStyle         *rc_style,
					       GtkSettings  *settings,
					       GScanner           *scanner);
static void      pixbuf_rc_style_merge        (GtkRcStyle         *dest,
					       GtkRcStyle         *src);
static GtkStyle *pixbuf_rc_style_create_style (GtkRcStyle         *rc_style);

static void theme_image_unref (ThemeImage *data);

static const struct
  {
    gchar              *name;
    guint               token;
  }
theme_symbols[] =
{
  { "image", 		TOKEN_IMAGE  },
  { "function", 	TOKEN_FUNCTION },
  { "file", 		TOKEN_FILE },
  { "stretch", 		TOKEN_STRETCH },
  { "recolorable", 	TOKEN_RECOLORABLE },
  { "border", 		TOKEN_BORDER },
  { "detail", 		TOKEN_DETAIL },
  { "state", 		TOKEN_STATE },
  { "shadow", 		TOKEN_SHADOW },
  { "gap_side", 	TOKEN_GAP_SIDE },
  { "gap_file", 	TOKEN_GAP_FILE },
  { "gap_border", 	TOKEN_GAP_BORDER },
  { "gap_start_file", 	TOKEN_GAP_START_FILE },
  { "gap_start_border", TOKEN_GAP_START_BORDER },
  { "gap_end_file", 	TOKEN_GAP_END_FILE },
  { "gap_end_border", 	TOKEN_GAP_END_BORDER },
  { "overlay_file", 	TOKEN_OVERLAY_FILE },
  { "overlay_border", 	TOKEN_OVERLAY_BORDER },
  { "overlay_stretch", 	TOKEN_OVERLAY_STRETCH },
  { "arrow_direction", 	TOKEN_ARROW_DIRECTION },
  { "orientation", 	TOKEN_ORIENTATION },
  { "expander_style",	TOKEN_EXPANDER_STYLE },
  { "window_edge",	TOKEN_WINDOW_EDGE },

  { "HLINE", 		TOKEN_D_HLINE },
  { "VLINE",		TOKEN_D_VLINE },
  { "SHADOW",		TOKEN_D_SHADOW },
  { "POLYGON",		TOKEN_D_POLYGON },
  { "ARROW",		TOKEN_D_ARROW },
  { "DIAMOND",		TOKEN_D_DIAMOND },
  { "OVAL",		TOKEN_D_OVAL },
  { "STRING",		TOKEN_D_STRING },
  { "BOX",		TOKEN_D_BOX },
  { "FLAT_BOX",		TOKEN_D_FLAT_BOX },
  { "CHECK",		TOKEN_D_CHECK },
  { "OPTION",		TOKEN_D_OPTION },
  { "CROSS",		TOKEN_D_CROSS },
  { "RAMP",		TOKEN_D_RAMP },
  { "TAB",		TOKEN_D_TAB },
  { "SHADOW_GAP",	TOKEN_D_SHADOW_GAP },
  { "BOX_GAP",		TOKEN_D_BOX_GAP },
  { "EXTENSION",	TOKEN_D_EXTENSION },
  { "FOCUS",		TOKEN_D_FOCUS },
  { "SLIDER",		TOKEN_D_SLIDER },
  { "ENTRY",		TOKEN_D_ENTRY },
  { "HANDLE",		TOKEN_D_HANDLE },
  { "STEPPER",		TOKEN_D_STEPPER },
  { "EXPANDER",		TOKEN_D_EXPANDER },
  { "RESIZE_GRIP",	TOKEN_D_RESIZE_GRIP },

  { "TRUE",		TOKEN_TRUE },
  { "FALSE",		TOKEN_FALSE },

  { "TOP",		TOKEN_TOP },
  { "UP",		TOKEN_UP },
  { "BOTTOM",		TOKEN_BOTTOM },
  { "DOWN",		TOKEN_DOWN },
  { "LEFT",		TOKEN_LEFT },
  { "RIGHT",		TOKEN_RIGHT },

  { "NORMAL",		TOKEN_NORMAL },
  { "ACTIVE",		TOKEN_ACTIVE },
  { "PRELIGHT",		TOKEN_PRELIGHT },
  { "SELECTED",		TOKEN_SELECTED },
  { "INSENSITIVE",	TOKEN_INSENSITIVE },

  { "NONE",		TOKEN_NONE },
  { "IN",		TOKEN_IN },
  { "OUT",		TOKEN_OUT },
  { "ETCHED_IN",	TOKEN_ETCHED_IN },
  { "ETCHED_OUT",	TOKEN_ETCHED_OUT },

  { "HORIZONTAL",	TOKEN_HORIZONTAL },
  { "VERTICAL",		TOKEN_VERTICAL },

  { "COLLAPSED",	TOKEN_COLLAPSED },
  { "SEMI_COLLAPSED",	TOKEN_SEMI_COLLAPSED },
  { "SEMI_EXPANDED",	TOKEN_SEMI_EXPANDED },
  { "EXPANDED",		TOKEN_EXPANDED },

  { "NORTH_WEST",	TOKEN_NORTH_WEST },
  { "NORTH",		TOKEN_NORTH },
  { "NORTH_EAST",	TOKEN_NORTH_EAST },
  { "WEST",		TOKEN_WEST },
  { "EAST",		TOKEN_EAST },
  { "SOUTH_WEST",	TOKEN_SOUTH_WEST },
  { "SOUTH",		TOKEN_SOUTH },
  { "SOUTH_EAST",	TOKEN_SOUTH_EAST }
};

static GtkRcStyleClass *parent_class;

GType pixbuf_type_rc_style = 0;

void
pixbuf_rc_style_register_type (GTypeModule *module)
{
  const GTypeInfo object_info =
  {
    sizeof (PixbufRcStyleClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) pixbuf_rc_style_class_init,
    NULL,           /* class_finalize */
    NULL,           /* class_data */
    sizeof (PixbufRcStyle),
    0,              /* n_preallocs */
    (GInstanceInitFunc) pixbuf_rc_style_init,
  };
  
  pixbuf_type_rc_style = g_type_module_register_type (module,
						      GTK_TYPE_RC_STYLE,
						      "PixbufRcStyle",
						      &object_info, 0);
}

static void
pixbuf_rc_style_init (PixbufRcStyle *style)
{
}

static void
pixbuf_rc_style_class_init (PixbufRcStyleClass *klass)
{
  GtkRcStyleClass *rc_style_class = GTK_RC_STYLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  rc_style_class->parse = pixbuf_rc_style_parse;
  rc_style_class->merge = pixbuf_rc_style_merge;
  rc_style_class->create_style = pixbuf_rc_style_create_style;
  
  object_class->finalize = pixbuf_rc_style_finalize;
}

static void
pixbuf_rc_style_finalize (GObject *object)
{
  PixbufRcStyle *rc_style = PIXBUF_RC_STYLE (object);
  
  g_list_free_full (rc_style->img_list, theme_image_unref);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static guint
theme_parse_file(GtkSettings  *settings,
		 GScanner     *scanner,
		 ThemePixbuf **theme_pb)
{
  guint token;
  gchar *pixmap;

  /* Skip 'blah_file' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (!*theme_pb)
    *theme_pb = theme_pixbuf_new ();

  pixmap = gtk_rc_find_pixmap_in_path(settings, scanner, scanner->value.v_string);
  if (pixmap)
    {
      theme_pixbuf_set_filename (*theme_pb, pixmap);
      g_free (pixmap);
    }

  return G_TOKEN_NONE;
}

static guint
theme_parse_border (GScanner     *scanner,
		    ThemePixbuf **theme_pb)
{
  guint               token;
  gint left, right, top, bottom;

  /* Skip 'blah_border' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  if (!*theme_pb)
    *theme_pb = theme_pixbuf_new ();
  
  theme_pixbuf_set_border (*theme_pb, left, right, top, bottom);
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_stretch(GScanner     *scanner,
		    ThemePixbuf **theme_pb)
{
  guint token;
  gboolean stretch;

  /* Skip 'blah_stretch' */
  token = g_scanner_get_next_token(scanner);

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_TRUE)
    stretch = TRUE;
  else if (token == TOKEN_FALSE)
    stretch = FALSE;
  else
    return TOKEN_TRUE;

  if (!*theme_pb)
    *theme_pb = theme_pixbuf_new ();
  
  theme_pixbuf_set_stretch (*theme_pb, stretch);
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_recolorable(GScanner * scanner,
			ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_RECOLORABLE)
    return TOKEN_RECOLORABLE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_TRUE)
    data->recolorable = 1;
  else if (token == TOKEN_FALSE)
    data->recolorable = 0;
  else
    return TOKEN_TRUE;

  return G_TOKEN_NONE;
}

static guint
theme_parse_function(GScanner * scanner,
		     ThemeImage *data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_FUNCTION)
    return TOKEN_FUNCTION;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if ((token >= TOKEN_D_HLINE) && (token <= TOKEN_D_RESIZE_GRIP))
    data->match_data.function = token;

  return G_TOKEN_NONE;
}

static guint
theme_parse_detail(GScanner * scanner,
		   ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_DETAIL)
    return TOKEN_DETAIL;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_free (data->match_data.detail);
  
  data->match_data.detail = g_strdup(scanner->value.v_string);

  return G_TOKEN_NONE;
}

static guint
theme_parse_state(GScanner * scanner,
		  ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_STATE)
    return TOKEN_STATE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_NORMAL)
    data->match_data.state = GTK_STATE_NORMAL;
  else if (token == TOKEN_ACTIVE)
    data->match_data.state = GTK_STATE_ACTIVE;
  else if (token == TOKEN_PRELIGHT)
    data->match_data.state = GTK_STATE_PRELIGHT;
  else if (token == TOKEN_SELECTED)
    data->match_data.state = GTK_STATE_SELECTED;
  else if (token == TOKEN_INSENSITIVE)
    data->match_data.state = GTK_STATE_INSENSITIVE;
  else
    return TOKEN_NORMAL;

  data->match_data.flags |= THEME_MATCH_STATE;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_shadow(GScanner * scanner,
		   ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_SHADOW)
    return TOKEN_SHADOW;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_NONE)
    data->match_data.shadow = GTK_SHADOW_NONE;
  else if (token == TOKEN_IN)
    data->match_data.shadow = GTK_SHADOW_IN;
  else if (token == TOKEN_OUT)
    data->match_data.shadow = GTK_SHADOW_OUT;
  else if (token == TOKEN_ETCHED_IN)
    data->match_data.shadow = GTK_SHADOW_ETCHED_IN;
  else if (token == TOKEN_ETCHED_OUT)
    data->match_data.shadow = GTK_SHADOW_ETCHED_OUT;
  else
    return TOKEN_NONE;

  data->match_data.flags |= THEME_MATCH_SHADOW;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_arrow_direction(GScanner * scanner,
			    ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_ARROW_DIRECTION)
    return TOKEN_ARROW_DIRECTION;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_UP)
    data->match_data.arrow_direction = GTK_ARROW_UP;
  else if (token == TOKEN_DOWN)
    data->match_data.arrow_direction = GTK_ARROW_DOWN;
  else if (token == TOKEN_LEFT)
    data->match_data.arrow_direction = GTK_ARROW_LEFT;
  else if (token == TOKEN_RIGHT)
    data->match_data.arrow_direction = GTK_ARROW_RIGHT;
  else
    return TOKEN_UP;

  data->match_data.flags |= THEME_MATCH_ARROW_DIRECTION;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_gap_side(GScanner * scanner,
		     ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_SIDE)
    return TOKEN_GAP_SIDE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);

  if (token == TOKEN_TOP)
    data->match_data.gap_side = GTK_POS_TOP;
  else if (token == TOKEN_BOTTOM)
    data->match_data.gap_side = GTK_POS_BOTTOM;
  else if (token == TOKEN_LEFT)
    data->match_data.gap_side = GTK_POS_LEFT;
  else if (token == TOKEN_RIGHT)
    data->match_data.gap_side = GTK_POS_RIGHT;
  else
    return TOKEN_TOP;

  data->match_data.flags |= THEME_MATCH_GAP_SIDE;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_orientation(GScanner * scanner,
			ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_ORIENTATION)
    return TOKEN_ORIENTATION;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);

  if (token == TOKEN_HORIZONTAL)
    data->match_data.orientation = GTK_ORIENTATION_HORIZONTAL;
  else if (token == TOKEN_VERTICAL)
    data->match_data.orientation = GTK_ORIENTATION_VERTICAL;
  else
    return TOKEN_HORIZONTAL;

  data->match_data.flags |= THEME_MATCH_ORIENTATION;
  
  return G_TOKEN_NONE;
}

static guint
theme_parse_expander_style(GScanner * scanner,
			   ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_EXPANDER_STYLE)
    return TOKEN_EXPANDER_STYLE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_COLLAPSED)
    data->match_data.expander_style = GTK_EXPANDER_COLLAPSED;
  else if (token == TOKEN_SEMI_COLLAPSED)
    data->match_data.expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
  else if (token == TOKEN_SEMI_EXPANDED)
    data->match_data.expander_style = GTK_EXPANDER_SEMI_EXPANDED;
  else if (token == TOKEN_EXPANDED)
    data->match_data.expander_style = GTK_EXPANDER_EXPANDED;
  else
    return TOKEN_COLLAPSED;

  data->match_data.flags |= THEME_MATCH_EXPANDER_STYLE;

  return G_TOKEN_NONE;
}

static guint
theme_parse_window_edge(GScanner * scanner,
			ThemeImage * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_WINDOW_EDGE)
    return TOKEN_WINDOW_EDGE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_NORTH_WEST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_NORTH_WEST;
  else if (token == TOKEN_NORTH)
    data->match_data.window_edge = GDK_WINDOW_EDGE_NORTH;
  else if (token == TOKEN_NORTH_EAST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_NORTH_EAST;
  else if (token == TOKEN_WEST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_WEST;
  else if (token == TOKEN_EAST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_EAST;
  else if (token == TOKEN_SOUTH_WEST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_SOUTH_WEST;
  else if (token == TOKEN_SOUTH)
    data->match_data.window_edge = GDK_WINDOW_EDGE_SOUTH;
  else if (token == TOKEN_SOUTH_EAST)
    data->match_data.window_edge = GDK_WINDOW_EDGE_SOUTH_EAST;
  else
    return TOKEN_NORTH_WEST;

  data->match_data.flags |= THEME_MATCH_WINDOW_EDGE;

  return G_TOKEN_NONE;
}

static void
theme_image_ref (ThemeImage *data)
{
  data->refcount++;
}

static void
theme_image_unref (ThemeImage *data)
{
  data->refcount--;
  if (data->refcount == 0)
    {
      g_free (data->match_data.detail);
      if (data->background)
	theme_pixbuf_destroy (data->background);
      if (data->overlay)
	theme_pixbuf_destroy (data->overlay);
      if (data->gap_start)
	theme_pixbuf_destroy (data->gap_start);
      if (data->gap)
	theme_pixbuf_destroy (data->gap);
      if (data->gap_end)
	theme_pixbuf_destroy (data->gap_end);
      g_free (data);
    }
}

static guint
theme_parse_image(GtkSettings  *settings,
		  GScanner      *scanner,
		  PixbufRcStyle *pixbuf_style,
		  ThemeImage   **data_return)
{
  guint               token;
  ThemeImage *data;

  data = NULL;
  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_IMAGE)
    return TOKEN_IMAGE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  data = g_malloc(sizeof(ThemeImage));

  data->refcount = 1;

  data->background = NULL;
  data->overlay = NULL;
  data->gap_start = NULL;
  data->gap = NULL;
  data->gap_end = NULL;

  data->recolorable = FALSE;

  data->match_data.function = 0;
  data->match_data.detail = NULL;
  data->match_data.flags = 0;

  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_FUNCTION:
	  token = theme_parse_function(scanner, data);
	  break;
	case TOKEN_RECOLORABLE:
	  token = theme_parse_recolorable(scanner, data);
	  break;
	case TOKEN_DETAIL:
	  token = theme_parse_detail(scanner, data);
	  break;
	case TOKEN_STATE:
	  token = theme_parse_state(scanner, data);
	  break;
	case TOKEN_SHADOW:
	  token = theme_parse_shadow(scanner, data);
	  break;
	case TOKEN_GAP_SIDE:
	  token = theme_parse_gap_side(scanner, data);
	  break;
	case TOKEN_ARROW_DIRECTION:
	  token = theme_parse_arrow_direction(scanner, data);
	  break;
	case TOKEN_ORIENTATION:
	  token = theme_parse_orientation(scanner, data);
	  break;
	case TOKEN_FILE:
	  token = theme_parse_file(settings, scanner, &data->background);
	  break;
	case TOKEN_BORDER:
	  token = theme_parse_border(scanner, &data->background);
	  break;
	case TOKEN_STRETCH:
	  token = theme_parse_stretch(scanner, &data->background);
	  break;
	case TOKEN_GAP_FILE:
	  token = theme_parse_file(settings, scanner, &data->gap);
	  break;
	case TOKEN_GAP_BORDER:
	  token = theme_parse_border(scanner, &data->gap);
	  break;
	case TOKEN_GAP_START_FILE:
	  token = theme_parse_file(settings, scanner, &data->gap_start);
	  break;
	case TOKEN_GAP_START_BORDER:
	  token = theme_parse_border(scanner, &data->gap_start);
	  break;
	case TOKEN_GAP_END_FILE:
	  token = theme_parse_file(settings, scanner, &data->gap_end);
	  break;
	case TOKEN_GAP_END_BORDER:
	  token = theme_parse_border(scanner, &data->gap_end);
	  break;
	case TOKEN_OVERLAY_FILE:
	  token = theme_parse_file(settings, scanner, &data->overlay);
	  break;
	case TOKEN_OVERLAY_BORDER:
	  token = theme_parse_border(scanner, &data->overlay);
	  break;
	case TOKEN_OVERLAY_STRETCH:
	  token = theme_parse_stretch(scanner, &data->overlay);
	  break;
	case TOKEN_EXPANDER_STYLE:
	  token = theme_parse_expander_style(scanner, data);
	  break;
	case TOKEN_WINDOW_EDGE:
	  token = theme_parse_window_edge(scanner, data);
	  break;
	default:
	  g_scanner_get_next_token(scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}
      if (token != G_TOKEN_NONE)
	{
	  /* error - cleanup for exit */
	  theme_image_unref (data);
	  *data_return = NULL;
	  return token;
	}
      token = g_scanner_peek_next_token(scanner);
    }

  token = g_scanner_get_next_token(scanner);

  if (data->background && !data->background->filename)
    {
      g_scanner_warn (scanner, "Background image options specified without filename");
      theme_pixbuf_destroy (data->background);
      data->background = NULL;
    }

  if (data->overlay && !data->overlay->filename)
    {
      g_scanner_warn (scanner, "Overlay image options specified without filename");
      theme_pixbuf_destroy (data->overlay);
      data->overlay = NULL;
    }

  if (token != G_TOKEN_RIGHT_CURLY)
    {
      /* error - cleanup for exit */
      theme_image_unref (data);
      *data_return = NULL;
      return G_TOKEN_RIGHT_CURLY;
    }

  /* everything is fine now - insert yer cruft */
  *data_return = data;
  return G_TOKEN_NONE;
}

static guint
pixbuf_rc_style_parse (GtkRcStyle *rc_style,
		       GtkSettings  *settings,
		       GScanner   *scanner)
		     
{
  static GQuark scope_id = 0;
  PixbufRcStyle *pixbuf_style = PIXBUF_RC_STYLE (rc_style);

  guint old_scope;
  guint token;
  gint i;
  ThemeImage *img;
  
  /* Set up a new scope in this scanner. */

  if (!scope_id)
    scope_id = g_quark_from_string("pixbuf_theme_engine");

  /* If we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope(scanner, scope_id);

  /* Now check if we already added our symbols to this scope
   * (in some previous call to theme_parse_rc_style for the
   * same scanner.
   */

  if (!g_scanner_lookup_symbol(scanner, theme_symbols[0].name))
    {
      for (i = 0; i < G_N_ELEMENTS (theme_symbols); i++)
	g_scanner_scope_add_symbol(scanner, scope_id,
				   theme_symbols[i].name,
				   GINT_TO_POINTER(theme_symbols[i].token));
    }

  /* We're ready to go, now parse the top level */

  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_IMAGE:
	  img = NULL;
	  token = theme_parse_image(settings, scanner, pixbuf_style, &img);
	  break;
	default:
	  g_scanner_get_next_token(scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	return token;
      else
	pixbuf_style->img_list = g_list_append(pixbuf_style->img_list, img);

      token = g_scanner_peek_next_token(scanner);
    }

  g_scanner_get_next_token(scanner);

  g_scanner_set_scope(scanner, old_scope);

  return G_TOKEN_NONE;
}

static void
pixbuf_rc_style_merge (GtkRcStyle *dest,
		       GtkRcStyle *src)
{
  if (PIXBUF_IS_RC_STYLE (src))
    {
      PixbufRcStyle *pixbuf_dest = PIXBUF_RC_STYLE (dest);
      PixbufRcStyle *pixbuf_src = PIXBUF_RC_STYLE (src);
      GList *tmp_list1, *tmp_list2;
      
      if (pixbuf_src->img_list)
	{
	  /* Copy src image list and append to dest image list */
	  
	  tmp_list2 = g_list_last (pixbuf_dest->img_list);
	  tmp_list1 = pixbuf_src->img_list;
	  
	  while (tmp_list1)
	    {
	      if (tmp_list2)
		{
		  tmp_list2->next = g_list_alloc();
		  tmp_list2->next->data = tmp_list1->data;
		  tmp_list2->next->prev = tmp_list2;
		  
		  tmp_list2 = tmp_list2->next;
		}
	      else
		{
		  pixbuf_dest->img_list = g_list_append (NULL, tmp_list1->data);
		  tmp_list2 = pixbuf_dest->img_list;
		}
	      
	      theme_image_ref (tmp_list1->data);
	      tmp_list1 = tmp_list1->next;
	    }
	}
    }

  parent_class->merge (dest, src);
}

/* Create an empty style suitable to this RC style
 */
static GtkStyle *
pixbuf_rc_style_create_style (GtkRcStyle *rc_style)
{
  return g_object_new (PIXBUF_TYPE_STYLE, NULL);
}

