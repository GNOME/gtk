#include "theme2.h"

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
   int i,j,k,l;
   
   h=getenv("HOME");
   snprintf(s,2048,"%s/themes/config",h);
   f=fopen(s,"r");
   cf=(ThemeConfig *)th_dat.data;
   cf->button_padding.left=1;
   cf->button_padding.right=1;
   cf->button_padding.top=1;
   cf->button_padding.bottom=1;
   for(i=0;i<3;i++)
     {
	for(j=0;j<5;j++)
	  {
	     for(k=0;k<2;k++)
	       {
		  cf->buttonconfig[i][j][k].border.filename=NULL;
		  cf->buttonconfig[i][j][k].border.image=NULL;
		  cf->buttonconfig[i][j][k].background.filename=NULL;
		  cf->buttonconfig[i][j][k].background.image=NULL;
		  cf->buttonconfig[i][j][k].number_of_decorations=0;
		  cf->buttonconfig[i][j][k].decoration=NULL;
	       }
	  }
     }
   if (!f)
     {
	fprintf(stderr,"THEME ERROR: No config file found. Looked for %s\n",s);
	exit(1);
     }
   while(fgets(s,2048,f))
     {
	printf("%s\n",s);
	if (s[0]!='#')
	  {
	     ss[0]=0;
	     sscanf(s,"%s",ss);
	     if (!strcmp(ss,"button"))
	       {
		  sscanf(s,"%*s %s",ss);
		  if (!strcmp(ss,"padding"))
		    {
		       sscanf(s,"%*s %*s %i %i %i %i",&i,&j,&k,&l);
		       cf->button_padding.left=i;
		       cf->button_padding.right=j;
		       cf->button_padding.top=k;
		       cf->button_padding.bottom=l;
		    }
		  else
		    {
		       sscanf(s,"%*s %i %i %i %s",&a,&b,&c,ss);
		       if (!strcmp(ss,"background"))
			 {
			    sscanf(s,"%*s %*i %*i %*i %*s %s",ss);
			    if (!strcmp(ss,"image"))
			      {
				 sscanf(s,"%*s %*i %*i %*i %*s %*s %s",ss);
				 snprintf(s,2048,"%s/themes/%s",h,ss);
				 cf->buttonconfig[a][b][c].background.filename=strdup(s);
				 printf("%i %i %i %s\n",a,b,c,s);
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
   th_dat.data=malloc(sizeof(ThemeConfig));
   gdk_imlib_init();
   theme_read_config();
}

void 
theme_exit               (void)
{
   printf("Theme2 Exit\n* Need to add memory deallocation code here *\n");
}

