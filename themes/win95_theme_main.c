#include "win95_theme.h"

ThemeConfig *theme_config = NULL;

/* Theme functions to export */
void theme_init               (GtkThemeEngine *engine);
void theme_exit               (void);

/* Exported vtable from th_draw */

extern GtkStyleClass th_default_class;

/* internals */

/* external theme functions called */

typedef struct {
  gchar *name;
} ThemeRcData;

typedef struct {
  gchar *name;
} ThemeStyleData;

enum {
  TOKEN_IMAGE = G_TOKEN_LAST + 1,
  TOKEN_FUNCTION,
  TOKEN_FILE,
  TOKEN_RECOLORABLE,
  TOKEN_BORDER,
  TOKEN_DETAIL,
  TOKEN_STATE,
  TOKEN_SHADOW,
   TOKEN_D_HLINE,
   TOKEN_D_VLINE,
   TOKEN_D_SHADOW,
   TOKEN_D_POLYGON,
   TOKEN_D_ARROW,
   TOKEN_D_DIAMOND,
   TOKEN_D_OVAL,
   TOKEN_D_STRING,
   TOKEN_D_BOX,
   TOKEN_D_FLAT_BOX,
   TOKEN_D_CHECK,
   TOKEN_D_OPTION,
   TOKEN_D_CROSS,
   TOKEN_D_RAMP,
   TOKEN_D_TAB,
   TOKEN_D_SHADOW_GAP,
   TOKEN_D_BOX_GAP,
   TOKEN_D_EXTENSION,
   TOKEN_D_FOCUS,
   TOKEN_D_SLIDER,
   TOKEN_D_ENTRY,
   TOKEN_D_HANDLE,
   TOKEN_TRUE,
   TOKEN_FALSE,
   TOKEN_TOP,
   TOKEN_UP,
   TOKEN_BOTTOM,
   TOKEN_DOWN,
   TOKEN_LEFT,
   TOKEN_RIGHT,
};

static struct
{
   gchar *name;
   guint token;
} theme_symbols[] = {
     { "image", TOKEN_IMAGE },
     { "function", TOKEN_FUNCTION },
     { "file", TOKEN_FILE },
     { "recolorable", TOKEN_RECOLORABLE },
     { "border", TOKEN_BORDER },
     { "detail", TOKEN_DETAIL },
     { "state", TOKEN_STATE },
     { "shadow", TOKEN_SHADOW },
   
     { "HILNE", TOKEN_D_HLINE },
     { "VLINE", TOKEN_D_VLINE },
     { "SHADOW", TOKEN_D_SHADOW },
     { "POLYGON", TOKEN_D_POLYGON },
     { "ARROW", TOKEN_D_ARROW },
     { "DIAMOND", TOKEN_D_DIAMOND },
     { "OVAL", TOKEN_D_OVAL },
     { "STRING", TOKEN_D_STRING },
     { "BOX", TOKEN_D_BOX },
     { "FLAT_BOX", TOKEN_D_FLAT_BOX },
     { "CHECK", TOKEN_D_CHECK },
     { "OPTION", TOKEN_D_OPTION },
     { "CROSS", TOKEN_D_CROSS },
     { "RAMP", TOKEN_D_RAMP },
     { "TAB", TOKEN_D_TAB },
     { "SHADOW_GAP", TOKEN_D_SHADOW_GAP },
     { "BOX_GAP", TOKEN_D_BOX_GAP },
     { "EXTENSION", TOKEN_D_EXTENSION },
     { "FOCUS", TOKEN_D_FOCUS },
     { "SLIDER", TOKEN_D_SLIDER },
     { "ENTRY", TOKEN_D_ENTRY },
     { "HANDLE", TOKEN_D_HANDLE },

     { "TRUE", TOKEN_TRUE },
     { "FALSE", TOKEN_FALSE },

     { "TOP", TOKEN_TOP },
     { "UP", TOKEN_UP },
     { "BOTTOM", TOKEN_BOTTOM },
     { "DOWN", TOKEN_DOWN },
     { "LEFT", TOKEN_LEFT },
     { "RIGHT", TOKEN_RIGHT },
};

static guint n_theme_symbols = sizeof (theme_symbols) / sizeof (theme_symbols[0]);

struct theme_image
{
   guint             function;
   gchar            *file;
   gchar             recolorable;
   GdkImlibBorder    border;
   gchar            *detail;
   GtkStateType      state;
   GtkShadowType     shadow;
};

guint 
theme_parse_function (GScanner   *scanner,
		      struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_FUNCTION)
    return TOKEN_FUNCTION;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_file (GScanner   *scanner,
		  struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_FILE)
    return TOKEN_FILE;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
   data->file = g_strdup (scanner->value.v_string);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_recolorable (GScanner   *scanner,
			 struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_RECOLORABLE)
    return TOKEN_RECOLORABLE;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
   
   data->file = g_strdup (scanner->value.v_string);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_border (GScanner   *scanner,
		    GdkImlibBorder *border)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_BORDER)
     return TOKEN_BORDER;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
     return G_TOKEN_EQUAL_SIGN;
   
   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_LEFT_CURLY)
     return G_TOKEN_LEFT_CURLY;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_INT)
     return G_TOKEN_INT;
   border->left = scanner->value.v_int;
   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_COMMA)
     return G_TOKEN_COMMA;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_INT)
     return G_TOKEN_INT;
   border->right = scanner->value.v_int;
   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_COMMA)
     return G_TOKEN_COMMA;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_INT)
     return G_TOKEN_INT;
   border->top = scanner->value.v_int;
   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_COMMA)
     return G_TOKEN_COMMA;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_INT)
     return G_TOKEN_INT;
   border->bottom = scanner->value.v_int;
   
   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_RIGHT_CURLY)
     return G_TOKEN_RIGHT_CURLY;
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_detail (GScanner   *scanner,
		  struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_DETAIL)
    return TOKEN_DETAIL;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
   
   data->file = g_strdup (scanner->value.v_string);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_state (GScanner   *scanner,
		   struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_STATE)
    return TOKEN_STATE;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
   
   data->file = g_strdup (scanner->value.v_string);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_shadow (GScanner   *scanner,
		    struct theme_image *data)
{
   guint token;
   
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_SHADOW)
    return TOKEN_SHADOW;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;
   
   data->file = g_strdup (scanner->value.v_string);
   
   return G_TOKEN_NONE;
}

guint 
theme_parse_image (GScanner   *scanner,
		  ThemeRcData *theme_data)
{
   guint token;
   struct theme_image *data;
   
   data = NULL;
   token = g_scanner_get_next_token (scanner);
   if (token != TOKEN_IMAGE)
     return TOKEN_IMAGE;

   token = g_scanner_get_next_token (scanner);
   if (token != G_TOKEN_LEFT_CURLY)
     return G_TOKEN_LEFT_CURLY;

   data = g_malloc(sizeof(struct theme_image));
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
   
   token = g_scanner_peek_next_token (scanner);
   while (token != G_TOKEN_RIGHT_CURLY)
     {
	switch (token)
	  {
	   case TOKEN_FUNCTION:
	     token = theme_parse_function (scanner, data);
	     printf("func\n");
	     break;
	   case TOKEN_FILE:
	     token = theme_parse_file (scanner, data);
	     break;
	   case TOKEN_RECOLORABLE:
	     token = theme_parse_recolorable (scanner, data);
	     break;
	   case TOKEN_BORDER:
	     token = theme_parse_border (scanner, &data->border);
	     break;
	   case TOKEN_DETAIL:
	     token = theme_parse_detail (scanner, data);
	     break;
	   case TOKEN_STATE:
	     token = theme_parse_state (scanner, data);
	     break;
	   case TOKEN_SHADOW:
	     token = theme_parse_shadow (scanner, data);
	     break;
	   default:
	     g_scanner_get_next_token (scanner);
	     token = G_TOKEN_RIGHT_CURLY;
	     break;
	  }
	if (token != G_TOKEN_NONE)
	  {
	     /* error - cleanup for exit */
	     return token;
	  }
	token = g_scanner_peek_next_token (scanner);
     }
   
   token = g_scanner_get_next_token (scanner);
   
   if (token != G_TOKEN_RIGHT_CURLY)
     {
	/* error - cleanup for exit */
	return G_TOKEN_RIGHT_CURLY;
     }
   
   /* everything is fine now - insert yer cruft */
   return G_TOKEN_NONE;
}

guint 
theme_parse_rc_style    (GScanner   *scanner, 
			  GtkRcStyle *rc_style)
{
  static GQuark scope_id = 0;
  ThemeRcData *theme_data;
  guint old_scope;
  guint token;
  gint i;

  /* Set up a new scope in this scanner. */

  if (!scope_id)
    scope_id = g_quark_from_string ("theme_engine");
  
  /* If we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, scope_id);

  /* Now check if we already added our symbols to this scope
   * (in some previous call to theme_parse_rc_style for the
   * same scanner.
   */
  
  if (!g_scanner_lookup_symbol (scanner, theme_symbols[0].name))
    {
      g_scanner_freeze_symbol_table (scanner);
      for (i = 0; i < n_theme_symbols; i++)
	g_scanner_scope_add_symbol (scanner, scope_id, 
				    theme_symbols[i].name, 
				    GINT_TO_POINTER (theme_symbols[i].token));
      g_scanner_thaw_symbol_table (scanner);
    }

  /* We're ready to go, now parse the top level */

  theme_data = g_new (ThemeRcData, 1);
  theme_data->name = NULL;
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_IMAGE:
	  token = theme_parse_image (scanner, theme_data);
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  g_free (theme_data);
	  return token;
	}
      token = g_scanner_peek_next_token (scanner);
    }

  g_scanner_get_next_token (scanner);

  rc_style->engine_data = theme_data;
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

void  
theme_merge_rc_style    (GtkRcStyle *dest,     
			  GtkRcStyle *src)
{
  ThemeRcData *src_data = src->engine_data;
  ThemeRcData *dest_data = dest->engine_data;

  if (!dest_data)
    {
      dest_data = g_new (ThemeRcData, 1);
      dest_data->name = NULL;
      dest->engine_data = dest_data;
    }

  if (dest_data->name == NULL)
    dest_data->name = g_strdup (src_data->name);
}

void  
theme_rc_style_to_style (GtkStyle   *style,    
			  GtkRcStyle *rc_style)
{
  ThemeRcData *data = rc_style->engine_data;
  ThemeStyleData *style_data;

  style_data = g_new (ThemeStyleData, 1);
  style_data->name = g_strdup (data->name);

  style->klass = &th_default_class;
  style->engine_data = style_data;

  g_print ("Theme theme: Creating style for \"%s\"\n", data->name);
}

void  
theme_duplicate_style   (GtkStyle   *dest,       
			  GtkStyle   *src)
{
  ThemeStyleData *dest_data;
  ThemeStyleData *src_data = src->engine_data;

  dest_data = g_new (ThemeStyleData, 1);
  dest_data->name = g_strdup (src_data->name);

  dest->engine_data = dest_data;
  
  g_print ("Theme theme: Duplicated style for \"%s\"\n", src_data->name);
}

void  
theme_realize_style     (GtkStyle   *style)
{
  ThemeStyleData *data = style->engine_data;

  g_print ("Theme theme: Realizing style for \"%s\"\n", data->name);
}

void  
theme_unrealize_style     (GtkStyle   *style)
{
  ThemeStyleData *data = style->engine_data;

  g_print ("Theme theme: Unrealizing style for \"%s\"\n", data->name);
}

void  
theme_destroy_rc_style  (GtkRcStyle *rc_style)
{
  ThemeRcData *data = rc_style->engine_data;

  if (data)
    {
      g_free (data->name);
      g_free (data);
    }

  g_print ("Theme theme: Destroying rc style for \"%s\"\n", data->name);
}

void  
theme_destroy_style     (GtkStyle   *style)
{
  ThemeStyleData *data = style->engine_data;

  if (data)
    {
      g_free (data->name);
      g_free (data);
    }

  g_print ("Theme theme: Destroying style for \"%s\"\n", data->name);
}

void
theme_set_background (GtkStyle     *style,
		      GdkWindow    *window,
		      GtkStateType  state_type)
{
  GdkPixmap *pixmap;
  gint parent_relative;
  
  g_return_if_fail (style != NULL);
  g_return_if_fail (window != NULL);
  
  if (style->bg_pixmap[state_type])
    {
      if (style->bg_pixmap[state_type] == (GdkPixmap*) GDK_PARENT_RELATIVE)
	{
	  pixmap = NULL;
	  parent_relative = TRUE;
	}
      else
	{
	  pixmap = style->bg_pixmap[state_type];
	  parent_relative = FALSE;
	}
      
      gdk_window_set_back_pixmap (window, pixmap, parent_relative);
    }
  else
    gdk_window_set_background (window, &style->bg[state_type]);
}

void 
theme_init (GtkThemeEngine *engine)
{
  
   printf("Theme2 Init\n");

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

   /* theme_config = th_dat.data = malloc(sizeof(ThemeConfig)); */
   /* theme_read_config(); */
}

void 
theme_exit (void)
{
   printf("Theme2 Exit\n* Need to add memory deallocation code here *\n");
}

