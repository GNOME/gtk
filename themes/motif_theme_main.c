#include <gtk/gtk.h>

/* Theme functions to export */
void                theme_init(GtkThemeEngine * engine);
void                theme_exit(void);

/* Exported vtable from th_draw */

extern GtkStyleClass th_default_class;

/* internals */

/* external theme functions called */

typedef struct
  {
    gchar              *name;
  }
ThemeRcData;

typedef struct
  {
    gchar              *name;
  }
ThemeStyleData;

guint
theme_parse_rc_style(GScanner * scanner,
		     GtkRcStyle * rc_style)
{
  static GQuark       scope_id = 0;
  ThemeRcData        *theme_data;
  guint               old_scope;
  guint               token;

  /* Set up a new scope in this scanner. */

  if (!scope_id)
    scope_id = g_quark_from_string("theme_engine");

  /* If we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope(scanner, scope_id);

  /* We're ready to go, now parse the top level */

  theme_data = g_new(ThemeRcData, 1);
  theme_data->name = NULL;

  token = g_scanner_peek_next_token(scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	default:
	  g_scanner_get_next_token(scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  g_free(theme_data);
	  return token;
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
  ThemeRcData        *src_data = src->engine_data;
  ThemeRcData        *dest_data = dest->engine_data;

  if (!dest_data)
    {
      dest_data = g_new(ThemeRcData, 1);
      dest_data->name = NULL;
      dest->engine_data = dest_data;
    }

  if (dest_data->name == NULL)
    dest_data->name = g_strdup(src_data->name);
}

void
theme_rc_style_to_style(GtkStyle * style,
			GtkRcStyle * rc_style)
{
  ThemeRcData        *data = rc_style->engine_data;
  ThemeStyleData     *style_data;

  style_data = g_new(ThemeStyleData, 1);
  style_data->name = g_strdup(data->name);

  style->klass = &th_default_class;
  style->engine_data = style_data;

  g_print("Theme theme: Creating style for \"%s\"\n", data->name);
}

void
theme_duplicate_style(GtkStyle * dest,
		      GtkStyle * src)
{
  ThemeStyleData     *dest_data;
  ThemeStyleData     *src_data = src->engine_data;

  dest_data = g_new(ThemeStyleData, 1);
  dest_data->name = g_strdup(src_data->name);

  dest->engine_data = dest_data;

  g_print("Theme theme: Duplicated style for \"%s\"\n", src_data->name);
}

void
theme_realize_style(GtkStyle * style)
{
  ThemeStyleData     *data = style->engine_data;

  g_print("Theme theme: Realizing style for \"%s\"\n", data->name);
}

void
theme_unrealize_style(GtkStyle * style)
{
  ThemeStyleData     *data = style->engine_data;

  g_print("Theme theme: Unrealizing style for \"%s\"\n", data->name);
}

void
theme_destroy_rc_style(GtkRcStyle * rc_style)
{
  ThemeRcData        *data = rc_style->engine_data;

  if (data)
    {
      g_free(data->name);
      g_free(data);
    }

  g_print("Theme theme: Destroying rc style for \"%s\"\n", data->name);
}

void
theme_destroy_style(GtkStyle * style)
{
  ThemeStyleData     *data = style->engine_data;

  if (data)
    {
      g_free(data->name);
      g_free(data);
    }

  g_print("Theme theme: Destroying style for \"%s\"\n", data->name);
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

  printf("Motif Theme Init\n");

  engine->parse_rc_style = theme_parse_rc_style;
  engine->merge_rc_style = theme_merge_rc_style;
  engine->rc_style_to_style = theme_rc_style_to_style;
  engine->duplicate_style = theme_duplicate_style;
  engine->realize_style = theme_realize_style;
  engine->unrealize_style = theme_unrealize_style;
  engine->destroy_rc_style = theme_destroy_rc_style;
  engine->destroy_style = theme_destroy_style;
  engine->set_background = theme_set_background;

}

void
theme_exit(void)
{
  printf("Motif Theme Exit\n* Need to add memory deallocation code here *\n");
}
