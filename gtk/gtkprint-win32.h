/* GTK - The GIMP Toolkit
 * gtkprint-win32.h: Win32 Print utils
 * Copyright (C) 2006, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_PRINT_WIN32_H__
#define __GTK_PRINT_WIN32_H__

#include "win32/gdkwin32.h"

G_BEGIN_DECLS

#ifndef START_PAGE_GENERAL
#define START_PAGE_GENERAL 0xffffffff
#endif

#ifndef PD_RESULT_CANCEL
#define PD_RESULT_CANCEL  0
#define PD_RESULT_PRINT  1
#define PD_RESULT_APPLY  2
#endif

#ifndef PD_NOCURRENTPAGE
#define PD_NOCURRENTPAGE  0x00800000
#endif

#ifndef PD_CURRENTPAGE
#define PD_CURRENTPAGE  0x00400000
#endif

typedef struct {
  char *driver;
  char *device;
  char *output;
  int flags;
} GtkPrintWin32Devnames;

void gtk_print_win32_devnames_free (GtkPrintWin32Devnames *devnames);
GtkPrintWin32Devnames *gtk_print_win32_devnames_from_win32 (HGLOBAL global);
GtkPrintWin32Devnames *gtk_print_win32_devnames_from_printer_name (const char *printer);
HGLOBAL gtk_print_win32_devnames_to_win32 (const GtkPrintWin32Devnames *devnames);
HGLOBAL gtk_print_win32_devnames_to_win32_from_printer_name (const char *printer);

G_END_DECLS

#endif /* __GTK_PRINT_WIN32_H__ */
