#include "th.h"

extern GtkThemesData th_dat;
ThemeConfig *theme_config = NULL;

/* Theme functions to export */
void theme_init               (int     *argc,
			       char ***argv);
void theme_exit               (void);

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

void 
theme_init               (int     *argc,
			  char ***argv)
{
   printf("Theme2 Init\n");
   theme_config = th_dat.data = malloc(sizeof(ThemeConfig));
   gdk_imlib_init();
/*   theme_read_config();*/
}

void 
theme_exit               (void)
{
   printf("Theme2 Exit\n* Need to add memory deallocation code here *\n");
}

