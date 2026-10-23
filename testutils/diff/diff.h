#pragma once

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * Command line flags
 */
#define D_FOLDBLANKS	0x010	/* Treat all white space as equal */
#define D_MINIMAL	0x020	/* Make diff as small as possible */
#define D_PROTOTYPE	0x080	/* Display C function prototype */
#define D_EXPANDTABS	0x100	/* Expand tabs to spaces */
#define D_IGNOREBLANKS	0x200	/* Ignore white space changes */

/*
 * Status values for print_status() and diffreg() return values
 */
#define	D_SAME		0	/* Files are the same */
#define	D_DIFFER	1	/* Files are different */

int diffreg (const char     *filename,
             GInputStream *file1,
             GInputStream *file2,
             GOutputStream *out,
             int flags);

G_END_DECLS
