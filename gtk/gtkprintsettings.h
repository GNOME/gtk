/* EGG - The GIMP Toolkit
 * gtkprintsettings.h: Print Settings
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

#ifndef __GTK_PRINT_SETTINGS_H__
#define __GTK_PRINT_SETTINGS_H__

#include <glib-object.h>
#include "gtkpapersize.h"

G_BEGIN_DECLS

typedef struct _GtkPrintSettings GtkPrintSettings;

#define GTK_TYPE_PRINT_SETTINGS    (gtk_print_settings_get_type ())
#define GTK_PRINT_SETTINGS(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINT_SETTINGS, GtkPrintSettings))
#define GTK_IS_PRINT_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINT_SETTINGS))

typedef void  (*GtkPrintSettingsFunc)  (const char *key,
					const char *value,
					gpointer  user_data);

typedef struct {
  int start;
  int end;
} GtkPageRange;

GType             gtk_print_settings_get_type (void);
GtkPrintSettings *gtk_print_settings_new      (void);

GtkPrintSettings *gtk_print_settings_copy                    (GtkPrintSettings     *other);
gboolean          gtk_print_settings_has_key                 (GtkPrintSettings     *settings,
							      const char           *key);
const char *      gtk_print_settings_get                     (GtkPrintSettings     *settings,
							      const char           *key);
void              gtk_print_settings_set                     (GtkPrintSettings     *settings,
							      const char           *key,
							      const char           *value);
void              gtk_print_settings_unset                   (GtkPrintSettings     *settings,
							      const char           *key);
void              gtk_print_settings_foreach                 (GtkPrintSettings     *settings,
							      GtkPrintSettingsFunc  func,
							      gpointer              user_data);
gboolean          gtk_print_settings_get_bool                (GtkPrintSettings     *settings,
							      const char           *key);
void              gtk_print_settings_set_bool                (GtkPrintSettings     *settings,
							      const char           *key,
							      gboolean              value);
double            gtk_print_settings_get_double              (GtkPrintSettings     *settings,
							      const char           *key);
double            gtk_print_settings_get_double_with_default (GtkPrintSettings     *settings,
							      const char           *key,
							      double                def);
void              gtk_print_settings_set_double              (GtkPrintSettings     *settings,
							      const char           *key,
							      double                value);
double            gtk_print_settings_get_length              (GtkPrintSettings     *settings,
							      const char           *key,
							      GtkUnit               unit);
void              gtk_print_settings_set_length              (GtkPrintSettings     *settings,
							      const char           *key,
							      double                value,
							      GtkUnit               unit);
int               gtk_print_settings_get_int                 (GtkPrintSettings     *settings,
							      const char           *key);
int               gtk_print_settings_get_int_with_default    (GtkPrintSettings     *settings,
							      const char           *key,
							      int                   def);
void              gtk_print_settings_set_int                 (GtkPrintSettings     *settings,
							      const char           *key,
							      int                   value);

#define GTK_PRINT_SETTINGS_PRINTER "printer"
#define GTK_PRINT_SETTINGS_ORIENTATION "orientation"
#define GTK_PRINT_SETTINGS_PAPER_FORMAT "paper-format"
#define GTK_PRINT_SETTINGS_PAPER_WIDTH "paper-width"
#define GTK_PRINT_SETTINGS_PAPER_HEIGHT "paper-height"
#define GTK_PRINT_SETTINGS_NUM_COPIES "num-copies"
#define GTK_PRINT_SETTINGS_DEFAULT_SOURCE "default-source"
#define GTK_PRINT_SETTINGS_QUALITY "quality"
#define GTK_PRINT_SETTINGS_RESOLUTION "resolution"
#define GTK_PRINT_SETTINGS_USE_COLOR "use-color"
#define GTK_PRINT_SETTINGS_DUPLEX "duplex"
#define GTK_PRINT_SETTINGS_COLLATE "collate"
#define GTK_PRINT_SETTINGS_REVERSE "reverse"
#define GTK_PRINT_SETTINGS_MEDIA_TYPE "media-type"
#define GTK_PRINT_SETTINGS_DITHER "dither"
#define GTK_PRINT_SETTINGS_SCALE "scale"
#define GTK_PRINT_SETTINGS_PRINT_PAGES "print-pages"
#define GTK_PRINT_SETTINGS_PAGE_RANGES "page-ranges"
#define GTK_PRINT_SETTINGS_PAGE_SET "page-set"
#define GTK_PRINT_SETTINGS_PRINT_TO_FILE "print-to-file"
#define GTK_PRINT_SETTINGS_FINISHINGS "finishings"
#define GTK_PRINT_SETTINGS_NUMBER_UP "number-up"
#define GTK_PRINT_SETTINGS_OUTPUT_BIN "output-bin"

#define GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION "win32-driver-version"
#define GTK_PRINT_SETTINGS_WIN32_DRIVER_EXTRA "win32-driver-extra"

/* Helpers: */

const char *       gtk_print_settings_get_printer        (GtkPrintSettings   *settings);
void               gtk_print_settings_set_printer        (GtkPrintSettings   *settings,
							  const char         *printer);
GtkPageOrientation gtk_print_settings_get_orientation    (GtkPrintSettings   *settings);
void               gtk_print_settings_set_orientation    (GtkPrintSettings   *settings,
							  GtkPageOrientation  orientation);
GtkPaperSize *     gtk_print_settings_get_paper_size     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_paper_size     (GtkPrintSettings   *settings,
							  GtkPaperSize       *paper_size);
double             gtk_print_settings_get_paper_width    (GtkPrintSettings   *settings,
							  GtkUnit             unit);
void               gtk_print_settings_set_paper_width    (GtkPrintSettings   *settings,
							  double              width,
							  GtkUnit             unit);
double             gtk_print_settings_get_paper_height   (GtkPrintSettings   *settings,
							  GtkUnit             unit);
void               gtk_print_settings_set_paper_height   (GtkPrintSettings   *settings,
							  double              width,
							  GtkUnit             unit);
gboolean           gtk_print_settings_get_use_color      (GtkPrintSettings   *settings);
void               gtk_print_settings_set_use_color      (GtkPrintSettings   *settings,
							  gboolean            use_color);
gboolean           gtk_print_settings_get_collate        (GtkPrintSettings   *settings);
void               gtk_print_settings_set_collate        (GtkPrintSettings   *settings,
							  gboolean            collate);
gboolean           gtk_print_settings_get_reverse        (GtkPrintSettings   *settings);
void               gtk_print_settings_set_reverse        (GtkPrintSettings   *settings,
							  gboolean            reverse);
GtkPrintDuplex     gtk_print_settings_get_duplex         (GtkPrintSettings   *settings);
void               gtk_print_settings_set_duplex         (GtkPrintSettings   *settings,
							  GtkPrintDuplex      duplex);
GtkPrintQuality    gtk_print_settings_get_quality        (GtkPrintSettings   *settings);
void               gtk_print_settings_set_quality        (GtkPrintSettings   *settings,
							  GtkPrintQuality     quality);
int                gtk_print_settings_get_num_copies     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_num_copies     (GtkPrintSettings   *settings,
							  int                 num_copies);
int                gtk_print_settings_get_number_up      (GtkPrintSettings   *settings);
void               gtk_print_settings_set_number_up      (GtkPrintSettings   *settings,
							  int                 number_up);
int                gtk_print_settings_get_resolution     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_resolution     (GtkPrintSettings   *settings,
							  int                 resolution);
double             gtk_print_settings_get_scale          (GtkPrintSettings   *settings);
void               gtk_print_settings_set_scale          (GtkPrintSettings   *settings,
							  double              scale);
gboolean           gtk_print_settings_get_print_to_file  (GtkPrintSettings   *settings);
void               gtk_print_settings_set_print_to_file  (GtkPrintSettings   *settings,
							  gboolean            print_to_file);
GtkPrintPages      gtk_print_settings_get_print_pages    (GtkPrintSettings   *settings);
void               gtk_print_settings_set_print_pages    (GtkPrintSettings   *settings,
							  GtkPrintPages       pages);
GtkPageRange *     gtk_print_settings_get_page_ranges    (GtkPrintSettings   *settings,
							  int                *num_ranges);
void               gtk_print_settings_set_page_ranges    (GtkPrintSettings   *settings,
							  GtkPageRange       *page_ranges,
							  int                 num_ranges);
GtkPageSet         gtk_print_settings_get_page_set       (GtkPrintSettings   *settings);
void               gtk_print_settings_set_page_set       (GtkPrintSettings   *settings,
							  GtkPageSet          page_set);
const char *       gtk_print_settings_get_default_source (GtkPrintSettings   *settings);
void               gtk_print_settings_set_default_source (GtkPrintSettings   *settings,
							  const char         *default_source);
const char *       gtk_print_settings_get_media_type     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_media_type     (GtkPrintSettings   *settings,
							  const char         *media_type);
const char *       gtk_print_settings_get_dither         (GtkPrintSettings   *settings);
void               gtk_print_settings_set_dither         (GtkPrintSettings   *settings,
							  const char         *dither);
const char *       gtk_print_settings_get_finishings     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_finishings     (GtkPrintSettings   *settings,
							  const char         *finishings);
const char *       gtk_print_settings_get_output_bin     (GtkPrintSettings   *settings);
void               gtk_print_settings_set_output_bin     (GtkPrintSettings   *settings,
							  const char         *output_bin);

G_END_DECLS

#endif /* __GTK_PRINT_SETTINGS_H__ */
