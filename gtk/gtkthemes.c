/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include "gtkthemes.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkprivate.h"

typedef struct _GtkThemeEnginePrivate GtkThemeEnginePrivate;

struct _GtkThemeEnginePrivate {
  GtkThemeEngine engine;
  
  void *theme_lib;
  guint refcount;
};

GtkThemesData th_dat;

void
gtk_themes_init (int	 *argc,
		 char ***argv)
{
   int i;
   char *s,ss[1024];
   char *thenv;
   
   /* Reset info */
   th_dat.theme_lib=NULL;
   th_dat.theme_name=NULL;
   th_dat.data=NULL;
   
   printf("init theme\n");
   /* get the library name for the theme */
   
   thenv=getenv("THEME");
   if (thenv)
     {
	snprintf(ss,1024,"%s/themes/lib%s.so",getenv("HOME"),thenv);
	th_dat.theme_name=strdup(ss);
     }
   else
     {
	if ((argc)&&(argv))
	  {
	     for(i=1;i<*argc;i++)
	       {
		  if ((*argv)[i]==NULL)
		    {
		       i+=1;continue;
		    }
/* If the program is run wiht a --theme THEME_NAME parameter it loads that */
/* theme currently hard-coded as ".lib/libTHEME_NAME.so" jsut so it will */
/* work for me for the moment */	     
		  if (strcmp("--theme",(*argv)[i])==0)
		    {
		       (*argv)[i]=NULL;
		       if (((i+1)<*argc)&&((*argv)[i+1]))
			 {
			    s=(*argv)[i+1];
			    if (s)
			      {
				 snprintf(ss,1024,"%s/themes/lib%s.so",getenv("HOME"),s);
				 th_dat.theme_name=strdup(ss);
			      }
			    (*argv)[i+1]=NULL;
			    i+=1;
			 }
		    }
	       }
	  }
     }
   /* load the lib */
   th_dat.theme_lib=NULL;
   if (th_dat.theme_name)
     {
	printf("Loading Theme %s\n",th_dat.theme_name);
	th_dat.theme_lib=dlopen(th_dat.theme_name,RTLD_NOW);
	if (!th_dat.theme_lib) 
	  fputs(dlerror(),stderr);
     }
   if (!th_dat.theme_lib) 
     {
	th_dat.init=NULL;
	th_dat.exit=NULL;
	th_dat.functions.gtk_style_new=NULL;
	th_dat.functions.gtk_style_set_background=NULL;
	return;
     }
   /* extract symbols from the lib */   
   th_dat.init=dlsym(th_dat.theme_lib,"theme_init");
   th_dat.exit=dlsym(th_dat.theme_lib,"theme_exit");
   th_dat.functions.gtk_style_new=dlsym(th_dat.theme_lib,"gtk_style_new");
   th_dat.functions.gtk_style_set_background=dlsym(th_dat.theme_lib,"gtk_style_set_background");

/* call the theme's init (theme_init) function to let it setup anything */   
   th_dat.init(argc,argv);
}

void
gtk_themes_exit (int errorcode)
{
   if (th_dat.exit);
   th_dat.exit();
/* free the theme library name and unload the lib */
   if (th_dat.theme_name) free(th_dat.theme_name);
   if (th_dat.theme_lib) dlclose(th_dat.theme_lib);
/* reset pointers to NULL */   
   th_dat.theme_name=NULL;
   th_dat.theme_lib=NULL;
   th_dat.data=NULL;
}

static GtkThemeEngine sample_engine;

GtkThemeEngine *
gtk_theme_engine_get (gchar          *name)
{
  GtkThemeEnginePrivate *result;
  
  if (!strcmp (name, "sample"))
    {
      result = g_new (GtkThemeEnginePrivate, 1);
      result->engine = sample_engine;
      result->refcount = 1;
      return (GtkThemeEngine *)result;
    }
  else
    return NULL;
}

void
gtk_theme_engine_ref (GtkThemeEngine *engine)
{
  g_return_if_fail (engine != NULL);
  
  ((GtkThemeEnginePrivate *)engine)->refcount++;
}

void
gtk_theme_engine_unref (GtkThemeEngine *engine)
{
  GtkThemeEnginePrivate *private;

  g_return_if_fail (engine != NULL);

  private = (GtkThemeEnginePrivate *)engine;
  private->refcount--;

  if (private->refcount == 0)
    g_free (private);
}

typedef struct {
  gchar *name;
} SampleRcData;

typedef struct {
  gchar *name;
} SampleStyleData;

enum {
  TOKEN_NAME = G_TOKEN_LAST + 1
};

static struct
{
  gchar *name;
  guint token;
} sample_symbols[] = {
  { "name", TOKEN_NAME },
};

static guint n_sample_symbols = sizeof (sample_symbols) / sizeof (sample_symbols[0]);

guint 
sample_parse_name (GScanner   *scanner,
		   SampleRcData *sample_data)
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

  if (sample_data->name)
    g_free (sample_data->name);

  sample_data->name = g_strdup (scanner->value.v_string);

  return G_TOKEN_NONE;
}

guint 
sample_parse_rc_style    (GScanner   *scanner, 
			  GtkRcStyle *rc_style)
{
  static GQuark scope_id = 0;
  SampleRcData *sample_data;
  guint old_scope;
  guint token;
  gint i;

  /* Set up a new scope in this scanner. */

  if (!scope_id)
    scope_id = g_quark_from_string ("sample_engine");
  
  /* If we bail out due to errors, we *don't* reset the scope, so the
   * error messaging code can make sense of our tokens.
   */
  old_scope = g_scanner_set_scope (scanner, scope_id);

  /* Now check if we already added our symbols to this scope
   * (in some previous call to sample_parse_rc_style for the
   * same scanner.
   */
  
  if (!g_scanner_lookup_symbol (scanner, sample_symbols[0].name))
    {
      g_scanner_freeze_symbol_table (scanner);
      for (i = 0; i < n_sample_symbols; i++)
	g_scanner_scope_add_symbol (scanner, scope_id, 
				    sample_symbols[i].name, 
				    GINT_TO_POINTER (sample_symbols[i].token));
      g_scanner_thaw_symbol_table (scanner);
    }

  /* We're ready to go, now parse the top level */

  sample_data = g_new (SampleRcData, 1);
  sample_data->name = NULL;
  
  /*  token = g_scanner_get_next_token (scanner);
  if (token != G_TOKEN_LEFT_CURLY)
  return G_TOKEN_LEFT_CURLY;*/
  
  token = g_scanner_peek_next_token (scanner);
  while (token != G_TOKEN_RIGHT_CURLY)
    {
      switch (token)
	{
	case TOKEN_NAME:
	  token = sample_parse_name (scanner, sample_data);
	  break;
	default:
	  g_scanner_get_next_token (scanner);
	  token = G_TOKEN_RIGHT_CURLY;
	  break;
	}

      if (token != G_TOKEN_NONE)
	{
	  g_free (sample_data);
	  return token;
	}
      token = g_scanner_peek_next_token (scanner);
    }

  g_scanner_get_next_token (scanner);

  rc_style->engine_data = sample_data;
  g_scanner_set_scope (scanner, old_scope);

  return G_TOKEN_NONE;
}

void  
sample_merge_rc_style    (GtkRcStyle *dest,     
			  GtkRcStyle *src)
{
  SampleRcData *src_data = src->engine_data;
  SampleRcData *dest_data = dest->engine_data;

  if (!dest_data)
    {
      dest_data = g_new (SampleRcData, 1);
      dest_data->name = NULL;
      dest->engine_data = dest_data;
    }

  if (dest_data->name == NULL)
    dest_data->name = g_strdup (src_data->name);
}

void  
sample_rc_style_to_style (GtkStyle   *style,    
			  GtkRcStyle *rc_style)
{
  SampleRcData *data = rc_style->engine_data;
  SampleStyleData *style_data;

  style_data = g_new (SampleStyleData, 1);
  style_data->name = g_strdup (data->name);

  style->engine_data = style_data;

  g_print ("Sample theme: Creating style for \"%s\"\n", data->name);
}

void  
sample_duplicate_style   (GtkStyle   *dest,       
			  GtkStyle   *src)
{
  SampleStyleData *dest_data;
  SampleStyleData *src_data = src->engine_data;

  dest_data = g_new (SampleStyleData, 1);
  dest_data->name = g_strdup (src_data->name);

  dest->engine_data = dest_data;
  
  g_print ("Sample theme: Duplicated style for \"%s\"\n", src_data->name);
}

void  
sample_realize_style     (GtkStyle   *style)
{
  SampleStyleData *data = style->engine_data;

  g_print ("Sample theme: Realizing style for \"%s\"\n", data->name);
}

void  
sample_unrealize_style     (GtkStyle   *style)
{
  SampleStyleData *data = style->engine_data;

  g_print ("Sample theme: Unrealizing style for \"%s\"\n", data->name);
}

void  
sample_destroy_rc_style  (GtkRcStyle *rc_style)
{
  SampleRcData *data = rc_style->engine_data;

  if (data)
    {
      g_free (data->name);
      g_free (data);
    }

  g_print ("Sample theme: Destroying rc style for \"%s\"\n", data->name);
}

void  
sample_destroy_style     (GtkStyle   *style)
{
  SampleStyleData *data = style->engine_data;

  if (data)
    {
      g_free (data->name);
      g_free (data);
    }

  g_print ("Sample theme: Destroying style for \"%s\"\n", data->name);
}

static GtkThemeEngine sample_engine = {
  sample_parse_rc_style,
  sample_merge_rc_style,
  sample_rc_style_to_style,
  sample_duplicate_style,
  sample_realize_style,
  sample_unrealize_style,
  sample_destroy_rc_style,
  sample_destroy_style
};
