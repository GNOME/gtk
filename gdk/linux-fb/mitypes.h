#ifndef MITYPES_H
#define MITYPES_H

#include <alloca.h>

#define ALLOCATE_LOCAL(size) alloca((int)(size))
#define DEALLOCATE_LOCAL(ptr)  /* as nothing */

#include <stdlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdkfb.h>
#include <gdkprivate-fb.h>
#include <gdkregion-generic.h>

typedef struct _miDash *miDashPtr;

#define CT_NONE                 0
#define CT_PIXMAP               1
#define CT_REGION               2
#define CT_UNSORTED             6
#define CT_YSORTED              10
#define CT_YXSORTED             14
#define CT_YXBANDED             18

struct _GdkGCFuncs {
  void        (* ValidateGC)(
			     GdkGC* /*pGC*/,
			     unsigned long /*stateChanges*/,
			     GdkDrawable* /*pDrawable*/
			     );

  void        (* ChangeGC)(
			   GdkGC* /*pGC*/,
			   unsigned long /*mask*/
			   );

  void        (* CopyGC)(
			 GdkGC* /*pGCSrc*/,
			 unsigned long /*mask*/,
			 GdkGC* /*pGCDst*/
			 );

  void        (* DestroyGC)(
			    GdkGC* /*pGC*/
			    );

  void        (* ChangeClip)(
			     GdkGC* /*pGC*/,
			     int /*type*/,
			     gpointer /*pvalue*/,
			     int /*nrects*/
			     );
  void        (* DestroyClip)(
			      GdkGC* /*pGC*/
			      );

  void        (* CopyClip)(
			   GdkGC* /*pgcDst*/,
			   GdkGC* /*pgcSrc*/
			   );

};

typedef union {
  guint32 val;
  gpointer ptr;
} ChangeGCVal, *ChangeGCValPtr;

#define PixmapBytePad(w, d) (w)
#define BitmapBytePad(w) (w)

#if 0
typedef struct _PaddingInfo {
        int     padRoundUp;     /* pixels per pad unit - 1 */
        int     padPixelsLog2;  /* log 2 (pixels per pad unit) */
        int     padBytesLog2;   /* log 2 (bytes per pad unit) */
        int     notPower2;      /* bitsPerPixel not a power of 2 */
        int     bytesPerPixel;  /* only set when notPower2 is TRUE */
} PaddingInfo;
extern PaddingInfo PixmapWidthPaddingInfo[];

#define PixmapWidthInPadUnits(w, d) \
    (PixmapWidthPaddingInfo[d].notPower2 ? \
    (((int)(w) * PixmapWidthPaddingInfo[d].bytesPerPixel +  \
                 PixmapWidthPaddingInfo[d].bytesPerPixel) >> \
        PixmapWidthPaddingInfo[d].padBytesLog2) : \
    ((int)((w) + PixmapWidthPaddingInfo[d].padRoundUp) >> \
        PixmapWidthPaddingInfo[d].padPixelsLog2))

#define PixmapBytePad(w, d) \
    (PixmapWidthInPadUnits(w, d) << PixmapWidthPaddingInfo[d].padBytesLog2)

#define BitmapBytePad(w) \
    (((int)((w) + BITMAP_SCANLINE_PAD - 1) >> LOG2_BITMAP_PAD) << LOG2_BYTES_PER_SCANLINE_PAD)

typedef struct _GdkDrawable GdkPixmap, *GdkPixmap*;
typedef struct _GdkDrawable GdkDrawable, *GdkDrawable*;
typedef struct _GdkGCOps GdkGCOps;
typedef struct _Region Region, *GdkRegion*;
  
#define EVEN_DASH	0
#define ODD_DASH	~0

typedef struct _GdkPoint {
    gint16		x, y;
} GdkPoint, *GdkPoint*;

typedef struct _GdkGC {
    unsigned char       depth;    
    unsigned char       alu;
    unsigned short      line_width;          
    unsigned short      dashOffset;
    unsigned short      numInDashList;
    unsigned char       *dash;
    unsigned int        lineStyle : 2;
    unsigned int        capStyle : 2;
    unsigned int        joinStyle : 2;
    unsigned int        fillStyle : 2;
    unsigned int        fillRule : 1;
    unsigned int        arcMode : 1;
    unsigned int        subWindowMode : 1;
    unsigned int        graphicsExposures : 1;
    unsigned int        clientClipType : 2; /* CT_<kind> */
    unsigned int        miTranslate:1; /* should mi things translate? */
    unsigned int        tileIsPixel:1; /* tile is solid pixel */
    unsigned int        fExpose:1;     /* Call exposure handling */
    unsigned int        freeCompClip:1;  /* Free composite clip */
    unsigned int        unused:14; /* see comment above */
    unsigned long       planemask;
    unsigned long       fgPixel;
    unsigned long       bgPixel;
    /*
     * alas -- both tile and stipple must be here as they
     * are independently specifiable
     */
  /*    PixUnion            tile;
	GdkPixmap*           stipple;*/
    GdkPoint             patOrg;         /* origin for (tile, stipple) */
    struct _Font        *font;
    GdkPoint             clipOrg;
    GdkPoint             lastWinOrg;     /* position of window last validated */
    gpointer            clientClip;
    unsigned long       stateChanges;   /* masked with GC_<kind> */
    unsigned long       serialNumber;
  /*    GCFuncs             *funcs; */
    GdkGCOps             *ops;
} GdkGC, *GdkGC*;

struct _GdkDrawable {
    unsigned char	type;	/* DRAWABLE_<type> */
    unsigned char	depth;
    unsigned char	bitsPerPixel;
    short		x;	/* window: screen absolute, pixmap: 0 */
    short		y;	/* window: screen absolute, pixmap: 0 */
    unsigned short	width;
    unsigned short	height;
    unsigned long	serialNumber;

    guchar             *buffer;
    int                 rowStride;                 
};

typedef struct _GdkSegment {
    gint16 x1, y1, x2, y2;
} GdkSegment;

typedef struct _GdkRectangle {
    gint16 x, y;
    guint16  width, height;
} GdkRectangle, *GdkRectangle*;

typedef struct _Box {
    short x1, y1, x2, y2;
} BoxRec, *BoxPtr;

/*
 * graphics operations invoked through a GC
 */

/* graphics functions, as in GC.alu */

#define GDK_CLEAR                 0x0             /* 0 */
#define GDK_AND                   0x1             /* src AND dst */
#define GDK_AND_REVERSE            0x2             /* src AND NOT dst */
#define GDK_COPY                  0x3             /* src */
#define GDK_AND_INVERT           0x4             /* NOT src AND dst */
#define GDK_NOOP                  0x5             /* dst */
#define GDK_XOR                   0x6             /* src XOR dst */
#define GDK_OR                    0x7             /* src OR dst */
#define GDK_NOR                   0x8             /* NOT src AND NOT dst */
#define GDK_EQUIV                 0x9             /* NOT src XOR dst */
#define GDK_INVERT                0xa             /* NOT dst */
#define GDK_OR_REVERSE             0xb             /* src OR NOT dst */
#define GDK_COPY_INVERT          0xc             /* NOT src */
#define GDK_OR_INVERT            0xd             /* NOT src OR dst */
#define GDK_NAND                  0xe             /* NOT src OR NOT dst */
#define GDK_SET                   0xf             /* 1 */

/* CoordinateMode for drawing routines */

#define CoordModeOrigin         0       /* relative to the origin */
#define CoordModePrevious       1       /* relative to previous point */

/* Polygon shapes */

#define Complex                 0       /* paths may intersect */
#define Nonconvex               1       /* no paths intersect, but not convex */
#define Convex                  2       /* wholly convex */

/* LineStyle */

#define GDK_LINE_SOLID               0
#define GDK_LINE_ON_OFF_DASH           1
#define GDK_LINE_DOUBLE_DASH          2

/* capStyle */

#define GDK_CAP_NOT_LAST              0
#define GDK_CAP_BUTT                 1
#define GDK_CAP_ROUND                2
#define GDK_CAP_PROJECTING           3

/* joinStyle */

#define GDK_JOIN_MITER               0
#define GDK_JOIN_ROUND               1
#define GDK_JOIN_BEVEL               2

/* Arc modes for PolyFillArc */

#define ArcChord                0       /* join endpoints of arc */
#define ArcPieSlice             1       /* join endpoints to center of arc */

/* fillRule */

#define EvenOddRule             0
#define WindingRule             1

/* fillStyle */

#define GDK_SOLID               0
#define GDK_TILED               1
#define GDK_STIPPLED            2
#define GDK_OPAQUE_STIPPLED      3

typedef enum {  Linear8Bit, TwoD8Bit, Linear16Bit, TwoD16Bit }  FontEncoding;

#define MININT G_MININT
#define MAXINT G_MAXINT
#define MINSHORT G_MINSHORT
#define MAXSHORT G_MAXSHORT

/* GC components: masks used in CreateGC, CopyGC, ChangeGC, OR'ed into
   GC.stateChanges */

#define GDK_GC_FUNCTION              (1L<<0)
#define GCPlaneMask             (1L<<1)
#define GDK_GC_FOREGROUND            (1L<<2)
#define GDK_GC_BACKGROUND            (1L<<3)
#define GDK_GC_LINE_WIDTH             (1L<<4)
#define GCLineStyle             (1L<<5)
#define GDK_GC_CAP_STYLE              (1L<<6)
#define GDK_GC_JOIN_STYLE             (1L<<7)
#define GDK_GC_FILL_STYLE             (1L<<8)
#define GCFillRule              (1L<<9) 
#define GCTile                  (1L<<10)
#define GDK_GC_STIPPLE               (1L<<11)
#define GDK_GC_TS_X_ORIGIN       (1L<<12)
#define GDK_GC_TS_Y_ORIGIN       (1L<<13)
#define GCFont                  (1L<<14)
#define GCSubwindowMode         (1L<<15)
#define GCGraphicsExposures     (1L<<16)
#define GCClipXOrigin           (1L<<17)
#define GCClipYOrigin           (1L<<18)
#define GCClipMask              (1L<<19)
#define GCDashOffset            (1L<<20)
#define GCDashList              (1L<<21)
#define GCArcMode               (1L<<22)

/* types for Drawable */

#define GDK_WINDOW_CHILD 0
#define GDK_DRAWABLE_PIXMAP 1
#define GDK_WINDOW_FOREIGN 2
#define GDK_WINDOW_TEMP 3

#endif

typedef GdkSegment BoxRec, *BoxPtr;

typedef struct _miArc {
    gint16 x, y;
    guint16   width, height;
    gint16   angle1, angle2;
} miArc;

struct _GdkGCOps {
    void	(* FillSpans)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*nInit*/,
		GdkPoint* /*pptInit*/,
		int * /*pwidthInit*/,
		int /*fSorted*/
);

    void	(* SetSpans)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		char * /*psrc*/,
		GdkPoint* /*ppt*/,
		int * /*pwidth*/,
		int /*nspans*/,
		int /*fSorted*/
);

    void	(* PutImage)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*depth*/,
		int /*x*/,
		int /*y*/,
		int /*w*/,
		int /*h*/,
		int /*leftPad*/,
		int /*format*/,
		char * /*pBits*/
);

    GdkRegion*	(* CopyArea)(
		GdkDrawable* /*pSrc*/,
		GdkDrawable* /*pDst*/,
		GdkGC* /*pGC*/,
		int /*srcx*/,
		int /*srcy*/,
		int /*w*/,
		int /*h*/,
		int /*dstx*/,
		int /*dsty*/
);

    GdkRegion*	(* CopyPlane)(
		GdkDrawable* /*pSrcDrawable*/,
		GdkDrawable* /*pDstDrawable*/,
		GdkGC* /*pGC*/,
		int /*srcx*/,
		int /*srcy*/,
		int /*width*/,
		int /*height*/,
		int /*dstx*/,
		int /*dsty*/,
		unsigned long /*bitPlane*/
);
    void	(* PolyPoint)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*mode*/,
		int /*npt*/,
		GdkPoint* /*pptInit*/
);

    void	(* Polylines)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*mode*/,
		int /*npt*/,
		GdkPoint* /*pptInit*/
);

    void	(* PolySegment)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*nseg*/,
		GdkSegment * /*pSegs*/
);

    void	(* PolyRectangle)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*nrects*/,
		GdkRectangle * /*pRects*/
);

    void	(* PolyArc)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*narcs*/,
		miArc * /*parcs*/
);

    void	(* FillPolygon)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*shape*/,
		int /*mode*/,
		int /*count*/,
		GdkPoint* /*pPts*/
);

    void	(* PolyFillRect)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*nrectFill*/,
		GdkRectangle * /*prectInit*/
);

    void	(* PolyFillArc)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*narcs*/,
		miArc * /*parcs*/
);

#if 0  
    int		(* PolyText8)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		int /*count*/,
		char * /*chars*/
);

    int		(* PolyText16)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		int /*count*/,
		unsigned short * /*chars*/
);

    void	(* ImageText8)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		int /*count*/,
		char * /*chars*/
);

    void	(* ImageText16)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		int /*count*/,
		unsigned short * /*chars*/
);

    void	(* ImageGlyphBlt)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		unsigned int /*nglyph*/,
		CharInfoPtr * /*ppci*/,
		pointer /*pglyphBase*/
);

    void	(* PolyGlyphBlt)(
		GdkDrawable* /*pDrawable*/,
		GdkGC* /*pGC*/,
		int /*x*/,
		int /*y*/,
		unsigned int /*nglyph*/,
		CharInfoPtr * /*ppci*/,
		pointer /*pglyphBase*/
);
#endif
  
    void	(* PushPixels)(
		GdkGC* /*pGC*/,
		GdkPixmap* /*pBitMap*/,
		GdkDrawable* /*pDst*/,
		int /*w*/,
		int /*h*/,
		int /*x*/,
		int /*y*/
);
};

#define SCRRIGHT(x, n) ((x)>>(n))

#endif /* MITYPES_H */
