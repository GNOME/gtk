#include "th.h"

ThemeConfig *theme_config = NULL;

/* Theme functions to export */
void theme_init               (GtkThemeEngine *engine);
void theme_exit               (void);

/* Exported vtable from th_draw */

extern GtkStyleClass th_default_class;

/* internals */

void 
theme_read_config        ()
{
   ThemeConfig *cf;
   char *h,s[2048],ss[2048];
   FILE *f;
   int a,b,c;
   int i,j,k,l,m,n,o,p;
   
   h=getenv("HOME");
   snprintf(s,2048,"%s/themes/config",h);
   f=fopen(s,"r");
   cf=theme_config;
   for(i=0;i<3;i++)
     {
	for(j=0;j<5;j++)
	  {
	     for(k=0;k<2;k++)
	       {
		  cf->buttonconfig[i][j][k].button_padding.left=1;
		  cf->buttonconfig[i][j][k].button_padding.right=1;
		  cf->buttonconfig[i][j][k].button_padding.top=1;
		  cf->buttonconfig[i][j][k].button_padding.bottom=1;
		  cf->buttonconfig[i][j][k].min_w=0;
		  cf->buttonconfig[i][j][k].min_h=0;
		  cf->buttonconfig[i][j][k].border.filename=NULL;
		  cf->buttonconfig[i][j][k].border.image=NULL;
		  cf->buttonconfig[i][j][k].background.filename=NULL;
		  cf->buttonconfig[i][j][k].background.image=NULL;
		  cf->buttonconfig[i][j][k].number_of_decorations=0;
		  cf->buttonconfig[i][j][k].decoration=NULL;
	       }
	  }
     }
   cf->windowconfig.window_padding.left=1;
   cf->windowconfig.window_padding.right=1;
   cf->windowconfig.window_padding.top=1;
   cf->windowconfig.window_padding.bottom=1;
   cf->windowconfig.min_w=0;
   cf->windowconfig.min_h=0;
   cf->windowconfig.border.filename=NULL;
   cf->windowconfig.border.image=NULL;
   cf->windowconfig.background.filename=NULL;
   cf->windowconfig.background.image=NULL;
   cf->windowconfig.number_of_decorations=0;
   cf->windowconfig.decoration=NULL;
   if (!f)
     {
	fprintf(stderr,"THEME ERROR: No config file found. Looked for %s\n",s);
	exit(1);
     }
   while(fgets(s,2048,f))
     {
	if (s[0]!='#')
	  {
	     ss[0]=0;
	     sscanf(s,"%s",ss);
	     if (!strcmp(ss,"button"))
	       {
		  sscanf(s,"%*s %i %i %i %s",&a,&b,&c,ss);
		  if (!strcmp(ss,"padding"))
		    {
		       sscanf(s,"%*s %*i %*i %*i %*s %i %i %i %i",&i,&j,&k,&l);
		       cf->buttonconfig[a][b][c].button_padding.left=i;
		       cf->buttonconfig[a][b][c].button_padding.right=j;
		       cf->buttonconfig[a][b][c].button_padding.top=k;
		       cf->buttonconfig[a][b][c].button_padding.bottom=l;
		    }
		  else if (!strcmp(ss,"minimums"))
		    {
		       sscanf(s,"%*s %*i %*i %*i %*s %i %i",&i,&j);
		       cf->buttonconfig[a][b][c].min_w=i;
		       cf->buttonconfig[a][b][c].min_h=j;
		    }
		  else if (!strcmp(ss,"background"))
		    {
		       sscanf(s,"%*s %*i %*i %*i %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->buttonconfig[a][b][c].background.filename=strdup(s);
			    cf->buttonconfig[a][b][c].background.image=
			      gdk_imlib_load_image(cf->buttonconfig[a][b][c].background.filename);
			    if (!cf->buttonconfig[a][b][c].background.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->buttonconfig[a][b][c].background.filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"color"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i %i %i",&i,&j,&k);
			    cf->buttonconfig[a][b][c].background.color.r=i;
			    cf->buttonconfig[a][b][c].background.color.g=j;
			    cf->buttonconfig[a][b][c].background.color.b=k;
			    cf->buttonconfig[a][b][c].background.color.pixel=
			      gdk_imlib_best_color_match(&i,&j,&k);
			 }
		       else if (!strcmp(ss,"border"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i %i %i %i",&i,&j,&k,&l);
			    cf->buttonconfig[a][b][c].background.border.left=i;
			    cf->buttonconfig[a][b][c].background.border.right=j;
			    cf->buttonconfig[a][b][c].background.border.top=k;
			    cf->buttonconfig[a][b][c].background.border.bottom=l;
			    if (cf->buttonconfig[a][b][c].background.image)
			      gdk_imlib_set_image_border(cf->buttonconfig[a][b][c].background.image,
							 &cf->buttonconfig[a][b][c].background.border);
			 }
		       else if (!strcmp(ss,"scale"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i",&i);
			    cf->buttonconfig[a][b][c].background.scale_to_fit=i;
			 }
		       else if (!strcmp(ss,"parent_tile"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i",&i);
			    cf->buttonconfig[a][b][c].background.tile_relative_to_parent=i;
			 }
		    }
		  else if (!strcmp(ss,"border"))
		    {
		       sscanf(s,"%*s %*i %*i %*i %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->buttonconfig[a][b][c].border.filename=strdup(s);
			    cf->buttonconfig[a][b][c].border.image=
			      gdk_imlib_load_image(cf->buttonconfig[a][b][c].border.filename);
			    if (!cf->buttonconfig[a][b][c].border.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->buttonconfig[a][b][c].border.filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"border"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i %i %i %i",&i,&j,&k,&l);
			    cf->buttonconfig[a][b][c].border.border.left=i;
			    cf->buttonconfig[a][b][c].border.border.right=j;
			    cf->buttonconfig[a][b][c].border.border.top=k;
			    cf->buttonconfig[a][b][c].border.border.bottom=l;
			    if (cf->buttonconfig[a][b][c].border.image)
			      gdk_imlib_set_image_border(cf->buttonconfig[a][b][c].border.image,
							 &cf->buttonconfig[a][b][c].border.border);
			 }
		    }
		  else if (!strcmp(ss,"decoration"))
		    {
		       sscanf(s,"%*s %*i %*i %*i %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->buttonconfig[a][b][c].number_of_decorations++;
			    cf->buttonconfig[a][b][c].decoration=realloc
			      (cf->buttonconfig[a][b][c].decoration,
			       cf->buttonconfig[a][b][c].number_of_decorations*
			       sizeof(ThemeButtonDecoration));
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .filename=strdup(s);
			    cf->buttonconfig[a][b][c].decoration[cf->buttonconfig[a][b][c].number_of_decorations-1].image=
			      gdk_imlib_load_image(cf->buttonconfig[a][b][c].decoration
						   [cf->buttonconfig[a][b][c].number_of_decorations-1].filename);
			    if (!cf->buttonconfig[a][b][c].decoration
				[cf->buttonconfig[a][b][c].number_of_decorations-1]
				.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->buttonconfig[a][b][c].decoration[cf->buttonconfig[a][b][c].number_of_decorations-1].filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"coords"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %*s %i %i %i %i %i %i %i %i",&i,&j,&k,&l,&m,&n,&o,&p);
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .xrel=i;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .yrel=j;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .xabs=k;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .yabs=l;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .x2rel=m;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .y2rel=n;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .x2abs=o;
			    cf->buttonconfig[a][b][c].decoration
			      [cf->buttonconfig[a][b][c].number_of_decorations-1]
			      .y2abs=p;
			 }
		    }
	       }
	     else if (!strcmp(ss,"window"))
	       {
		  sscanf(s,"%*s %s",ss);
		  if (!strcmp(ss,"padding"))
		    {
		       sscanf(s,"%*s %*s %i %i %i %i",&i,&j,&k,&l);
		       cf->windowconfig.window_padding.left=i;
		       cf->windowconfig.window_padding.right=j;
		       cf->windowconfig.window_padding.top=k;
		       cf->windowconfig.window_padding.bottom=l;
		    }
		  else if (!strcmp(ss,"minimums"))
		    {
		       sscanf(s,"%*s %*s %i %i",&i,&j);
		       cf->windowconfig.min_w=i;
		       cf->windowconfig.min_h=j;
		    }
		  else if (!strcmp(ss,"background"))
		    {
		       sscanf(s,"%*s %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->windowconfig.background.filename=strdup(s);
			    cf->windowconfig.background.image=
			      gdk_imlib_load_image(cf->windowconfig.background.filename);
			    if (!cf->windowconfig.background.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->windowconfig.background.filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"color"))
			 {
			    sscanf(s,"%*s %*s %*s %i %i %i",&i,&j,&k);
			    cf->windowconfig.background.color.r=i;
			    cf->windowconfig.background.color.g=j;
			    cf->windowconfig.background.color.b=k;
			    cf->windowconfig.background.color.pixel=
			      gdk_imlib_best_color_match(&i,&j,&k);
			 }
		       else if (!strcmp(ss,"border"))
			 {
			    sscanf(s,"%*s %*s %*s %i %i %i %i",&i,&j,&k,&l);
			    cf->windowconfig.background.border.left=i;
			    cf->windowconfig.background.border.right=j;
			    cf->windowconfig.background.border.top=k;
			    cf->windowconfig.background.border.bottom=l;
			    if (cf->windowconfig.background.image)
			      gdk_imlib_set_image_border(cf->windowconfig.background.image,
							 &cf->windowconfig.background.border);
			 }
		       else if (!strcmp(ss,"scale"))
			 {
			    sscanf(s,"%*s %*s %*s %i",&i);
			    cf->windowconfig.background.scale_to_fit=i;
			 }
		       else if (!strcmp(ss,"parent_tile"))
			 {
			    sscanf(s,"%*s %*s %*s %i",&i);
			    cf->windowconfig.background.tile_relative_to_parent=i;
			 }
		    }
		  else if (!strcmp(ss,"border"))
		    {
		       sscanf(s,"%*s %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->windowconfig.border.filename=strdup(s);
			    cf->windowconfig.border.image=
			      gdk_imlib_load_image(cf->windowconfig.border.filename);
			    if (!cf->windowconfig.border.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->windowconfig.border.filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"border"))
			 {
			    sscanf(s,"%*s %*s %*s %i %i %i %i",&i,&j,&k,&l);
			    cf->windowconfig.border.border.left=i;
			    cf->windowconfig.border.border.right=j;
			    cf->windowconfig.border.border.top=k;
			    cf->windowconfig.border.border.bottom=l;
			    if (cf->windowconfig.border.image)
			      gdk_imlib_set_image_border(cf->windowconfig.border.image,
							 &cf->windowconfig.border.border);
			 }
		    }
		  else if (!strcmp(ss,"decoration"))
		    {
		       sscanf(s,"%*s %*s %s",ss);
		       if (!strcmp(ss,"image"))
			 {
			    sscanf(s,"%*s %*s %*s %s",ss);
			    snprintf(s,2048,"%s/themes/%s",h,ss);
			    cf->windowconfig.number_of_decorations++;
			    cf->windowconfig.decoration=realloc
			      (cf->windowconfig.decoration,
			       cf->windowconfig.number_of_decorations*
			       sizeof(ThemeButtonDecoration));
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .filename=strdup(s);
			    cf->windowconfig.decoration[cf->windowconfig.number_of_decorations-1].image=
			      gdk_imlib_load_image(cf->windowconfig.decoration
						   [cf->windowconfig.number_of_decorations-1].filename);
			    if (!cf->windowconfig.decoration
				[cf->windowconfig.number_of_decorations-1]
				.image)
			      {
				 fprintf(stderr,"ERROR: Cannot load %s\n",cf->windowconfig.decoration[cf->windowconfig.number_of_decorations-1].filename);
				 exit(1);
			      }
			 }
		       else if (!strcmp(ss,"coords"))
			 {
			    sscanf(s,"%*s %*s %*s %i %i %i %i %i %i %i %i",&i,&j,&k,&l,&m,&n,&o,&p);
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .xrel=i;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .yrel=j;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .xabs=k;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .yabs=l;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .x2rel=m;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .y2rel=n;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .x2abs=o;
			    cf->windowconfig.decoration
			      [cf->windowconfig.number_of_decorations-1]
			      .y2abs=p;
			 }
		    }
	       }
	  }
     }
   fclose(f);
}

/* external theme functions called */

typedef struct {
  gchar *name;
} ThemeRcData;

typedef struct {
  gchar *name;
} ThemeStyleData;

enum {
  TOKEN_NAME = G_TOKEN_LAST + 1
};

static struct
{
  gchar *name;
  guint token;
} theme_symbols[] = {
  { "name", TOKEN_NAME },
};

static guint n_theme_symbols = sizeof (theme_symbols) / sizeof (theme_symbols[0]);

guint 
theme_parse_name (GScanner   *scanner,
		   ThemeRcData *theme_data)
{
  guint token;
  
  token = g_scanner_get_next_token (scanner);
  if (token != TOKEN_NAME)
    return TOKEN_NAME;
  
  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_EQUAL_SIGN)
    return G_TOKEN_EQUAL_SIGN;

  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  if (theme_data->name)
    g_free (theme_data->name);

  theme_data->name = g_strdup (scanner->value.v_string);

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
  
  /*  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
  return G_TOKEN_LEFT_CURLY;*/
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_NAME:
	  token = theme_parse_name (scanner, theme_data);
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

