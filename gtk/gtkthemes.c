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
#include "gtkbutton.h"
#include "gtkhscrollbar.h"
#include "gtkhseparator.h"
#include "gtkmain.h"
#include "gtkpreview.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtktext.h"
#include "gtkvbox.h"
#include "gtkvscrollbar.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"


GtkThemesData th_dat;

void
gtk_themes_init (int	 *argc,
		 char ***argv)
{
   int i;
   char *s,ss[1024];
/* Reset info */
   th_dat.theme_lib=NULL;
   th_dat.theme_name=NULL;
   th_dat.data=NULL;
   
   printf("init theme\n");
/* get the library name for the theme */
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
	th_dat.functions.button.border=NULL;
	th_dat.functions.button.init=NULL;
	th_dat.functions.button.draw=NULL;
	th_dat.functions.button.exit=NULL;
	return;
     }
/* extract symbols from the lib */   
   th_dat.init=dlsym(th_dat.theme_lib,"theme_init");
   th_dat.exit=dlsym(th_dat.theme_lib,"theme_exit");
   th_dat.functions.button.border=dlsym(th_dat.theme_lib,"button_border");
   th_dat.functions.button.init=dlsym(th_dat.theme_lib,"button_init");
   th_dat.functions.button.draw=dlsym(th_dat.theme_lib,"button_draw");
   th_dat.functions.button.exit=dlsym(th_dat.theme_lib,"button_exit");

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
