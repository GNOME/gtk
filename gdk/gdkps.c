/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* FIXME: add X copyright */
/* TODO:
   font mapping
   font downloading
   font sizes
   checks for drawable types everywhere
   split long lines in postscript output
   background color in dashes
*/
#include "gdk/gdk.h"
#include "gdk/gdkprivate.h"
#include "gdk/gdkx.h"
#include "gdk/gdkps.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#define GDKPS(w) (GdkPsDrawable*)((GdkWindowPrivate*)w)->window.user_data

static void ps_destroy (GdkDrawable *d);
static void ps_out_num (GdkPsDrawable *d, gfloat f);
static void ps_out_int (GdkPsDrawable *d, gint f);
static void ps_out_flush (GdkPsDrawable *d, gint force);
static void ps_out_text(GdkPsDrawable *d, gchar* text);
static void ps_out_invalidate(GdkPsDrawable *d);

/* Add this function to gdkimage.c and gdk.h */
static gchar*
get_image_data(GdkImage* image, GdkColormap *cmap, gint* bpp, gint x, gint y, gint width, gint height) {
	guchar* data;
	guchar* ptr;
	GdkColor ctab[256];
	GdkColormapPrivate* pcmap;
	GdkVisual *visual;
	gint i, sx;
	guint32 pixel;
	gint white=1, black=0;

	pcmap = (GdkColormapPrivate*)cmap;
	visual = image->visual;
	if ( !visual )
		visual = pcmap->visual;
	if ( !visual )
		visual = gdk_visual_get_system();

	if ( width < 0 )
		width = image->width;
	if ( height < 0 )
		height = image->height;

	sx = x;

#ifdef buggy_1bpp
	if ( image->depth == 1 ) /* What about gray levels? */
		*bpp = 1;
	else
#endif
		*bpp = 3;
	/*g_warning("IMAGE DATA: bpp=%d; depth=%d; my_bpp=%d", image->bpp, image->depth, *bpp);*/

	data = g_malloc((*bpp) * width * height);
	if ( !data )
		return NULL;
	/* mostly stolen from imlib */
	if ( image->depth == 1 ) {
		/*white = WhitePixel(GDK_DISPLAY(), 0);
		black = BlackPixel(GDK_DISPLAY(), 0);*/
		/*g_warning("b: %d; w: %d", black, white);*/
		ctab[white].red = 65535;
		ctab[white].green = 65535;
		ctab[white].blue = 65535;
		ctab[black].red = 0;
		ctab[black].green = 0;
		ctab[black].blue = 0;
	}
	if ( image->depth <= 8 ) {
		XColor cols[256];
		for (i = 0; i < (1 << image->depth); i++) {
			cols[i].pixel = i;
			cols[i].flags = DoRed|DoGreen|DoBlue;
		}
		XQueryColors(pcmap->xdisplay, pcmap->xcolormap, cols, 1 << image->depth);
		for (i = 0; i < (1 << image->depth); i++) {
			ctab[i].red = cols[i].red;
			ctab[i].green = cols[i].green;
			ctab[i].blue = cols[i].blue;
			ctab[i].pixel = cols[i].pixel;
		}
	}
	ptr = data;
	switch(image->depth) {
	case 0:
	case 1:
#ifdef buggy_1bpp
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = ctab[pixel & 0xff].red>>8;  /* FIXME */
			}
		}
		break;
#endif
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = ctab[pixel & 0xff].red >> 8;
				*ptr++ = ctab[pixel & 0xff].green >> 8;
				*ptr++ = ctab[pixel & 0xff].blue >> 8;
			}
		}
		break;
	case 15:
/*
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = (pixel >> 7) & 0xf8;
				*ptr++ = (pixel >> 3) & 0xf8;
				*ptr++ = (pixel << 3) & 0xf8;
			}
		}
		break;
*/
	case 16:
/*
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = (pixel >> 8) & 0xf8;
				*ptr++ = (pixel >> 3) & 0xfc;
				*ptr++ = (pixel << 3) & 0xf8;
			}
		}
		break;
*/
	case 24:
	case 32:
/*
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = (pixel >> 16) & 0xff;
				*ptr++ = (pixel >> 8) & 0xff;
				*ptr++ = pixel & 0xff;
			}
		}
		break;
*/
		for (; y < height; ++y) {
			for (x=sx; x < width; ++x) {
				pixel = gdk_image_get_pixel(image, x, y);
				*ptr++ = (pixel & visual->red_mask) >> visual->red_shift;
				*ptr++ = (pixel & visual->green_mask) >> visual->green_shift;
				*ptr++ = (pixel & visual->blue_mask) >> visual->blue_shift;
				/*
				*ptr++ = (pixel >> visual->red_shift) & visual->red_mask;
				*ptr++ = (pixel >> visual->green_shift) & visual->grenn_mask;
				*ptr++ = (pixel >> visual->blue_shift) & visual->blue_mask;
				*/
			}
		}
		break;
	default:
		/* error? */
	}

	return data;
}

void
gdk_ps_drawable_add_font_info(GdkPsFontInfo *info) {
}

static void ps_out_invalidate(GdkPsDrawable *d) {
	if ( d->font )
		gdk_font_unref(d->font);
	d->font = NULL;
	d->valid = 0;
	d->valid_fg = 0;
	if ( d->dash_list ) {
		g_free(d->dash_list);
		d->dash_list = NULL;
		d->dash_num = 0;
		d->dash_offset = 0;
	}
	
}

/*
 *  Standard definitions
 */
  
static char *S_StandardDefs = "\
/d{def}bind def\
/b{bind}bind d\
/bd{b d}b d\
/x{exch}bd\
/xd{x d}bd\
/dp{dup}bd\
\n\
/t{true}bd\
/f{false}bd\
/p{pop}bd\
/r{roll}bd\
/c{copy}bd\
/i{index}bd\
\n\
/rp{repeat}bd\
/n{newpath}bd\
/w{setlinewidth}bd\
\n\
/lc{setlinecap}bd\
/lj{setlinejoin}bd\
/sml{setmiterlimit}bd\
\n\
/ds{setdash}bd\
/ie{ifelse}bd\
/len{length}bd\
/m{moveto}bd\
\n\
/l{lineto}bd\
/rl{rlineto}bd\
/a{arc}bd\
/an{arcn}bd\
/st{stroke}bd\
\n\
/fl{fill}bd\
/ef{eofill}bd\
/sp{showpage}bd\
\n\
/cp{closepath}bd\
/clp{clippath}bd\
/cl{clip}bd\
/pb{pathbbox}bd\
\n\
/tr{translate}bd\
/rt{rotate}bd\
/dv{div}bd\
/ml{mul}bd\
\n\
/ad{add}bd\
/ng{neg}bd\
/scl{scale}bd\
/sc{setrgbcolor}bd\
\n\
/g{setgray}bd\
/gs{gsave}bd\
/gr{grestore}bd\
/sv{save}bd\
/rs{restore}bd\
\n\
/mx{matrix}bd\
/cm{currentmatrix}bd\
/sm{setmatrix}bd\
\n\
/ccm{concatmatrix}bd\
/cc{concat}bd\
/ff{findfont}bd\
/mf{makefont}bd\
\n\
/sf{setfont}bd\
/cft{currentfont}bd\
/fd{FontDirectory}bd\
/sh{show}bd\
/stw{stringwidth}bd\
\n\
/ci{colorimage}bd\
/ig{image}bd\
/im{imagemask}bd\
/cf{currentfile}bd\
\n\
/rh{readhexstring}bd\
/str{string}bd\
/al{aload}bd\
/wh{where}bd\
/kn{known}bd\
\n\
/stp{stopped}bd\
/bg{begin}bd\
/ed{end}bd\
/fa{forall}bd\
\n\
/pi{putinterval}bd\
/mk{mark}bd\
/ctm{cleartomark}bd\
/df{definefont}bd\
\n\
/cd{currentdict}bd\
/db{20 dict dp bg}bd\
/de{ed}bd\
\n\
/languagelevel wh{p languagelevel}{1}ie\
\n\
 1 eq{/makepattern{p}bd/setpattern{p}bd/setpagedevice{p}bd}if\
\n\
/mp{makepattern}bd\
/spt{setpattern}bd\
/spd{setpagedevice}bd\
\n";

/*
 *  Composite definitions
 *
 *
 *    XYr  -  Return X/Y dpi for device
 *
 *      XYr <xdpi> <ydpi>
 *
 *    Cs  -  Coordinate setup (for origin upper left)
 *
 *      <orient(0,1,2,3)> Cs
 *
 *    P  -  Draw a point
 *
 *      <x> <y> P
 *
 *    R  -  Add rectangle to path
 *
 *      <x> <y> <w> <h> R
 *
 *    Ac  -  Add arc to path
 *
 *      <x> <y> <w> <h> <ang1> <ang2> Ac
 *
 *    An  -  Add arc to path (counterclockwise)
 *
 *      <x> <y> <w> <h> <ang1> <ang2> An
 *
 *    Tf  -  Set font
 *
 *      <font_name> <size> <iso> Tf
 *
 *    Tfm  -  Set font with matrix
 *
 *      <font_name> <matrix> <iso> Tfm
 *
 *    T  -  Draw text
 *
 *      <text> <x> <y> T
 *
 *    Tb  -  Draw text with background color
 *
 *      <text> <x> <y> <bg_red> <bg_green> <bg_blue> Tb
 *
 *    Im1  -  Image 1 bit monochrome imagemask
 *
 *      <x> <y> <w> <h> <sw> <sh> Im1
 *
 *    Im24  -  Image 24 bit RGB color
 *
 *      <x> <y> <w> <h> <sw> <sh> Im24
 *
 *    Im1t  -  Image 1 bit monochrome imagemask (in tile)
 *
 *      <data> <x> <y> <w> <h> <sw> <sh> Im1t
 *
 *    Im24t  -  Image 24 bit RGB color (in tile)
 *
 *      <data> <x> <y> <w> <h> <sw> <sh> Im24t
 */

static char *S_CompositeDefs = "\
/XYr{/currentpagedevice wh\n\
  {p currentpagedevice dp /HWResolution kn\n\
    {/HWResolution get al p}{p 300 300}ie}{300 300}ie}bd\n\
/Cs{dp 0 eq{0 pHt tr XYr -1 x dv 72 ml x 1 x dv 72 ml x scl}if\n\
  dp 1 eq{90 rt XYr -1 x dv 72 ml x 1 x dv 72 ml x scl}if\n\
  dp 2 eq{pWd 0 tr XYr 1 x dv 72 ml x -1 x dv 72 ml x scl}if\n\
  3 eq{pHt pWd tr 90 rt XYr 1 x dv 72 ml x -1 x dv 72 ml x scl}if}bd\n\
/P{gs 1 w [] 0 ds 2 c m .1 ad x .1 ad x l st gr}bd\n\
/R{4 2 r m 1 i 0 rl 0 x rl ng 0 rl cp}bd\n\
/Ac{mx_ cm p 6 -2 r tr 4 2 r ng scl 0 0 .5 5 3 r a mx_ sm}bd\n\
/An{mx_ cm p 6 -2 r tr 4 2 r ng scl 0 0 .5 5 3 r an mx_ sm}bd\n\
/ISO{dp len dict bg{1 i/FID ne{d}{p p}ie}fa\n\
  /Encoding ISOLatin1Encoding d cd ed df}bd\n\
/iN{dp len str cvs dp len x 1 i 3 ad str 2 c c p x p dp 3 -1 r(ISO)pi}bd\n\
/Tp{{x dp iN dp fd x kn{x p dp/f_ x d ff}{dp/f_ x d x ff ISO}ie x}\n\
  {x dp/f_ x d ff x}ie}bd\n\
/Tf{Tp[x 0 0 2 i ng 0 0] dp/fm_ x d mf sf}bd\n\
/Tfm{Tp 1 -1 tm1_ scl tm2_ ccm dp/fm_ x d mf sf}bd\n\
/T{m sh}bd\n\
/Tb{gs sc f_ ff sf cft/FontMatrix get 3 get\n\
  cft/FontBBox get dp 1 get x 3 get 2 i ml 3 1 r ml\n\
  0 0 m 4 i stw p 4 i 4 i m fm_ cc\n\
  0 2 i rl dp 0 rl 0 2 i ng rl 0 3 i rl ng 0 rl cp fl p p\n\
  gr T}bd\n\
/Im1{6 4 r tr scl t [3 i 0 0 5 i 0 0]{cf str1 rh p} im}bd\n\
/Im24{gs 6 4 r tr scl 8 [3 i 0 0 5 i 0 0]{cf str3 rh p} f 3 ci}bd\n\
/Im1t{6 4 r tr scl t [3 i 0 0 5 i 0 0]{} im}bd\n\
/Im24t{gs 6 4 r tr scl 8 [3 i 0 0 5 i 0 0]{} f 3 ci}bd\n\
\n";

/*
 *  Setup definitions
 */

static char *S_SetupDefs = "\
 /mx_ mx d\
 /im_ mx d\
 /tm1_ mx d\
 /tm2_ mx d\
 /str3 3 str d\
 /str1 1 str d\
\n";

static void
ps_out_download(GdkPsDrawable *d, gchar* name, gchar* fname) {
	int stt;
	gchar buf[16];
	gchar hexbuf[80];
	gint pos;
	FILE *fp;
	gchar *nb;
	gint cur_len=1024;
	const char hextab[16] = "0123456789abcdef";

	fp = fopen(fname, "r");
	if( !fp ) return;
	ps_out_text(d, "\n%%BeginFont: ");
	ps_out_text(d, name);
	ps_out_text(d, "\n");

	nb = g_malloc(cur_len);
	fread(buf, 1, 1, fp);
	fseek(fp, (long)0, 0);
	if ( (buf[0]&0xFF)==0x80 ) { /* pfb to pfa */
		int len;
		int type;
		for (;;) {
			stt = fread(buf, 1, 2, fp);
			if( stt!=2 || (buf[0]&0xFF)!=0x80 ) break;
			type = buf[1];
			if( type<1 || type>2 ) break;
			stt = fread(buf, 1, 4, fp);
			if( stt!=4 ) break;
			len = ((buf[3]&0xFF)<<24)|((buf[2]&0xFF)<<16)|
				((buf[1]&0xFF)<<8)|(buf[0]&0xFF);
			pos = 0;
			if ( len > cur_len -1 ) {
				nb = g_realloc(nb, len*2 );
				cur_len = len*2;
			}
			stt = fread(nb, 1, len, fp);
			if( stt<=0 ) break;
			nb[stt] = 0;
			if ( type == 1 )
				ps_out_text(d, nb);
			else {
				int j;
				for (j=0; j < stt; ++j) {
					sprintf(hexbuf+pos, "%02x", (int)nb[j]);
					pos += 2;
					/*hexbuf[pos++] = hextab[(nb[j] >> 4)&0xff];
					hexbuf[pos++] = hextab[(nb[j] & 15)&0xff];*/
					if ( pos == 76 ) {
						hexbuf[pos++] = '\n';
						hexbuf[pos++] = 0;
						ps_out_text(d, hexbuf);
						pos = 0;
					}
				}
				if ( pos == 76 ) {
					hexbuf[pos++] = '\n';
					hexbuf[pos++] = 0;
					ps_out_text(d, hexbuf);
				}
			}
		}
	} else {
		for(;;) {
			stt = fread(nb, 1, cur_len-1, fp);
			if( stt<=0 ) break;
			nb[stt] = 0;
			ps_out_text(d, nb);
			if( stt<cur_len-1 ) break;
		}
	}
	fclose(fp);
	g_free(nb);
	ps_out_text(d, "\n%%EndFont\n");
}

static void
ps_out_begin(GdkPsDrawable *d, gchar* title, gchar* author) {
	time_t date;

	date = time(NULL);
	
	ps_out_text(d, "%!PS-Adobe-3.0\n");
	ps_out_text(d, "%%Creator: Gdk PostScript Print Driver\n");
	ps_out_text(d, "%%DocumentMedia: A4 595 842 80 white ()\n"); /* FIXME */
	ps_out_text(d, "%%CreationDate: ");
	ps_out_text(d, ctime(&date));
	if (title ) {
		ps_out_text(d, "%%Title: ");
		ps_out_text(d, title?title:"");
		ps_out_text(d, "\n");
	}
	if ( author ) {
		ps_out_text(d, "%%Author: ");
		ps_out_text(d, author?author:"");
		ps_out_text(d, "\n");
	}
	ps_out_text(d, "%%Pages (atend)\n");
	/**/
	ps_out_text(d, "%%EndComments\n");
	ps_out_text(d, "%%BeginProlog\n");
	ps_out_text(d, "%%BeginProcSet: XServer_PS_Functions\n");
	ps_out_text(d, S_StandardDefs);
	ps_out_text(d, S_CompositeDefs);
	ps_out_text(d, "%%EndProcSet\n");
	ps_out_text(d, "%%EndProlog\n");
	ps_out_text(d, "%%BeginSetup\n");
	ps_out_text(d, S_SetupDefs);
	ps_out_text(d, "%%EndSetup\n");

	/* Download fonts */
	/*ps_out_download(d, "Agate-Normal", "/usr/X11R6/lib/X11/fonts/freefont/agate.pfb");*/

}

static void
ps_out_end(GdkPsDrawable *d) {
	ps_out_text(d, "%%Pages: ");
	ps_out_int(d, d->page);
	ps_out_text(d, "\n%%EOF\n");
	ps_out_flush(d, 1);
}

static void
ps_out_page(GdkDrawable *w, gint orient, gint count, gint plex, gint res, gint wd, gint ht) {
	gfloat fwd = ((gfloat)wd/(gfloat)res)*72.;
	gfloat fht = ((gfloat)ht/(gfloat)res)*72.;
	gchar buf[64];
	GdkPsDrawable* d = GDKPS(w);
	/*GdkWindowPrivate* wp = (GdkWindowPrivate*)w;

	wp->width = wd;
	wp->height = ht;*/
	
	ps_out_invalidate(d);

	sprintf(buf, "%%%%Page: %d\n", ++d->page); /* FIXME: page label */
	ps_out_text(d, buf);
	ps_out_text(d, "%%PageMedia: A4\n");
	/* FIXME: bounding box, page media */
	sprintf(buf, "%%%%BoundingBox: %d %d %d %d\n", 0, 0, (int)fwd, (int)fht);
	ps_out_text(d, buf);
	ps_out_text(d, " /pWd");
	ps_out_num(d, fwd);
	ps_out_text(d, " d /pHt");
	ps_out_num(d, fht);
	ps_out_text(d, " d\n");

	ps_out_text(d, " {db");
	if ( count > 1 ) {
		ps_out_text(d, " /NumCopies");
		ps_out_int(d, count);
		ps_out_text(d, " d");
	}
	if (plex) ps_out_text(d, " /Duplex t d");
	if ( plex==1 ) ps_out_text(d, " /Tumble f d");
	else ps_out_text(d, " /Tumble t d");

	ps_out_text(d, " /Orientation");
	ps_out_int(d, orient);

	ps_out_text(d, " d/HWResolution [");
	ps_out_num(d, res);
	ps_out_num(d, res);
	ps_out_text(d, " ] d/PageSize [pWd pHt]d de spd}stp p\n");

	ps_out_text(d, " gs");
	ps_out_int(d, orient);
	ps_out_text(d, " Cs 100 sml gs\n");
}

static void
ps_out_page_end(GdkPsDrawable *d) {
	if (d->clipped)
		ps_out_text(d, " gr");
	ps_out_text(d, " gr gr sp\n");
	d->clipped = 0;
	g_free(d->rects);
	d->rects = NULL;
	d->nrects = 0;
}

static void
ps_out_color(GdkPsDrawable *d, GdkColor* color) {
	if (color->green == color->red && color->green == color->blue) {
		ps_out_num(d, color->green/65535.0);
		ps_out_text(d, " g\n");
	} else {
		ps_out_num(d, color->red/65535.0);
		ps_out_num(d, color->green/65535.0);
		ps_out_num(d, color->blue/65535.0);
		ps_out_text(d, " sc\n");
	}
}

static void
ps_out_fgcolor(GdkPsDrawable *d, GdkColor* color) {
	if (d->valid_fg && color->red == d->fg.red && color->green == d->fg.green 
			&& color->blue == d->fg.blue )
		return;
	ps_out_color(d, color);
	d->fg = *color;
	d->valid_fg = 1;
}

static void
ps_out_fill(GdkPsDrawable *d, GdkGCValues* v) {
	/* FIXME */
	d->valid_fg = 0;
	if ( v->fill == GDK_SOLID ) {
		ps_out_fgcolor(d, &(v->foreground));
	} else if ( v->fill == GDK_TILED ) {
	} else if ( v->fill == GDK_STIPPLED ) {
	} else if ( v->fill == GDK_OPAQUE_STIPPLED ) {
	}
}

static gint
compare_dashes(GdkPsDrawable *d, GdkGC *gc, GdkGCValues* v) {
	GdkGCPrivate* p = (GdkGCPrivate*)gc;
	if ( p->dash_num != d->dash_num )
		return 1;
	if ( p->dash_offset != d->dash_offset )
		return 1;
	if ( p->dash_num && memcmp(p->dash_list, d->dash_list, p->dash_num) )
		return 1;
	return 0;
}

static void
ps_out_line_attrs(GdkPsDrawable *d, GdkGC *gc, GdkGCValues* v) {
	GdkGCPrivate* p = (GdkGCPrivate*)gc;
	gint cap[] = {0, 0, 1, 2}; /* mapping between ps and gdk cap/join values */
	gint join[] = {0, 1, 2};
	gint i;

	if ( !d->valid || (v->line_width != d->line_width && v->line_width >= 0) ) {
		if ( v->line_width == 0 ) 
			v->line_width = 1;
		d->line_width = v->line_width;
		ps_out_num(d, (gfloat)v->line_width);
		ps_out_text(d, " w");
	}
	if ( !d->valid || (v->cap_style != d->cap_style) ) {
		d->cap_style = v->cap_style;
		ps_out_int(d, cap[v->cap_style]);
		ps_out_text(d, " lc");
	}
	if ( !d->valid || (v->join_style != d->join_style) ) {
		d->join_style = v->join_style;
		ps_out_int(d, join[v->join_style]);
		ps_out_text(d, " lj");
	}
	if ( !d->valid || (compare_dashes(d, gc, v))) {
		g_free(d->dash_list);
		d->dash_list = NULL;
		d->dash_offset = p->dash_offset;
		d->dash_num = p->dash_num;
		if (d->dash_num) {
			d->dash_list = g_new(gchar, d->dash_num);
			memcpy(d->dash_list, p->dash_list, d->dash_num);
		}
		ps_out_text(d, " [");
		for ( i=0; i < d->dash_num; ++i)
			ps_out_int(d, p->dash_list[i]);
		ps_out_text(d, " ]");
		ps_out_int(d, d->dash_num);
		ps_out_text(d, " ds\n");
	}
	if (v->line_style != GDK_LINE_DOUBLE_DASH) {
		d->valid_bg = 1;
		d->bg = p->bg;
	} else {
		d->valid_bg = 0;
	}
	d->valid = 1;
}

/*
static void
ps_out_tok(GdkPsDrawable *d, gchar* text, gint cr) {
	if (d->file) {
		fputc(' ', d->file);
		fwrite(text, strlen(text), 1, d->file);
		if (cr)
			fputc('\n', d->file);
	} else {
		d->sbuf = g_string_append_c(d->sbuf, ' ');
		d->sbuf = g_string_append(d->sbuf, text);
		if (cr)
			d->sbuf = g_string_append_c(d->sbuf, '\n');
	}
}
*/

static void
ps_out_flush(GdkPsDrawable* d, gint force) {
	if ( d->fd >= 0 && (force || (d->sbuf->str && strlen(d->sbuf->str) > 1024)) ) {
		write(d->fd, d->sbuf->str, strlen(d->sbuf->str));
		d->sbuf = g_string_truncate(d->sbuf, 0); /* FIXME: check out of space */
	}
}

static void
ps_out_num (GdkPsDrawable *d, gfloat num) {
	gchar buf[64];
	gint i;
	
	sprintf(buf, " %.3f", num);
	for (i=strlen(buf)-1; buf[i]=='0' ; i-- ); 
	buf[i+1] = '\0';
	if ( buf[strlen(buf)-1]=='.' ) 
		buf[strlen(buf)-1] = '\0';
	g_string_append(d->sbuf, buf);
	ps_out_flush(d, 0);
}

static void
ps_out_int (GdkPsDrawable *d, gint num) {
	g_string_sprintfa(d->sbuf, " %d", num);
	ps_out_flush(d, 0);
}

static void
ps_out_str (GdkPsDrawable *d, gchar* text, gint tlen) {
	gint i;

	d->sbuf = g_string_append_c(d->sbuf, '(');
	for (i=0; i < tlen; ++i) {
		if ( (text[i] >= ' ' && text[i] <= '~') &&
				text[i] != '(' && text[i] != ')' && text[i] != '\\' )
			d->sbuf = g_string_append_c(d->sbuf, text[i]);
		else
			g_string_sprintfa(d->sbuf, "\\%03o", text[i]&0xFF);
	}
	d->sbuf = g_string_append_c(d->sbuf, ')');
	ps_out_flush(d, 0);
}

static void
ps_out_text(GdkPsDrawable *d, gchar* text) {
	d->sbuf = g_string_append(d->sbuf, text);
	ps_out_flush(d, 0);
}

static void
ps_out_offset(GdkPsDrawable *d, gint x, gint y) {
	d->xoff = x;
	d->yoff = y;
}

static void
ps_out_lines(GdkPsDrawable *d, gint n, GdkPoint* points) {
	gint i;
	gint xo = d->xoff;
	gint yo = d->yoff;

	if ( d->inframe || d->intile ) 
		xo = yo = 0;
	if ( n < 2 ) return;
	for ( i=0; i < n; ++i) {
		ps_out_num(d, (gfloat)(points[i].x+xo));
		ps_out_num(d, (gfloat)(points[i].y+yo));
		ps_out_text(d, i==0? " m": " l");
	}
	if ( d->valid_bg ) {
		ps_out_text(d, " gs");
		ps_out_color(d, &d->bg);
		ps_out_text(d, "[] 0 ds st gr");
	}
	ps_out_text(d, " st\n");
}

static void
ps_out_points(GdkPsDrawable *d, gint n, GdkPoint* points) {
	gint i;
	gint xo = d->xoff;
	gint yo = d->yoff;

	if ( d->inframe || d->intile ) 
		xo = yo = 0;
	for ( i=0; i < n; ++i) {
		ps_out_num(d, (gfloat)(points[i].x+xo));
		ps_out_num(d, (gfloat)(points[i].y+yo));
		ps_out_text(d, " P\n");
	}
}

static void
update_gc(GdkPsDrawable *d, GdkGC* gc) {
	/* output clip path */
	GdkGCPrivate *private;
	gint i, xo, yo;
	gint changed = 0;

	private = (GdkGCPrivate *)gc;
	xo = d->xoff;
	yo = d->yoff;

	if ( d->intile )
		return;
	if ( d->inframe )
		xo = yo = 0;

	if ( private->nrects != d->nrects ||
			memcmp(private->rects, d->rects, sizeof(GdkRectangle)*d->nrects)) 
		changed = 1;

	if ( !changed )
		return;

	g_free(d->rects);
	d->rects = NULL;
	d->nrects = private->nrects;
	if ( d->nrects ) {
		d->rects = g_new(GdkRectangle, private->nrects);
		memcpy(d->rects, private->rects, sizeof(GdkRectangle)*d->nrects);
	}

	ps_out_invalidate(d);
	if (d->clipped)
		ps_out_text(d, " gr");
	d->clipped = 1;
	ps_out_text(d, " gs\n");
	for (i=0; i < private->nrects; ++i) {
		ps_out_num(d, private->rects[i].x + xo);
		ps_out_num(d, private->rects[i].y + yo);
		ps_out_num(d, private->rects[i].width);
		ps_out_num(d, private->rects[i].height);
		ps_out_text(d, " R\n");
	}
	ps_out_text(d, "cl n\n");
	
}

static void
ps_draw_point(GdkDrawable* w, GdkGC* gc, gint x, gint y) {
	GdkPoint point;
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));

	point.x = x;
	point.y = y;
	ps_out_points(d, 1, &point);
}

static void 
ps_draw_line(GdkDrawable* w, GdkGC* gc, gint x1, gint y1, gint x2, gint y2) {
	GdkPoint points[2];
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);
	points[0].x = x1;
	points[0].y = y1;
	points[1].x = x2;
	points[1].y = y2;
	ps_out_lines(d, 2, points);

}

static void
ps_draw_rectangle(GdkDrawable* wd, GdkGC* gc, gint filled, gint x, gint y, gint w, gint h) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(wd);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	if ( filled )
		ps_out_fill(d, &values);
	else
		ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);

	ps_out_num(d, (gfloat)x);
	ps_out_num(d, (gfloat)y);
	ps_out_num(d, (gfloat)w);
	ps_out_num(d, (gfloat)h);
	ps_out_text(d, filled ? " R fl" : " R");
	if ( !filled && d->valid_bg ) {
		ps_out_text(d, " gs");
		ps_out_color(d, &d->bg);
		ps_out_text(d, "[] 0 ds st gr");
	}
	ps_out_text(d, " st\n");

}

static void
ps_draw_arc(GdkDrawable* wd, GdkGC* gc, gint filled, gint x, gint y, gint w, gint h, gint angle1, gint angle2) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(wd);
	gint xo = d->xoff;
	gint yo = d->yoff;

	if ( d->inframe || d->intile ) 
		xo = yo = 0;

	x += xo;
	y += yo;

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	if ( filled )
		ps_out_fill(d, &values);
	else
		ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);

	if ( 1 /* pieslice */ ) {
		ps_out_num(d, x + w/2.0);
		ps_out_num(d, y + h/2.0);
		ps_out_text(d, " m");
	}
	ps_out_num(d, x + w/2.0);
	ps_out_num(d, y + h/2.0);
	ps_out_num(d, (gfloat)w);
	ps_out_num(d, (gfloat)h);
	ps_out_num(d, (gfloat)angle1/64.0);
	ps_out_num(d, (angle1+angle2)/64.0);
	ps_out_text(d, angle2 < 0 ? " An" : " Ac");
	if ( !filled ) {
		if ( d->valid_bg ) {
			ps_out_text(d, " gs");
			ps_out_color(d, &d->bg);
			ps_out_text(d, "[] 0 ds st gr");
		}
		ps_out_text(d, " st\n");
	} else {
		ps_out_text(d, " cp fl\n");
	}
}

static void
ps_draw_polygon(GdkDrawable* w, GdkGC* gc, gint filled, GdkPoint* points, gint npoints) {
	GdkGCValues values;
	int i;
	GdkPsDrawable* d = GDKPS(w);
	gint xo = d->xoff;
	gint yo = d->yoff;

	if ( d->inframe || d->intile ) 
		xo = yo = 0;
	if ( npoints < 2 )
		return;

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	if ( filled )
		ps_out_fill(d, &values);
	else
		ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);

	for (i=0; i < npoints; ++i) {
		ps_out_num(d, points[i].x+xo);
		ps_out_num(d, points[i].y+yo);
		ps_out_text(d, i==0?" m":" l");
	}
	ps_out_num(d, points[0].x+xo);
	ps_out_num(d, points[0].y+yo);
	ps_out_text(d, " l");
	if ( !filled ) {
		/* lineBclr? */
		ps_out_text(d, " st\n");
	} else {
		ps_out_text(d, " cp fl\n"); /* FIXME: fillrule? */
	}
}

gchar* gdk_ps_drawable_check_font(GdkFont* font, gint *size, gint *is_ps, gint *is_iso);

static void
ps_out_font(GdkPsDrawable* d, GdkFont* font, GdkGC* gc) {
	gchar* name;
	gint is_ps, is_iso, size;
	gchar buf[80];

	if ( d->font && !g_strcasecmp(d->font->name, font->name) )
		return;
	if ( d->font )
	  gdk_font_unref(d->font);
	d->font = font;
	gdk_font_ref(d->font);

	name = gdk_ps_drawable_check_font(font, &size, &is_ps, &is_iso);
	if ( !is_ps )
		g_warning("Using a non-PS font");
	sprintf(buf, " /%s %d %c Tf\n", name, size, is_iso?'t':'f');
	ps_out_text(d, buf);
	g_free(name);
}

gchar*
gdk_ps_drawable_check_font(GdkFont* font, gint *size, gint *is_ps, gint *is_iso) {
	gchar* fname = NULL;
	static Atom pixel_size=None;
	static Atom adobe_ps1=None;
	static Atom adobe_ps2=None;
	static Atom dec_ps=None;
	static Atom face_name=None;
	static Atom weight=None;
	static Atom slant=None;
	static Atom xfont=None;
	GdkFontPrivate* fp;
	unsigned long value;
	gchar *pname=NULL;
	gchar* psname=NULL;
	gchar *result;
	gchar* slant_val, *weight_val;
	gint bold = 0;
	gint italic = 0;

	if ( is_iso )
		*is_iso = 1; /* check charset and encoding */
	
	fp = (GdkFontPrivate*)font;

	if ( slant == None )
		slant = XInternAtom(fp->xdisplay, "SLANT", True);
	if ( weight == None )
		weight = XInternAtom(fp->xdisplay, "WEIGHT_NAME", True);
	if ( pixel_size == None )
		pixel_size = XInternAtom(fp->xdisplay, "PIXEL_SIZE", True);
	if ( adobe_ps1 == None )
		adobe_ps1 = XInternAtom(fp->xdisplay, "_ADOBE_PSFONT", True);
	if ( adobe_ps2 == None )
		adobe_ps2 = XInternAtom(fp->xdisplay, "_ADOBE_POSTSCRIPT_FONTNAME", True);
	if ( dec_ps == None )
		dec_ps = XInternAtom(fp->xdisplay, "_DEC_DEVICE_FONTNAMES", True);
	if ( face_name == None )
		face_name = XInternAtom(fp->xdisplay, "FACE_NAME", True);

	if ( size ) {
		*size = 0;
		if ( pixel_size != None) {
			if (XGetFontProperty(fp->xfont, pixel_size, &value));
				*size = value;
		} 
		if (!*size)
			*size = font->ascent;
	}

	if ( adobe_ps1 != None ) {
		if (XGetFontProperty(fp->xfont, adobe_ps1, &value)) {
			psname = pname = XGetAtomName(fp->xdisplay, value);
		}
	} 
	if ( !psname && adobe_ps2 != None ) {
		if (XGetFontProperty(fp->xfont, adobe_ps2, &value)) {
			psname = pname = XGetAtomName(fp->xdisplay, value);
		}
	} 
	if ( !psname && dec_ps != None ) {
		if (XGetFontProperty(fp->xfont, dec_ps, &value)) {
			pname = XGetAtomName(fp->xdisplay, value);
			if ( pname && (psname=strstr(pname, "PS=")) ) {
				gchar *end;
				psname = psname+3;
				for (end = psname+3; isalnum(*end) || *end == '-' ; ++end);
				*end = 0;
			}
		}
	}
	if ( !psname && face_name != None ) {
		if (XGetFontProperty(fp->xfont, face_name, &value)) {
			gint i;
			pname = XGetAtomName(fp->xdisplay, value);
			/* FACE_NAME can have garbage at the end (freefont): X server bug? */
			for (i=0; pname[i]; ++i) /* FIXME: Need better font guessing! */
				if (pname[i] == ' ')
					pname[i] = '-';
			psname = pname;
			if ( strlen(psname) > 29 )
				psname[29]=0;
		}
	}
	if (!psname) {
		if ( is_ps )
			*is_ps = 0;
		if (slant != None && XGetFontProperty(fp->xfont, slant, &value)) {
			slant_val = XGetAtomName(fp->xdisplay, value);
			if ( slant_val && (*slant_val == 'I' || *slant_val == 'O') )
				italic = 1;
			if (slant_val) XFree(slant_val);
		}
		if (weight != None && XGetFontProperty(fp->xfont, weight, &value)) {
			weight_val = XGetAtomName(fp->xdisplay, value);
			g_strdown(weight_val);
			if ( weight_val && (strstr(weight_val,"bold") ||
				 strstr(weight_val,"black") ||
				 strstr(weight_val,"heavy")) )
				bold = 1;
			if (weight_val) XFree(weight_val);
		}
		if ( bold && italic )
			fname = "Times-BoldItalic";
		else if ( bold )
			fname = "Times-Bold";
		else if ( italic )
			fname = "Times-Italic";
		else
			fname = "Times-Roman";
	}
	result = g_strdup(psname?psname:fname);
	if (pname)
		XFree(pname);
	return result;
}

static void
ps_draw_text(GdkDrawable* w, GdkFont* font, GdkGC* gc, gint x, gint y, const gchar* text, gint length) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);
	gint xo = d->xoff;
	gint yo = d->yoff;

	if ( d->inframe || d->intile ) 
		xo = yo = 0;

	x += xo;
	y += yo;

	/* set font in gc? */
	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));

	ps_out_font(d, font, gc);

	ps_out_str(d, text, length);
	ps_out_num(d, (gfloat)x);
	ps_out_num(d, (gfloat)y);
	ps_out_text(d, " T\n");
}

static void
ps_draw_string(GdkDrawable* d, GdkFont* font, GdkGC* gc, gint x, gint y, const gchar* text) {
	ps_draw_text(d, font, gc, x, y, text, strlen(text));
}

static void
ps_draw_pixmap(GdkDrawable* win, GdkGC* gc, GdkDrawable* src, gint xs, gint ys, gint xd, gint yd, gint w, gint h) {
	GdkImage* im;
	gchar * data;
	GdkColormap * cmap;
	gint bpp;
	/* FIXME: check drawable type */
	im = gdk_image_get(src, xs, ys, w, h);
	cmap = ((GdkWindowPrivate*)win)->colormap;
	if (!cmap)
		cmap = gdk_colormap_get_system();
	data = get_image_data(im, cmap, &bpp, 0, 0, -1, -1);
	gdk_ps_drawable_draw_rgb (win, gc, data, bpp, xd, yd, w, h);
	gdk_image_destroy(im);
	g_free(data);
}

static void
ps_draw_image(GdkDrawable* win, GdkGC *gc, GdkImage* im, gint xs, gint ys, gint xd, gint yd, gint w, gint h) {
	gchar * data;
	GdkColormap * cmap;
	gint bpp;
	/* check colormap? */
	cmap = gdk_colormap_get_system();
	data = get_image_data(im, cmap, &bpp, xs, ys, w, h);
	gdk_ps_drawable_draw_rgb (win, gc, data, bpp, xd, yd, w, h);
	gdk_image_destroy(im);
	g_free(data);
}

static void
ps_draw_points(GdkDrawable* w, GdkGC* gc, GdkPoint* points, gint npoints) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));

	ps_out_points(d, npoints, points);
}

static void
ps_draw_segments(GdkDrawable* w, GdkGC* gc, GdkSegment* segs, gint nsegs) {
	GdkGCValues values;
	GdkPoint points[2];
	gint i;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);
	for (i = 0; i < nsegs; ++i) {
		points[0].x = segs[0].x1;
		points[0].y = segs[0].y1;
		points[1].x = segs[0].x2;
		points[1].y = segs[0].y2;
		ps_out_lines(d, 2, points);
	}
}

static void
ps_draw_lines(GdkPsDrawable* w, GdkGC* gc, GdkPoint* points, gint npoints) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_fgcolor(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);
	ps_out_lines(d, npoints, points);
}

static GdkDrawableClass gdk_ps_class = {
	0,
	"GdkPostscript",
	"A postscript drawable",
	NULL,
	ps_destroy,
	ps_draw_point,
	ps_draw_line,
	ps_draw_rectangle,
	ps_draw_arc,
	ps_draw_polygon,
	ps_draw_text,
	ps_draw_pixmap,
	ps_draw_image,
	ps_draw_points,
	ps_draw_segments,
	ps_draw_lines
};

GdkDrawable*
gdk_ps_drawable_new(gint fd, gchar* title, gchar* author) {
	GdkPsDrawable *d;
	GdkWindowPrivate *w;

	if ( !gdk_ps_class.type )
		gdk_drawable_register(&gdk_ps_class.type);
	
	d = g_new0(GdkPsDrawable, 1);
	w = g_new0(GdkWindowPrivate, 1);

	w->window.user_data = d;
	w->engine = &gdk_ps_class;
	w->window_type = GDK_WINDOW_DRAWABLE;

	d->fd = fd;
	d->font = NULL;
	d->sbuf = g_string_new(NULL);
	ps_out_invalidate(d);
	ps_out_begin(d, title, author);

	return (GdkDrawable*)w;
}

void 
gdk_ps_drawable_put_data (GdkDrawable *w, gchar *data, guint len) {
	GdkPsDrawable *d;

	g_return_if_fail(w != NULL);

	d = GDKPS(w);

	ps_out_flush(d, 1);
	g_string_append(d->sbuf, data);
	ps_out_flush(d, 1);
}

void 
gdk_ps_drawable_end (GdkDrawable *w) {
	GdkPsDrawable *d;

	g_return_if_fail(w != NULL);

	d = GDKPS(w);

	ps_out_end(d);
	ps_out_invalidate(d);
	g_free(d->rects);
}

void
gdk_ps_drawable_page_start (GdkDrawable *w, gint orientation, gint count, gint plex, gint resolution, gint width, gint height) {
	GdkPsDrawable *d;
	GdkWindowPrivate* wp;

	g_return_if_fail(w != NULL);

	d = GDKPS(w);
	wp = (GdkWindowPrivate*)w;

	wp->width = width;
	wp->height = height;
	
	ps_out_page(w, orientation, count, plex, resolution, width, height);
}

void 
gdk_ps_drawable_page_end (GdkDrawable *w) {
	GdkPsDrawable *d;

	g_return_if_fail(w != NULL);

	d = GDKPS(w);
	ps_out_page_end(d);
	ps_out_flush(d, 1);
}

static void
ps_destroy(GdkDrawable *w) {
	GdkPsDrawable *d;

	d = GDKPS(w);
	ps_out_flush(d, 1);
	g_free(d);

}

void 
gdk_ps_drawable_draw_rgb (GdkDrawable *w, GdkGC *gc, gchar *data, gint bpp, gint x, gint y, gint width, gint height) {
	GdkPsDrawable *d;
	char buf[128];
	int pos, total, i;
	
	d = GDKPS(w);
	sprintf(buf, " %d %d %d %d %d %d %s\n", x, y, width, height, width, height, bpp==1?"Im1":"Im24");
	ps_out_text(d, buf);
	total = bpp * width * height;

/*
	g_warning("total %d", total);
*/

	pos = 0;
	
	for (i=0; i < total; ++i) {
		sprintf(buf+pos, "%02x", data[i]);
		pos += 2;
		if ( pos == 76 ) {
			buf[pos++] = '\n';
			buf[pos++] = 0;
			ps_out_text(d, buf);
			pos = 0;
		}
	}

	if ( pos ) {
		buf[pos++] = '\n';
		buf[pos++] = 0;
		ps_out_text(d, buf);
	}
	
	ps_out_text(d, " gr\n");
}

