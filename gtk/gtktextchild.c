/* gtktextchild.c - child pixmaps and widgets
 * 
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 2000      Red Hat, Inc.
 * Tk -> Gtk port by Havoc Pennington <hp@redhat.com>
 *
 * This software is copyrighted by the Regents of the University of
 * California, Sun Microsystems, Inc., and other parties.  The
 * following terms apply to all files associated with the software
 * unless explicitly disclaimed in individual files.
 * 
 * The authors hereby grant permission to use, copy, modify,
 * distribute, and license this software and its documentation for any
 * purpose, provided that existing copyright notices are retained in
 * all copies and that this notice is included verbatim in any
 * distributions. No written agreement, license, or royalty fee is
 * required for any of the authorized uses.  Modifications to this
 * software may be copyrighted by their authors and need not follow
 * the licensing terms described here, provided that the new terms are
 * clearly indicated on the first page of each file where they apply.
 * 
 * IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION,
 * OR ANY DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 * NON-INFRINGEMENT.  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * AND THE AUTHORS AND DISTRIBUTORS HAVE NO OBLIGATION TO PROVIDE
 * MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * GOVERNMENT USE: If you are acquiring this software on behalf of the
 * U.S. government, the Government shall have only "Restricted Rights"
 * in the software and related documentation as defined in the Federal
 * Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
 * are acquiring the software on behalf of the Department of Defense,
 * the software shall be classified as "Commercial Computer Software"
 * and the Government shall have only "Restricted Rights" as defined
 * in Clause 252.227-7013 (c) (1) of DFARs.  Notwithstanding the
 * foregoing, the authors grant the U.S. Government and others acting
 * in its behalf permission to use and distribute the software in
 * accordance with the terms specified in this license.
 * 
 */

#include "gtktextchild.h"
#include "gtktextbtree.h"

static GtkTextLineSegment *
pixmap_segment_cleanup_func(GtkTextLineSegment *seg,
                            GtkTextLine *line)
{
  /* nothing */
  return seg;
}

static int
pixmap_segment_delete_func(GtkTextLineSegment *seg,
                           GtkTextLine *line,
                           gboolean tree_gone)
{
  if (seg->body.pixmap.pixmap)
    gdk_pixmap_unref(seg->body.pixmap.pixmap);

  if (seg->body.pixmap.mask)
    gdk_bitmap_unref(seg->body.pixmap.mask);

  g_free(seg);

  return 0;
}

static void
pixmap_segment_check_func(GtkTextLineSegment *seg,
                          GtkTextLine *line)
{
  if (seg->next == NULL)
    g_error("pixmap segment is the last segment in a line");

  if (seg->byte_count != 3)
    g_error("pixmap segment has byte count of %d", seg->byte_count);

  if (seg->char_count != 1)
    g_error("pixmap segment has char count of %d", seg->char_count);  
}


GtkTextLineSegmentClass gtk_text_pixmap_type = {
  "pixmap",                          /* name */
  0,                                            /* leftGravity */
  NULL,                                          /* splitFunc */
  pixmap_segment_delete_func,                             /* deleteFunc */
  pixmap_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                    /* lineChangeFunc */
  pixmap_segment_check_func                               /* checkFunc */

};

#if 0
GtkTextLineSegmentClass gtk_text_view_child_type = {
  "child-widget",                                       /* name */
  0,                                            /* leftGravity */
  child_segment_split_func,                              /* splitFunc */
  child_segment_delete_func,                             /* deleteFunc */
  child_segment_cleanup_func,                            /* cleanupFunc */
  NULL,                                                    /* lineChangeFunc */
  child_segment_check_func                               /* checkFunc */
};
#endif

#define PIXMAP_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET(GtkTextLineSegment, body) \
	+ sizeof(GtkTextPixmap)))

GtkTextLineSegment *
gtk_text_pixmap_segment_new(GdkPixmap *pixmap, GdkBitmap *mask)
{
  GtkTextLineSegment *seg;

  seg = g_malloc(PIXMAP_SEG_SIZE);

  seg->type = &gtk_text_pixmap_type;

  seg->next = NULL;

  seg->byte_count = 3; /* We convert to the 0xFFFD "unknown character",
                          a 3-byte sequence in UTF-8 */
  seg->char_count = 1;
  
  seg->body.pixmap.pixmap = pixmap;
  seg->body.pixmap.mask = mask;

  if (pixmap)
    gdk_pixmap_ref(pixmap);

  if (mask)
    gdk_bitmap_ref(mask);
  
  return seg;
}

#if 0

/*
 * The following structure is the official type record for the
 * embedded window geometry manager:
 */

static void		EmbWinRequestFunc _ANSI_ARGS_((gpointer clientData,
			    Tk_Window tkwin));
static void		EmbWinLostSlaveFunc _ANSI_ARGS_((gpointer clientData,
			    Tk_Window tkwin));

static Tk_GeomMgr textGeomType = {
    "text",			/* name */
    EmbWinRequestFunc,		/* requestFunc */
    EmbWinLostSlaveFunc,	/* lostSlaveFunc */
};

/*
 * Definitions for alignment values:
 */

#define ALIGN_BOTTOM		0
#define ALIGN_CENTER		1
#define ALIGN_TOP		2
#define ALIGN_BASELINE		3

/*
 * Macro that determines the size of an embedded window segment:
 */

#define EW_SEG_SIZE ((unsigned) (G_STRUCT_OFFSET(GtkTextLineSegment, body) \
	+ sizeof(GtkTextEmbWindow)))

/*
 * Prototypes for procedures defined in this file:
 */

static int		AlignParseFunc _ANSI_ARGS_((gpointer clientData,
			    Tcl_Interp *interp, Tk_Window tkwin, char *value,
			    char *widgRec, int offset));
static char *		AlignPrintFunc _ANSI_ARGS_((gpointer clientData,
			    Tk_Window tkwin, char *widgRec, int offset,
			    Tcl_FreeFunc **freeFuncPtr));
static GtkTextLineSegment *	EmbWinCleanupFunc _ANSI_ARGS_((GtkTextLineSegment *segPtr,
			    GtkTextLine *line));
static void		EmbWinCheckFunc _ANSI_ARGS_((GtkTextLineSegment *segPtr,
			    GtkTextLine *line));
static void		EmbWinBboxFunc _ANSI_ARGS_((GtkTextDisplayChunk *chunkPtr,
			    int index, int y, int lineHeight, int baseline,
			    int *xPtr, int *yPtr, int *widthPtr,
			    int *heightPtr));
static int		EmbWinConfigure _ANSI_ARGS_((GtkTextView *tkxt,
			    GtkTextLineSegment *ewPtr, int argc, char **argv));
static void		EmbWinDelayedUnmap _ANSI_ARGS_((
			    gpointer clientData));
static int		EmbWinDeleteFunc _ANSI_ARGS_((GtkTextLineSegment *segPtr,
			    GtkTextLine *line, int treeGone));
static void		EmbWinDisplayFunc _ANSI_ARGS_((
			    GtkTextDisplayChunk *chunkPtr, int x, int y,
			    int lineHeight, int baseline, Display *display,
			    Drawable dst, int screenY));
static int		EmbWinLayoutFunc _ANSI_ARGS_((GtkTextView *tkxt,
			    GtkTextIndex *indexPtr, GtkTextLineSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, GtkWrapMode wrapMode,
			    GtkTextDisplayChunk *chunkPtr));
static void		EmbWinStructureFunc _ANSI_ARGS_((gpointer clientData,
			    XEvent *eventPtr));
static void		EmbWinUndisplayFunc _ANSI_ARGS_((GtkTextView *tkxt,
			    GtkTextDisplayChunk *chunkPtr));

/*
 * The following structure declares the "embedded window" segment type.
 */

static GtkTextLineSegmentClass tkTextEmbWindowType = {
    "window",					/* name */
    0,						/* leftGravity */
    (GtkTextLineSegmentSplitFunc *) NULL,			/* splitFunc */
    EmbWinDeleteFunc,				/* deleteFunc */
    EmbWinCleanupFunc,				/* cleanupFunc */
    (GtkTextLineSegmentLineChangeFunc *) NULL,		/* lineChangeFunc */
    EmbWinLayoutFunc,				/* layoutFunc */
    EmbWinCheckFunc				/* checkFunc */
};

/*
 * Information used for parsing window configuration options:
 */

static Tk_CustomOption alignOption = {AlignParseFunc, AlignPrintFunc,
	(gpointer) NULL};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_CUSTOM, "-align", (char *) NULL, (char *) NULL,
	"center", 0, TK_CONFIG_DONT_SET_DEFAULT, &alignOption},
    {TK_CONFIG_STRING, "-create", (char *) NULL, (char *) NULL,
	(char *) NULL, G_STRUCT_OFFSET(GtkTextEmbWindow, create),
	TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-padx", (char *) NULL, (char *) NULL,
	"0", G_STRUCT_OFFSET(GtkTextEmbWindow, padX),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-pady", (char *) NULL, (char *) NULL,
	"0", G_STRUCT_OFFSET(GtkTextEmbWindow, padY),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-stretch", (char *) NULL, (char *) NULL,
	"0", G_STRUCT_OFFSET(GtkTextEmbWindow, stretch),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_WINDOW, "-window", (char *) NULL, (char *) NULL,
	(char *) NULL, G_STRUCT_OFFSET(GtkTextEmbWindow, tkwin),
	TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 *--------------------------------------------------------------
 *
 * GtkTextViewWindowCmd --
 *
 *	This procedure implements the "window" widget command
 *	for text widgets.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	A standard Tcl result or error.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
GtkTextViewWindowCmd(tkxt, interp, argc, argv)
    GtkTextView *tkxt;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  Someone else has already
				 * parsed this command enough to know that
				 * argv[1] is "window". */
{
    size_t length;
    GtkTextLineSegment *ewPtr;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    length = strlen(argv[2]);
    if ((strncmp(argv[2], "cget", length) == 0) && (length >= 2)) {
	GtkTextIndex index;
	GtkTextLineSegment *ewPtr;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window cget index option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (gtk_text_btree_index_from_string(interp, tkxt, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	ewPtr = gtk_text_view_index_to_seg(&index, (int *) NULL);
	if (ewPtr->type != &tkTextEmbWindowType) {
	    Tcl_AppendResult(interp, "no embedded window at index \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, tkxt->tkwin, configSpecs,
		(char *) &ewPtr->body.ew, argv[4], 0);
    } else if ((strncmp(argv[2], "configure", length) == 0) && (length >= 2)) {
	GtkTextIndex index;
	GtkTextLineSegment *ewPtr;

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window configure index ?option value ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (gtk_text_btree_index_from_string(interp, tkxt, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	ewPtr = gtk_text_view_index_to_seg(&index, (int *) NULL);
	if (ewPtr->type != &tkTextEmbWindowType) {
	    Tcl_AppendResult(interp, "no embedded window at index \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    return Tk_ConfigureInfo(interp, tkxt->tkwin, configSpecs,
		    (char *) &ewPtr->body.ew, (char *) NULL, 0);
	} else if (argc == 5) {
	    return Tk_ConfigureInfo(interp, tkxt->tkwin, configSpecs,
		    (char *) &ewPtr->body.ew, argv[4], 0);
	} else {
	    gtk_text_buffer_need_redisplay(tree->buffer, &index, &index);
	    return EmbWinConfigure(tkxt, ewPtr, argc-4, argv+4);
	}
    } else if ((strncmp(argv[2], "create", length) == 0) && (length >= 2)) {
	GtkTextIndex index;
	int lineIndex;

	/*
	 * Add a new window.  Find where to put the new window, and
	 * mark that position for redisplay.
	 */

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window create index ?option value ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (gtk_text_btree_index_from_string(interp, tkxt, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * Don't allow insertions on the last (dummy) line of the text.
	 */
    
	lineIndex = gtk_text_btree_line_number(index.line);
	if (lineIndex == gtk_text_btree_num_lines(tkxt->tree)) {
	    lineIndex--;
	    gtk_text_btree_get_line_byte_index(tkxt->tree, lineIndex, G_MAXINT, &index);
	}

	/*
	 * Create the new window segment and initialize it.
	 */

	ewPtr = (GtkTextLineSegment *) g_malloc(EW_SEG_SIZE);
	ewPtr->type = &tkTextEmbWindowType;
	ewPtr->size = 1;
	ewPtr->body.ew.tkxt = tkxt;
	ewPtr->body.ew.line = NULL;
	ewPtr->body.ew.tkwin = NULL;
	ewPtr->body.ew.create = NULL;
	ewPtr->body.ew.align = ALIGN_CENTER;
	ewPtr->body.ew.padX = ewPtr->body.ew.padY = 0;
	ewPtr->body.ew.stretch = 0;
	ewPtr->body.ew.chunkCount = 0;
	ewPtr->body.ew.displayed = 0;

	/*
	 * Link the segment into the text widget, then configure it (delete
	 * it again if the configuration fails).
	 */

	gtk_text_buffer_need_redisplay(tkxt, &index, &index);
	gtk_text_btree_link_segment(ewPtr, &index);
	if (EmbWinConfigure(tkxt, ewPtr, argc-4, argv+4) != TCL_OK) {
	    GtkTextIndex index2;

	    gtk_text_view_index_forw_chars(&index, 1, &index2);
	    gtk_text_btree_delete_chars(&index, &index2);
	    return TCL_ERROR;
	}
    } else if (strncmp(argv[2], "names", length) == 0) {
	Tcl_HashSearch search;
	Tcl_HashEntry *hPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window names\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&tkxt->windowTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    Tcl_AppendElement(interp,
		    Tcl_GetHashKey(&tkxt->markTable, hPtr));
	}
    } else {
	Tcl_AppendResult(interp, "bad window option \"", argv[2],
		"\": must be cget, configure, create, or names",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinConfigure --
 *
 *	This procedure is called to handle configuration options
 *	for an embedded window, using an argc/argv list.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message..
 *
 * Side effects:
 *	Configuration information for the embedded window changes,
 *	such as alignment, stretching, or name of the embedded
 *	window.
 *
 *--------------------------------------------------------------
 */

static int
EmbWinConfigure(tkxt, ewPtr, argc, argv)
    GtkTextView *tkxt;		/* Information about text widget that
				 * contains embedded window. */
    GtkTextLineSegment *ewPtr;	/* Embedded window to be configured. */
    int argc;			/* Number of strings in argv. */
    char **argv;		/* Array of strings describing configuration
				 * options. */
{
    Tk_Window oldWindow;
    Tcl_HashEntry *hPtr;
    int new;

    oldWindow = ewPtr->body.ew.tkwin;
    if (Tk_ConfigureWidget(tkxt->interp, tkxt->tkwin, configSpecs,
	    argc, argv, (char *) &ewPtr->body.ew, TK_CONFIG_ARGV_ONLY)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (oldWindow != ewPtr->body.ew.tkwin) {
	if (oldWindow != NULL) {
	    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&tkxt->windowTable,
		    Tk_PathName(oldWindow)));
	    Tk_DeleteEventHandler(oldWindow, StructureNotifyMask,
		    EmbWinStructureFunc, (gpointer) ewPtr);
	    Tk_ManageGeometry(oldWindow, (Tk_GeomMgr *) NULL,
		    (gpointer) NULL);
	    if (tkxt->tkwin != Tk_Parent(oldWindow)) {
		Tk_UnmaintainGeometry(oldWindow, tkxt->tkwin);
	    } else {
		Tk_UnmapWindow(oldWindow);
	    }
	}
	if (ewPtr->body.ew.tkwin != NULL) {
	    Tk_Window ancestor, parent;

	    /*
	     * Make sure that the text is either the parent of the
	     * embedded window or a descendant of that parent.  Also,
	     * don't allow a top-level window to be managed inside
	     * a text.
	     */

	    parent = Tk_Parent(ewPtr->body.ew.tkwin);
	    for (ancestor = tkxt->tkwin; ;
		    ancestor = Tk_Parent(ancestor)) {
		if (ancestor == parent) {
		    break;
		}
		if (Tk_IsTopLevel(ancestor)) {
		    badMaster:
		    Tcl_AppendResult(tkxt->interp, "can't embed ",
			    Tk_PathName(ewPtr->body.ew.tkwin), " in ",
			    Tk_PathName(tkxt->tkwin), (char *) NULL);
		    ewPtr->body.ew.tkwin = NULL;
		    return TCL_ERROR;
		}
	    }
	    if (Tk_IsTopLevel(ewPtr->body.ew.tkwin)
		    || (ewPtr->body.ew.tkwin == tkxt->tkwin)) {
		goto badMaster;
	    }

	    /*
	     * Take over geometry management for the window, plus create
	     * an event handler to find out when it is deleted.
	     */

	    Tk_ManageGeometry(ewPtr->body.ew.tkwin, &textGeomType,
		    (gpointer) ewPtr);
	    Tk_CreateEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		    EmbWinStructureFunc, (gpointer) ewPtr);

	    /*
	     * Special trick!  Must enter into the hash table *after*
	     * calling Tk_ManageGeometry:  if the window was already managed
	     * elsewhere in this text, the Tk_ManageGeometry call will cause
	     * the entry to be removed, which could potentially lose the new
	     * entry.
	     */

	    hPtr = Tcl_CreateHashEntry(&tkxt->windowTable,
		    Tk_PathName(ewPtr->body.ew.tkwin), &new);
	    Tcl_SetHashValue(hPtr, ewPtr);

	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * AlignParseFunc --
 *
 *	This procedure is invoked by Tk_ConfigureWidget during
 *	option processing to handle "-align" options for embedded
 *	windows.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The alignment for the embedded window may change.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
AlignParseFunc(clientData, interp, tkwin, value, widgRec, offset)
    gpointer clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window for text widget. */
    char *value;			/* Value of option. */
    char *widgRec;			/* Pointer to GtkTextEmbWindow
					 * structure. */
    int offset;				/* Offset into item (ignored). */
{
    GtkTextEmbWindow *embPtr = (GtkTextEmbWindow *) widgRec;

    if (strcmp(value, "baseline") == 0) {
	embPtr->align = ALIGN_BASELINE;
    } else if (strcmp(value, "bottom") == 0) {
	embPtr->align = ALIGN_BOTTOM;
    } else if (strcmp(value, "center") == 0) {
	embPtr->align = ALIGN_CENTER;
    } else if (strcmp(value, "top") == 0) {
	embPtr->align = ALIGN_TOP;
    } else {
	Tcl_AppendResult(interp, "bad alignment \"", value,
		"\": must be baseline, bottom, center, or top",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * AlignPrintFunc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-align" configuration
 *	option for embedded windows.
 *
 * Results:
 *	The return value is a string describing the embedded
 *	window's current alignment.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
AlignPrintFunc(clientData, tkwin, widgRec, offset, freeFuncPtr)
    gpointer clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window for text widget. */
    char *widgRec;			/* Pointer to GtkTextEmbWindow
					 * structure. */
    int offset;				/* Ignored. */
    Tcl_FreeFunc **freeFuncPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    switch (((GtkTextEmbWindow *) widgRec)->align) {
	case ALIGN_BASELINE:
	    return "baseline";
	case ALIGN_BOTTOM:
	    return "bottom";
	case ALIGN_CENTER:
	    return "center";
	case ALIGN_TOP:
	    return "top";
	default:
	    return "??";
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinStructureFunc --
 *
 *	This procedure is invoked by the Tk event loop whenever
 *	StructureNotify events occur for a window that's embedded
 *	in a text widget.  This procedure's only purpose is to
 *	clean up when windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window segment, and
 *	the portion of the text is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinStructureFunc(clientData, eventPtr)
    gpointer clientData;	/* Pointer to record describing window item. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) clientData;
    GtkTextIndex index;

    if (eventPtr->type != DestroyNotify) {
	return;
    }

    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&ewPtr->body.ew.tkxt->windowTable,
	    Tk_PathName(ewPtr->body.ew.tkwin)));
    ewPtr->body.ew.tkwin = NULL;
    index.tree = ewPtr->body.ew.tkxt->tree;
    index.line = ewPtr->body.ew.line;
    index.byteIndex = gtk_text_line_segment_get_offset(ewPtr, ewPtr->body.ew.line);
    gtk_text_buffer_need_redisplay(ewPtr->body.ew.tkxt, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinRequestFunc --
 *
 *	This procedure is invoked whenever a window that's associated
 *	with a window canvas item changes its requested dimensions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size and location on the screen of the window may change,
 *	depending on the options specified for the window item.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
EmbWinRequestFunc(clientData, tkwin)
    gpointer clientData;		/* Pointer to record for window item. */
    Tk_Window tkwin;			/* Window that changed its desired
					 * size. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) clientData;
    GtkTextIndex index;

    index.tree = ewPtr->body.ew.tkxt->tree;
    index.line = ewPtr->body.ew.line;
    index.byteIndex = gtk_text_line_segment_get_offset(ewPtr, ewPtr->body.ew.line);
    gtk_text_buffer_need_redisplay(ewPtr->body.ew.tkxt, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinLostSlaveFunc --
 *
 *	This procedure is invoked by the Tk geometry manager when
 *	a slave window managed by a text widget is claimed away
 *	by another geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window segment, and
 *	the portion of the text is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinLostSlaveFunc(clientData, tkwin)
    gpointer clientData;	/* Pointer to record describing window item. */
    Tk_Window tkwin;		/* Window that was claimed away by another
				 * geometry manager. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) clientData;
    GtkTextIndex index;

    Tk_DeleteEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
	    EmbWinStructureFunc, (gpointer) ewPtr);
    Tcl_CancelIdleCall(EmbWinDelayedUnmap, (gpointer) ewPtr);
    if (ewPtr->body.ew.tkxt->tkwin != Tk_Parent(tkwin)) {
	Tk_UnmaintainGeometry(tkwin, ewPtr->body.ew.tkxt->tkwin);
    } else {
	Tk_UnmapWindow(tkwin);
    }
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&ewPtr->body.ew.tkxt->windowTable,
	    Tk_PathName(ewPtr->body.ew.tkwin)));
    ewPtr->body.ew.tkwin = NULL;
    index.tree = ewPtr->body.ew.tkxt->tree;
    index.line = ewPtr->body.ew.line;
    index.byteIndex = gtk_text_line_segment_get_offset(ewPtr, ewPtr->body.ew.line);
    gtk_text_buffer_need_redisplay(ewPtr->body.ew.tkxt, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDeleteFunc --
 *
 *	This procedure is invoked by the text B-tree code whenever
 *	an embedded window lies in a range of characters being deleted.
 *
 * Results:
 *	Returns 0 to indicate that the deletion has been accepted.
 *
 * Side effects:
 *	The embedded window is deleted, if it exists, and any resources
 *	associated with it are released.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
EmbWinDeleteFunc(ewPtr, line, treeGone)
    GtkTextLineSegment *ewPtr;		/* Segment being deleted. */
    GtkTextLine *line;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    Tcl_HashEntry *hPtr;

    if (ewPtr->body.ew.tkwin != NULL) {
	hPtr = Tcl_FindHashEntry(&ewPtr->body.ew.tkxt->windowTable,
		Tk_PathName(ewPtr->body.ew.tkwin));
	if (hPtr != NULL) {
	    /*
	     * (It's possible for there to be no hash table entry for this
	     * window, if an error occurred while creating the window segment
	     * but before the window got added to the table)
	     */

	    Tcl_DeleteHashEntry(hPtr);
	}

	/*
	 * Delete the event handler for the window before destroying
	 * the window, so that EmbWinStructureFunc doesn't get called
	 * (we'll already do everything that it would have done, and
	 * it will just get confused).
	 */

	Tk_DeleteEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		EmbWinStructureFunc, (gpointer) ewPtr);
	Tk_DestroyWindow(ewPtr->body.ew.tkwin);
    }
    Tcl_CancelIdleCall(EmbWinDelayedUnmap, (gpointer) ewPtr);
    Tk_FreeOptions(configSpecs, (char *) &ewPtr->body.ew,
	    ewPtr->body.ew.tkxt->display, 0);
    g_free((char *) ewPtr);
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinCleanupFunc --
 *
 *	This procedure is invoked by the B-tree code whenever a
 *	segment containing an embedded window is moved from one
 *	line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line field of the segment gets updated.
 *
 *--------------------------------------------------------------
 */

static GtkTextLineSegment *
EmbWinCleanupFunc(ewPtr, line)
    GtkTextLineSegment *ewPtr;		/* Mark segment that's being moved. */
    GtkTextLine *line;		/* Line that now contains segment. */
{
    ewPtr->body.ew.line = line;
    return ewPtr;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinLayoutFunc --
 *
 *	This procedure is the "layoutFunc" for embedded window
 *	segments.
 *
 * Results:
 *	1 is returned to indicate that the segment should be
 *	displayed.  The chunkPtr structure is filled in.
 *
 * Side effects:
 *	None, except for filling in chunkPtr.
 *
 *--------------------------------------------------------------
 */

	/*ARGSUSED*/
static int
EmbWinLayoutFunc(tkxt, indexPtr, ewPtr, offset, maxX, maxChars,
	noCharsYet, wrapMode, chunkPtr)
    GtkTextView *tkxt;		/* Text widget being layed out. */
    GtkTextIndex *indexPtr;	/* Identifies first character in chunk. */
    GtkTextLineSegment *ewPtr;	/* Segment corresponding to indexPtr. */
    int offset;			/* Offset within segPtr corresponding to
				 * indexPtr (always 0). */
    int maxX;			/* Chunk must not occupy pixels at this
				 * position or higher. */
    int maxChars;		/* Chunk must not include more than this
				 * many characters. */
    int noCharsYet;		/* Non-zero means no characters have been
				 * assigned to this line yet. */
    GtkWrapMode wrapMode;	/* Wrap mode to use for line: GTK_WRAPMODE_CHAR,
				 * GTK_WRAPMODE_NONE, or GTK_WRAPMODE_WORD. */
    GtkTextDisplayChunk *chunkPtr;
				/* Structure to fill in with information
				 * about this chunk.  The x field has already
				 * been set by the caller. */
{
    int width, height;

    if (offset != 0) {
	panic("Non-zero offset in EmbWinLayoutFunc");
    }

    if ((ewPtr->body.ew.tkwin == NULL) && (ewPtr->body.ew.create != NULL)) {
	int code, new;
	Tcl_DString name;
	Tk_Window ancestor;
	Tcl_HashEntry *hPtr;

	/*
	 * The window doesn't currently exist.  Create it by evaluating
	 * the creation script.  The script must return the window's
	 * path name:  look up that name to get back to the window
	 * token.  Then ourselves as the geometry manager for
	 * the window.
	 */

	code = Tcl_GlobalEval(tkxt->interp, ewPtr->body.ew.create);
	if (code != TCL_OK) {
	    createError:
	    Tcl_BackgroundError(tkxt->interp);
	    goto gotWindow;
	}
	Tcl_DStringInit(&name);
	Tcl_DStringAppend(&name, Tcl_GetStringResult(tkxt->interp), -1);
	Tcl_ResetResult(tkxt->interp);
	ewPtr->body.ew.tkwin = Tk_NameToWindow(tkxt->interp,
		Tcl_DStringValue(&name), tkxt->tkwin);
	if (ewPtr->body.ew.tkwin == NULL) {
	    goto createError;
	}
	for (ancestor = tkxt->tkwin; ;
		ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == Tk_Parent(ewPtr->body.ew.tkwin)) {
		break;
	    }
	    if (Tk_IsTopLevel(ancestor)) {
		badMaster:
		Tcl_AppendResult(tkxt->interp, "can't embed ",
			Tk_PathName(ewPtr->body.ew.tkwin), " relative to ",
			Tk_PathName(tkxt->tkwin), (char *) NULL);
		Tcl_BackgroundError(tkxt->interp);
		ewPtr->body.ew.tkwin = NULL;
		goto gotWindow;
	    }
	}
	if (Tk_IsTopLevel(ewPtr->body.ew.tkwin)
		|| (tkxt->tkwin == ewPtr->body.ew.tkwin)) {
	    goto badMaster;
	}
	Tk_ManageGeometry(ewPtr->body.ew.tkwin, &textGeomType,
		(gpointer) ewPtr);
	Tk_CreateEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		EmbWinStructureFunc, (gpointer) ewPtr);

	/*
	 * Special trick!  Must enter into the hash table *after*
	 * calling Tk_ManageGeometry:  if the window was already managed
	 * elsewhere in this text, the Tk_ManageGeometry call will cause
	 * the entry to be removed, which could potentially lose the new
	 * entry.
	 */

	hPtr = Tcl_CreateHashEntry(&tkxt->windowTable,
		Tk_PathName(ewPtr->body.ew.tkwin), &new);
	Tcl_SetHashValue(hPtr, ewPtr);
    }

    /*
     * See if there's room for this window on this line.
     */

    gotWindow:
    if (ewPtr->body.ew.tkwin == NULL) {
	width = 0;
	height = 0;
    } else {
	width = Tk_ReqWidth(ewPtr->body.ew.tkwin) + 2*ewPtr->body.ew.padX;
	height = Tk_ReqHeight(ewPtr->body.ew.tkwin) + 2*ewPtr->body.ew.padY;
    }
    if ((width > (maxX - chunkPtr->x))
	    && !noCharsYet && (tkxt->wrapMode != GTK_WRAPMODE_NONE)) {
	return 0;
    }

    /*
     * Fill in the chunk structure.
     */

    chunkPtr->displayFunc = EmbWinDisplayFunc;
    chunkPtr->undisplayFunc = EmbWinUndisplayFunc;
    chunkPtr->measureFunc = (GtkTextViewChunkMeasureFunc *) NULL;
    chunkPtr->bboxFunc = EmbWinBboxFunc;
    chunkPtr->numBytes = 1;
    if (ewPtr->body.ew.align == ALIGN_BASELINE) {
	chunkPtr->minAscent = height - ewPtr->body.ew.padY;
	chunkPtr->minDescent = ewPtr->body.ew.padY;
	chunkPtr->minHeight = 0;
    } else {
	chunkPtr->minAscent = 0;
	chunkPtr->minDescent = 0;
	chunkPtr->minHeight = height;
    }
    chunkPtr->width = width;
    chunkPtr->breakIndex = -1;
    chunkPtr->breakIndex = 1;
    chunkPtr->clientData = (gpointer) ewPtr;
    ewPtr->body.ew.chunkCount += 1;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinCheckFunc --
 *
 *	This procedure is invoked by the B-tree code to perform
 *	consistency checks on embedded windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The procedure panics if it detects anything wrong with
 *	the embedded window.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinCheckFunc(ewPtr, line)
    GtkTextLineSegment *ewPtr;		/* Segment to check. */
    GtkTextLine *line;		/* Line containing segment. */
{
    if (ewPtr->next == NULL) {
	panic("EmbWinCheckFunc: embedded window is last segment in line");
    }
    if (ewPtr->size != 1) {
	panic("EmbWinCheckFunc: embedded window has size %d", ewPtr->size);
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDisplayFunc --
 *
 *	This procedure is invoked by the text displaying code
 *	when it is time to actually draw an embedded window
 *	chunk on the screen.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The embedded window gets moved to the correct location
 *	and mapped onto the screen.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinDisplayFunc(chunkPtr, x, y, lineHeight, baseline, display, dst, screenY)
    GtkTextDisplayChunk *chunkPtr;		/* Chunk that is to be drawn. */
    int x;				/* X-position in dst at which to
					 * draw this chunk (differs from
					 * the x-position in the chunk because
					 * of scrolling). */
    int y;				/* Top of rectangular bounding box
					 * for line: tells where to draw this
					 * chunk in dst (x-position is in
					 * the chunk itself). */
    int lineHeight;			/* Total height of line. */
    int baseline;			/* Offset of baseline from y. */
    Display *display;			/* Display to use for drawing. */
    Drawable dst;			/* Pixmap or window in which to draw */
    int screenY;			/* Y-coordinate in text window that
					 * corresponds to y. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) chunkPtr->clientData;
    int lineX, windowX, windowY, width, height;
    Tk_Window tkwin;

    tkwin = ewPtr->body.ew.tkwin;
    if (tkwin == NULL) {
	return;
    }
    if ((x + chunkPtr->width) <= 0) {
	/*
	 * The window is off-screen;  just unmap it.
	 */

	if (ewPtr->body.ew.tkxt->tkwin != Tk_Parent(tkwin)) {
	    Tk_UnmaintainGeometry(tkwin, ewPtr->body.ew.tkxt->tkwin);
	} else {
	    Tk_UnmapWindow(tkwin);
	}
	return;
    }

    /*
     * Compute the window's location and size in the text widget, taking
     * into account the align and stretch values for the window.
     */

    EmbWinBboxFunc(chunkPtr, 0, screenY, lineHeight, baseline, &lineX,
	    &windowY, &width, &height);
    windowX = lineX - chunkPtr->x + x;

    if (ewPtr->body.ew.tkxt->tkwin == Tk_Parent(tkwin)) {
	if ((windowX != Tk_X(tkwin)) || (windowY != Tk_Y(tkwin))
		|| (Tk_ReqWidth(tkwin) != Tk_Width(tkwin))
		|| (height != Tk_Height(tkwin))) {
	    Tk_MoveResizeWindow(tkwin, windowX, windowY, width, height);
	}
	Tk_MapWindow(tkwin);
    } else {
	Tk_MaintainGeometry(tkwin, ewPtr->body.ew.tkxt->tkwin,
		windowX, windowY, width, height);
    }

    /*
     * Mark the window as displayed so that it won't get unmapped.
     */

    ewPtr->body.ew.displayed = 1;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinUndisplayFunc --
 *
 *	This procedure is called when the chunk for an embedded
 *	window is no longer going to be displayed.  It arranges
 *	for the window associated with the chunk to be unmapped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is scheduled for unmapping.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinUndisplayFunc(tkxt, chunkPtr)
    GtkTextView *tkxt;			/* Overall information about text
					 * widget. */
    GtkTextDisplayChunk *chunkPtr;		/* Chunk that is about to be freed. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) chunkPtr->clientData;

    ewPtr->body.ew.chunkCount--;
    if (ewPtr->body.ew.chunkCount == 0) {
	/*
	 * Don't unmap the window immediately, since there's a good chance
	 * that it will immediately be redisplayed, perhaps even in the
	 * same place.  Instead, schedule the window to be unmapped later;
	 * the call to EmbWinDelayedUnmap will be cancelled in the likely
	 * event that the unmap becomes unnecessary.
	 */

	ewPtr->body.ew.displayed = 0;
	Tcl_DoWhenIdle(EmbWinDelayedUnmap, (gpointer) ewPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinBboxFunc --
 *
 *	This procedure is called to compute the bounding box of
 *	the area occupied by an embedded window.
 *
 * Results:
 *	There is no return value.  *xPtr and *yPtr are filled in
 *	with the coordinates of the upper left corner of the
 *	window, and *widthPtr and *heightPtr are filled in with
 *	the dimensions of the window in pixels.  Note:  not all
 *	of the returned bbox is necessarily visible on the screen
 *	(the rightmost part might be off-screen to the right,
 *	and the bottommost part might be off-screen to the bottom).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinBboxFunc(chunkPtr, index, y, lineHeight, baseline, xPtr, yPtr,
	widthPtr, heightPtr)
    GtkTextDisplayChunk *chunkPtr;		/* Chunk containing desired char. */
    int index;				/* Index of desired character within
					 * the chunk. */
    int y;				/* Topmost pixel in area allocated
					 * for this line. */
    int lineHeight;			/* Total height of line. */
    int baseline;			/* Location of line's baseline, in
					 * pixels measured down from y. */
    int *xPtr, *yPtr;			/* Gets filled in with coords of
					 * character's upper-left pixel. */
    int *widthPtr;			/* Gets filled in with width of
					 * character, in pixels. */
    int *heightPtr;			/* Gets filled in with height of
					 * character, in pixels. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) chunkPtr->clientData;
    Tk_Window tkwin;

    tkwin = ewPtr->body.ew.tkwin;
    if (tkwin != NULL) {
	*widthPtr = Tk_ReqWidth(tkwin);
	*heightPtr = Tk_ReqHeight(tkwin);
    } else {
	*widthPtr = 0;
	*heightPtr = 0;
    }
    *xPtr = chunkPtr->x + ewPtr->body.ew.padX;
    if (ewPtr->body.ew.stretch) {
	if (ewPtr->body.ew.align == ALIGN_BASELINE) {
	    *heightPtr = baseline - ewPtr->body.ew.padY;
	} else {
	    *heightPtr = lineHeight - 2*ewPtr->body.ew.padY;
	}
    }
    switch (ewPtr->body.ew.align) {
	case ALIGN_BOTTOM:
	    *yPtr = y + (lineHeight - *heightPtr - ewPtr->body.ew.padY);
	    break;
	case ALIGN_CENTER:
	    *yPtr = y + (lineHeight - *heightPtr)/2;
	    break;
	case ALIGN_TOP:
	    *yPtr = y + ewPtr->body.ew.padY;
	    break;
	case ALIGN_BASELINE:
	    *yPtr = y + (baseline - *heightPtr);
	    break;
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDelayedUnmap --
 *
 *	This procedure is an idle handler that does the actual
 *	work of unmapping an embedded window.  See the comment
 *	in EmbWinUndisplayFunc for details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window gets unmapped, unless its chunk reference count
 *	has become non-zero again.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinDelayedUnmap(clientData)
    gpointer clientData;		/* Token for the window to
					 * be unmapped. */
{
    GtkTextLineSegment *ewPtr = (GtkTextLineSegment *) clientData;

    if (!ewPtr->body.ew.displayed && (ewPtr->body.ew.tkwin != NULL)) {
	if (ewPtr->body.ew.tkxt->tkwin != Tk_Parent(ewPtr->body.ew.tkwin)) {
	    Tk_UnmaintainGeometry(ewPtr->body.ew.tkwin,
		    ewPtr->body.ew.tkxt->tkwin);
	} else {
	    Tk_UnmapWindow(ewPtr->body.ew.tkwin);
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * GtkTextViewWindowIndex --
 *
 *	Given the name of an embedded window within a text widget,
 *	returns an index corresponding to the window's position
 *	in the text.
 *
 * Results:
 *	The return value is 1 if there is an embedded window by
 *	the given name in the text widget, 0 otherwise.  If the
 *	window exists, *indexPtr is filled in with its index.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
GtkTextViewWindowIndex(tkxt, name, indexPtr)
    GtkTextView *tkxt;		/* Text widget containing window. */
    char *name;			/* Name of window. */
    GtkTextIndex *indexPtr;	/* Index information gets stored here. */
{
    Tcl_HashEntry *hPtr;
    GtkTextLineSegment *ewPtr;

    hPtr = Tcl_FindHashEntry(&tkxt->windowTable, name);
    if (hPtr == NULL) {
	return 0;
    }
    ewPtr = (GtkTextLineSegment *) Tcl_GetHashValue(hPtr);
    indexPtr->tree = tkxt->tree;
    indexPtr->line = ewPtr->body.ew.line;
    indexPtr->byteIndex = gtk_text_line_segment_get_offset(ewPtr, indexPtr->line);
    return 1;
}
#endif
