#include "gdk/gdk.h"
#include "gdk/gdkprivate.h"
#include "gdk/gdkx.h"
#include "gdk/gdkps.h"

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define GDKPS(w) (GdkPsDrawable*)((GdkWindowPrivate*)w)->window.user_data

static void ps_destroy (GdkDrawable *d);
static void ps_out_num (GdkPsDrawable *d, gfloat f);
static void ps_out_int (GdkPsDrawable *d, gint f);
static void ps_out_flush (GdkPsDrawable *d, gint force);
static void ps_out_text(GdkPsDrawable *d, gchar* text);
static void ps_out_invalidate(GdkPsDrawable *d);

static void ps_out_invalidate(GdkPsDrawable *d) {
	if ( d->font )
		gdk_font_unref(d->font);
	d->font = NULL;
	d->valid = 0;
	d->valid_fg = 0;
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
ps_out_begin(GdkPsDrawable *d, gchar* title, gchar* author) {
	time_t date;

	date = time(NULL);
	
	ps_out_text(d, "%!PS-Adobe-3.0 EPSF-3.0\n");
	ps_out_text(d, "%%Creator: Gdk PostScript Print Driver\n");
	ps_out_text(d, "%%DocumentMedia: A4 595 842 80 white {}\n"); /* FIXME */
	ps_out_text(d, "%%CreationDate: ");
	ps_out_text(d, ctime(&date));
	if (title ) {
		ps_out_text(d, "%%Title: ");
		ps_out_text(d, title?title:"");
		ps_out_text(d, "\n");
	}
	if ( author ) {
		ps_out_text(d, "%%CreatedFor: ");
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
	GdkWindowPrivate* wp = (GdkWindowPrivate*)w;

	/*wp->width = wd;
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
	if (d->valid_fg && color->red == d->fg.red && color->green == d->fg.green 
			&& color->blue == d->fg.blue )
		return;
	if (color->green == color->red && color->green == color->blue) {
		ps_out_num(d, color->green/65535.0);
		ps_out_text(d, " g\n");
	} else {
		ps_out_num(d, color->red/65535.0);
		ps_out_num(d, color->green/65535.0);
		ps_out_num(d, color->blue/65535.0);
		ps_out_text(d, " sc\n");
	}
	d->fg = *color;
	d->valid_fg = 1;
}

static void
ps_out_fill(GdkPsDrawable *d, GdkGCValues* v) {
	/* FIXME */
	d->valid_fg = 0;
	if ( v->fill == GDK_SOLID ) {
		ps_out_color(d, &(v->foreground));
	} else if ( v->fill == GDK_TILED ) {
	} else if ( v->fill == GDK_STIPPLED ) {
	} else if ( v->fill == GDK_OPAQUE_STIPPLED ) {
	}
}

static void
ps_out_line_attrs(GdkPsDrawable *d, GdkGC *gc, GdkGCValues* v) {
	gint cap[] = {0, 0, 1, 2};
	gint join[] = {0, 1, 2};

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
	d->valid = 1;
	/* FIXME: dashes */
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
		if ( i == 0 ) 
			ps_out_text(d, " m");
		else
			ps_out_text(d, " l");
	}
	/* linebclr */
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
	ps_out_color(d, &(values.foreground));

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
	ps_out_color(d, &(values.foreground));
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
		ps_out_color(d, &(values.foreground));
	ps_out_line_attrs(d, gc, &values);

	ps_out_num(d, (gfloat)x);
	ps_out_num(d, (gfloat)y);
	ps_out_num(d, (gfloat)w);
	ps_out_num(d, (gfloat)h);
	ps_out_text(d, filled ? " R fl" : " R");
	/* lineBclr */
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
		ps_out_color(d, &(values.foreground));
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
		/* lineBclr */
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
		ps_out_color(d, &(values.foreground));
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
		/* lineBclr */
		ps_out_text(d, " st\n");
	} else {
		ps_out_text(d, " cp fl\n"); /* FIXME: fillrule? */
	}
}

static void
ps_out_font(GdkPsDrawable* d, GdkFont* font, GdkGC* gc) {
	gchar* fname="Times-Roman";
	Atom face;
	GdkFontPrivate* fp;
	unsigned long value;
	gchar *pname=NULL;
	gchar buf[128];
	gint iso=1;

	if ( d->font && !g_strcasecmp(d->font->name, font->name) )
		return;
	if ( d->font )
	  gdk_font_unref(d->font);
	d->font = font;
	gdk_font_ref(d->font);

	fp = (GdkFontPrivate*)font;

	face = XInternAtom(fp->xdisplay, "FACE_NAME", True);
	/* FIXME: check fontset */
	if ( face != None ) {
		if (XGetFontProperty(fp->xfont, face, &value)) {
			pname = XGetAtomName(fp->xdisplay, value);
		}
	}
	if (!pname)
		g_warning("Using a non-PS font");
	else {
		gint i;
		for (i=0; pname[i]; ++i) /* FIXME: Need better font guessing! */
			if (pname[i] == ' ')
				pname[i] = '-';
	}
	sprintf(buf, " /%s %d %c Tf\n", pname?pname:fname, 
		font->ascent+font->descent, iso?'t':'f');
	ps_out_text(d, buf);
	if (pname)
		XFree(pname);
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
	ps_out_color(d, &(values.foreground));

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
ps_draw_pixmap(GdkDrawable* w) {
	GdkPsDrawable* d = GDKPS(w);
	g_warning("Unimplemented ps_draw_pixmap");
}

static void
ps_draw_image(GdkDrawable* w) {
	GdkPsDrawable* d = GDKPS(w);
	g_warning("Unimplemented ps_draw_image");
}

static void
ps_draw_points(GdkDrawable* w, GdkGC* gc, GdkPoint* points, gint npoints) {
	GdkGCValues values;
	GdkPsDrawable* d = GDKPS(w);

	gdk_gc_get_values(gc, &values);
	update_gc(d, gc);
	ps_out_color(d, &(values.foreground));

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
	ps_out_color(d, &(values.foreground));
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
	ps_out_color(d, &(values.foreground));
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

	/*wp->width = width;
	wp->height = height;
	*/
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


