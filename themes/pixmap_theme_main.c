#include "pixmap_theme.h"

/* Theme functions to export */
void                theme_init(GtkThemeEngine * engine);
void                theme_exit(void);

/* Exported vtable from th_draw */

extern GtkStyleClass th_default_class;

/* external theme functions called */

static struct
  {
    gchar              *name;
    guint               token;
  }
theme_symbols[] =
{
  {
    "image", TOKEN_IMAGE
  }
  ,
  {
    "function", TOKEN_FUNCTION
  }
  ,
  {
    "file", TOKEN_FILE
  }
  ,
  {
    "stretch", TOKEN_STRETCH
  }
  ,
  {
    "recolorable", TOKEN_RECOLORABLE
  }
  ,
  {
    "border", TOKEN_BORDER
  }
  ,
  {
    "detail", TOKEN_DETAIL
  }
  ,
  {
    "state", TOKEN_STATE
  }
  ,
  {
    "shadow", TOKEN_SHADOW
  }
  ,
  {
    "gap_side", TOKEN_GAP_SIDE
  }
  ,
  {
    "gap_file", TOKEN_GAP_FILE
  }
  ,
  {
    "gap_border", TOKEN_GAP_BORDER
  }
  ,
  {
    "gap_start_file", TOKEN_GAP_START_FILE
  }
  ,
  {
    "gap_start_border", TOKEN_GAP_START_BORDER
  }
  ,
  {
    "gap_end_file", TOKEN_GAP_END_FILE
  }
  ,
  {
    "gap_end_border", TOKEN_GAP_END_BORDER
  }
  ,
  {
    "overlay_file", TOKEN_OVERLAY_FILE
  }
  ,
  {
    "overlay_border", TOKEN_OVERLAY_BORDER
  }
  ,
  {
    "overlay_stretch", TOKEN_OVERLAY_STRETCH
  }
  ,
  {
    "arrow_direction", TOKEN_ARROW_DIRECTION
  }
  ,
  {
    "orientation", TOKEN_ORIENTATION
  }
  ,

  {
    "HLINE", TOKEN_D_HLINE
  }
  ,
  {
    "VLINE", TOKEN_D_VLINE
  }
  ,
  {
    "SHADOW", TOKEN_D_SHADOW
  }
  ,
  {
    "POLYGON", TOKEN_D_POLYGON
  }
  ,
  {
    "ARROW", TOKEN_D_ARROW
  }
  ,
  {
    "DIAMOND", TOKEN_D_DIAMOND
  }
  ,
  {
    "OVAL", TOKEN_D_OVAL
  }
  ,
  {
    "STRING", TOKEN_D_STRING
  }
  ,
  {
    "BOX", TOKEN_D_BOX
  }
  ,
  {
    "FLAT_BOX", TOKEN_D_FLAT_BOX
  }
  ,
  {
    "CHECK", TOKEN_D_CHECK
  }
  ,
  {
    "OPTION", TOKEN_D_OPTION
  }
  ,
  {
    "CROSS", TOKEN_D_CROSS
  }
  ,
  {
    "RAMP", TOKEN_D_RAMP
  }
  ,
  {
    "TAB", TOKEN_D_TAB
  }
  ,
  {
    "SHADOW_GAP", TOKEN_D_SHADOW_GAP
  }
  ,
  {
    "BOX_GAP", TOKEN_D_BOX_GAP
  }
  ,
  {
    "EXTENSION", TOKEN_D_EXTENSION
  }
  ,
  {
    "FOCUS", TOKEN_D_FOCUS
  }
  ,
  {
    "SLIDER", TOKEN_D_SLIDER
  }
  ,
  {
    "ENTRY", TOKEN_D_ENTRY
  }
  ,
  {
    "HANDLE", TOKEN_D_HANDLE
  }
  ,

  {
    "TRUE", TOKEN_TRUE
  }
  ,
  {
    "FALSE", TOKEN_FALSE
  }
  ,

  {
    "TOP", TOKEN_TOP
  }
  ,
  {
    "UP", TOKEN_UP
  }
  ,
  {
    "BOTTOM", TOKEN_BOTTOM
  }
  ,
  {
    "DOWN", TOKEN_DOWN
  }
  ,
  {
    "LEFT", TOKEN_LEFT
  }
  ,
  {
    "RIGHT", TOKEN_RIGHT
  }
  ,

  {
    "NORMAL", TOKEN_NORMAL
  }
  ,
  {
    "ACTIVE", TOKEN_ACTIVE
  }
  ,
  {
    "PRELIGHT", TOKEN_PRELIGHT
  }
  ,
  {
    "SELECTED", TOKEN_SELECTED
  }
  ,
  {
    "INSENSITIVE", TOKEN_INSENSITIVE
  }
  ,

  {
    "NONE", TOKEN_NONE
  }
  ,
  {
    "IN", TOKEN_IN
  }
  ,
  {
    "OUT", TOKEN_OUT
  }
  ,
  {
    "ETCHED_IN", TOKEN_ETCHED_IN
  }
  ,
  {
    "ETCHED_OUT", TOKEN_ETCHED_OUT
  }
  ,
  {
    "HORIZONTAL", TOKEN_HORIZONTAL
  }
  ,
  {
    "VERTICAL", TOKEN_VERTICAL
  }
  ,
};

static guint        n_theme_symbols = sizeof(theme_symbols) / sizeof(theme_symbols[0]);

guint
theme_parse_function(GScanner * scanner,
		     struct theme_image *data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_FUNCTION)
    return TOKEN_FUNCTION;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if ((token >= TOKEN_D_HLINE) && (token <= TOKEN_D_HANDLE))
    data->function = token;

  return G_TOKEN_NONE;
}

guint
theme_parse_file(GScanner * scanner,
		 struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_FILE)
    return TOKEN_FILE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  data->file = gtk_rc_find_pixmap_in_path(scanner, scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_recolorable(GScanner * scanner,
			struct theme_image * data)
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

guint
theme_parse_border(GScanner * scanner,
		   GdkImlibBorder * border)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_BORDER)
    return TOKEN_BORDER;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  return G_TOKEN_NONE;
}

guint
theme_parse_detail(GScanner * scanner,
		   struct theme_image * data)
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

  data->detail = g_strdup(scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_state(GScanner * scanner,
		  struct theme_image * data)
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
    data->state = GTK_STATE_NORMAL;
  else if (token == TOKEN_ACTIVE)
    data->state = GTK_STATE_ACTIVE;
  else if (token == TOKEN_PRELIGHT)
    data->state = GTK_STATE_PRELIGHT;
  else if (token == TOKEN_SELECTED)
    data->state = GTK_STATE_SELECTED;
  else if (token == TOKEN_INSENSITIVE)
    data->state = GTK_STATE_INSENSITIVE;
  else
    return TOKEN_NORMAL;

  data->__state = 1;
  return G_TOKEN_NONE;
}

guint
theme_parse_shadow(GScanner * scanner,
		   struct theme_image * data)
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
    data->shadow = GTK_SHADOW_NONE;
  else if (token == TOKEN_IN)
    data->shadow = GTK_SHADOW_IN;
  else if (token == TOKEN_OUT)
    data->shadow = GTK_SHADOW_OUT;
  else if (token == TOKEN_ETCHED_IN)
    data->shadow = GTK_SHADOW_ETCHED_IN;
  else if (token == TOKEN_ETCHED_OUT)
    data->shadow = GTK_SHADOW_ETCHED_OUT;
  else
    return TOKEN_NONE;

  data->__shadow = 1;
  return G_TOKEN_NONE;
}

guint
theme_parse_stretch(GScanner * scanner,
		    struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_STRETCH)
    return TOKEN_STRETCH;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_TRUE)
    data->stretch = 1;
  else if (token == TOKEN_FALSE)
    data->stretch = 0;
  else
    return TOKEN_TRUE;

  return G_TOKEN_NONE;
}

guint
theme_parse_overlay_stretch(GScanner * scanner,
			    struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_OVERLAY_STRETCH)
    return TOKEN_OVERLAY_STRETCH;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token == TOKEN_TRUE)
    data->overlay_stretch = 1;
  else if (token == TOKEN_FALSE)
    data->overlay_stretch = 0;
  else
    return TOKEN_TRUE;

  return G_TOKEN_NONE;
}

guint
theme_parse_overlay_border(GScanner * scanner,
			   GdkImlibBorder * border)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_OVERLAY_BORDER)
    return TOKEN_OVERLAY_BORDER;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_border(GScanner * scanner,
		       GdkImlibBorder * border)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_BORDER)
    return TOKEN_GAP_BORDER;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_start_border(GScanner * scanner,
			     GdkImlibBorder * border)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_START_BORDER)
    return TOKEN_GAP_START_BORDER;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_end_border(GScanner * scanner,
			   GdkImlibBorder * border)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_END_BORDER)
    return TOKEN_GAP_END_BORDER;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->left = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->right = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->top = scanner->value.v_int;
  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_COMMA)
    return G_TOKEN_COMMA;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_INT)
    return G_TOKEN_INT;
  border->bottom = scanner->value.v_int;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_RIGHT_CURLY)
    return G_TOKEN_RIGHT_CURLY;

  return G_TOKEN_NONE;
}

guint
theme_parse_overlay_file(GScanner * scanner,
			 struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_OVERLAY_FILE)
    return TOKEN_OVERLAY_FILE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  data->overlay_file = gtk_rc_find_pixmap_in_path(scanner, scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_file(GScanner * scanner,
		     struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_FILE)
    return TOKEN_GAP_FILE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  data->gap_file = gtk_rc_find_pixmap_in_path(scanner, scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_start_file(GScanner * scanner,
			   struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_START_FILE)
    return TOKEN_GAP_START_FILE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  data->gap_start_file = gtk_rc_find_pixmap_in_path(scanner, scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_gap_end_file(GScanner * scanner,
			 struct theme_image * data)
{
  guint               token;

  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_GAP_END_FILE)
    return TOKEN_GAP_END_FILE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
  data->gap_end_file = gtk_rc_find_pixmap_in_path(scanner, scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint
theme_parse_arrow_direction(GScanner * scanner,
			    struct theme_image * data)
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
    data->arrow_direction = GTK_ARROW_UP;
  else if (token == TOKEN_DOWN)
    data->arrow_direction = GTK_ARROW_DOWN;
  else if (token == TOKEN_LEFT)
    data->arrow_direction = GTK_ARROW_LEFT;
  else if (token == TOKEN_RIGHT)
    data->arrow_direction = GTK_ARROW_RIGHT;
  else
    return TOKEN_UP;

  data->__arrow_direction = 1;
  return G_TOKEN_NONE;
}

guint
theme_parse_gap_side(GScanner * scanner,
		     struct theme_image * data)
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
    data->gap_side = 0;
  else if (token == TOKEN_BOTTOM)
    data->gap_side = 1;
  else if (token == TOKEN_LEFT)
    data->gap_side = 2;
  else if (token == TOKEN_RIGHT)
    data->gap_side = 3;
  else
    return TOKEN_TOP;

  data->__gap_side = 1;
  return G_TOKEN_NONE;
}

guint
theme_parse_orientation(GScanner * scanner,
			struct theme_image * data)
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
    data->orientation = GTK_ORIENTATION_HORIZONTAL;
  else if (token == TOKEN_VERTICAL)
    data->orientation = GTK_ORIENTATION_VERTICAL;
  else
    return TOKEN_HORIZONTAL;

  data->__orientation = 1;
  return G_TOKEN_NONE;
}

void
theme_image_ref (struct theme_image *data)
{
  data->refcount++;
}

void
theme_image_unref (struct theme_image *data)
{
  data->refcount--;
  if (data->refcount == 0)
    {
      if (data->detail)
	g_free (data->detail);
      if (data->file)
	g_free (data->file);
      if (data->overlay_file)
	g_free (data->overlay_file);
      if (data->gap_file)
	g_free (data->gap_file);
      g_free (data);
    }
}

void
theme_data_ref (ThemeData *theme_data)
{
  theme_data->refcount++;
}

void
theme_data_unref (ThemeData *theme_data)
{
  theme_data->refcount--;
  if (theme_data->refcount == 0)
    {
      g_list_foreach (theme_data->img_list, (GFunc) theme_image_unref, NULL);
      g_list_free (theme_data->img_list);
    }
}

guint
theme_parse_image(GScanner *scanner,
		  ThemeData *theme_data,
		  struct theme_image **data_return)
{
  guint               token;
  struct theme_image *data;

  data = NULL;
  token = g_scanner_get_next_token(scanner);
  if (token != TOKEN_IMAGE)
    return TOKEN_IMAGE;

  token = g_scanner_get_next_token(scanner);
  if (token != G_TOKEN_LEFT_CURLY)
    return G_TOKEN_LEFT_CURLY;

  data = g_malloc(sizeof(struct theme_image));

  data->refcount = 1;

  data->function = -1;
  data->file = NULL;
  data->recolorable = 1;
  data->border.left = 0;
  data->border.right = 0;
  data->border.top = 0;
  data->border.bottom = 0;
  data->detail = NULL;
  data->state = GTK_STATE_NORMAL;
  data->shadow = GTK_SHADOW_NONE;
  data->stretch = 1;
  data->gap_side = 0;
  data->gap_border.left = 0;
  data->gap_border.right = 0;
  data->gap_border.top = 0;
  data->gap_border.bottom = 0;
  data->gap_file = NULL;
  data->gap_start_border.left = 0;
  data->gap_start_border.right = 0;
  data->gap_start_border.top = 0;
  data->gap_start_border.bottom = 0;
  data->gap_start_file = NULL;
  data->gap_end_border.left = 0;
  data->gap_end_border.right = 0;
  data->gap_end_border.top = 0;
  data->gap_end_border.bottom = 0;
  data->gap_end_file = NULL;
  data->overlay_border.left = 0;
  data->overlay_border.right = 0;
  data->overlay_border.top = 0;
  data->overlay_border.bottom = 0;
  data->overlay_file = NULL;
  data->overlay_stretch = 0;
  data->arrow_direction = GTK_ARROW_UP;
  data->orientation = GTK_ORIENTATION_HORIZONTAL;
  data->__gap_side = 0;
  data->__orientation = 0;
  data->__state = 0;
  data->__shadow = 0;
  data->__arrow_direction = 0;
  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_FUNCTION:
	  token = theme_parse_function(scanner, data);
	  break;
	case TOKEN_FILE:
	  token = theme_parse_file(scanner, data);
	  break;
	case TOKEN_RECOLORABLE:
	  token = theme_parse_recolorable(scanner, data);
	  break;
	case TOKEN_BORDER:
	  token = theme_parse_border(scanner, &data->border);
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
	case TOKEN_GAP_FILE:
	  token = theme_parse_gap_file(scanner, data);
	  break;
	case TOKEN_GAP_BORDER:
	  token = theme_parse_gap_border(scanner, &data->gap_border);
	  break;
	case TOKEN_GAP_START_FILE:
	  token = theme_parse_gap_start_file(scanner, data);
	  break;
	case TOKEN_GAP_START_BORDER:
	  token = theme_parse_gap_start_border(scanner, &data->gap_start_border);
	  break;
	case TOKEN_GAP_END_FILE:
	  token = theme_parse_gap_end_file(scanner, data);
	  break;
	case TOKEN_GAP_END_BORDER:
	  token = theme_parse_gap_end_border(scanner, &data->gap_end_border);
	  break;
	case TOKEN_OVERLAY_FILE:
	  token = theme_parse_overlay_file(scanner, data);
	  break;
	case TOKEN_OVERLAY_BORDER:
	  token = theme_parse_overlay_border(scanner, &data->overlay_border);
	  break;
	case TOKEN_OVERLAY_STRETCH:
	  token = theme_parse_overlay_stretch(scanner, data);
	  break;
	case TOKEN_STRETCH:
	  token = theme_parse_stretch(scanner, data);
	  break;
	case TOKEN_ARROW_DIRECTION:
	  token = theme_parse_arrow_direction(scanner, data);
	  break;
	case TOKEN_ORIENTATION:
	  token = theme_parse_orientation(scanner, data);
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

guint
theme_parse_rc_style(GScanner * scanner,
		     GtkRcStyle * rc_style)
{
  static GQuark       scope_id = 0;
  ThemeData        *theme_data;
  guint               old_scope;
  guint               token;
  gint                i;
  struct theme_image *img;
  
  /* Set up a new scope in this scanner. */

  if (!scope_id)
    scope_id = g_quark_from_string("theme_engine");

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
      g_scanner_freeze_symbol_table(scanner);
      for (i = 0; i < n_theme_symbols; i++)
	g_scanner_scope_add_symbol(scanner, scope_id,
				   theme_symbols[i].name,
				   GINT_TO_POINTER(theme_symbols[i].token));
      g_scanner_thaw_symbol_table(scanner);
    }

  /* We're ready to go, now parse the top level */

  theme_data = g_new(ThemeData, 1);
  theme_data->img_list = NULL;
  theme_data->refcount = 1;

  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_IMAGE:
	  img = NULL;
	  token = theme_parse_image(scanner, theme_data, &img);
	  break;
	default:
	  g_scanner_get_next_token(scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  g_list_foreach (theme_data->img_list, (GFunc)theme_image_unref, NULL);
	  g_list_free (theme_data->img_list);
	  g_free (theme_data);
	  return token;
	}
      else
	{
	  theme_data->img_list = g_list_append(theme_data->img_list, img);
	}
      token = g_scanner_peek_next_token(scanner);
    }

  g_scanner_get_next_token(scanner);

  rc_style->engine_data = theme_data;
  g_scanner_set_scope(scanner, old_scope);

  return G_TOKEN_NONE;
}

void
theme_merge_rc_style(GtkRcStyle * dest,
		     GtkRcStyle * src)
{
  GList *dest_images;
  
  ThemeData        *src_data = src->engine_data;
  ThemeData        *dest_data = dest->engine_data;

  if (!dest_data)
    {
      dest_data = g_new(ThemeData, 1);
      dest_data->img_list = NULL;
      dest_data->refcount = 1;
      dest->engine_data = dest_data;
    }

  if (src_data->img_list)
    {
      GList *tmp_list1, *tmp_list2;
      GList *dest_images = dest_data->img_list;
      
      /* Copy src image list and prepend to dest image list */

      dest_data->img_list = g_list_append (NULL, src_data->img_list->data);
      theme_data_ref (src_data->img_list->data);
      tmp_list2 = dest_data->img_list;
      tmp_list1 = src_data->img_list->next;
      
      while (tmp_list1)
	{
 	  tmp_list2->next = g_list_alloc();
	  tmp_list2->next->data = tmp_list1->data;
	  theme_data_ref (tmp_list1->data);
	  tmp_list2->next->prev = tmp_list2;

	  tmp_list2 = tmp_list2->next;
	  tmp_list1 = tmp_list1->next;
	}

      tmp_list2->next = dest_images;
    }
}

void
theme_rc_style_to_style(GtkStyle * style,
			GtkRcStyle * rc_style)
{
  ThemeData        *data = rc_style->engine_data;

  style->klass = &th_default_class;
  style->engine_data = data;
  theme_data_ref (data);

  g_print("Theme theme: Creating style\n");
}

void
theme_duplicate_style(GtkStyle * dest,
		      GtkStyle * src)
{
  ThemeData     *src_data = src->engine_data;
  ThemeData     *dest_data;

  dest_data = g_new(ThemeData, 1);
  dest_data->img_list = src_data->img_list;

  dest->klass = &th_default_class;
  dest->engine_data = dest_data;
  theme_data_ref (dest_data);

  g_print("Theme theme: Duplicated style\n");
}

void
theme_realize_style(GtkStyle * style)
{
/*  ThemeStyleData     *data = style->engine_data;*/

  g_print("Theme theme: Realizing style\n");
}

void
theme_unrealize_style(GtkStyle * style)
{
/*  ThemeStyleData     *data = style->engine_data;*/

  g_print("Theme theme: Unrealizing style\n");
}

void
theme_destroy_rc_style(GtkRcStyle * rc_style)
{
  theme_data_unref (rc_style->engine_data);

  g_print("Theme theme: Destroying rc style for \n");
}

void
theme_destroy_style(GtkStyle * style)
{
  theme_data_unref (style->engine_data);

  g_print("Theme theme: Destroying style for \n");
}

void
theme_set_background(GtkStyle * style,
		     GdkWindow * window,
		     GtkStateType state_type)
{
  GdkPixmap          *pixmap;
  gint                parent_relative;

  g_return_if_fail(style != NULL);
  g_return_if_fail(window != NULL);

  if (style->bg_pixmap[state_type])
    {
      if (style->bg_pixmap[state_type] == (GdkPixmap *) GDK_PARENT_RELATIVE)
	{
	  pixmap = NULL;
	  parent_relative = TRUE;
	}
      else
	{
	  pixmap = style->bg_pixmap[state_type];
	  parent_relative = FALSE;
	}

      gdk_window_set_back_pixmap(window, pixmap, parent_relative);
    }
  else
    gdk_window_set_background(window, &style->bg[state_type]);
}

void
theme_init(GtkThemeEngine * engine)
{
  engine->parse_rc_style = theme_parse_rc_style;
  engine->merge_rc_style = theme_merge_rc_style;
  engine->rc_style_to_style = theme_rc_style_to_style;
  engine->duplicate_style = theme_duplicate_style;
  engine->realize_style = theme_realize_style;
  engine->unrealize_style = theme_unrealize_style;
  engine->destroy_rc_style = theme_destroy_rc_style;
  engine->destroy_style = theme_destroy_style;
  engine->set_background = theme_set_background;

  gdk_imlib_init();
}

void
theme_exit(void)
{
}
