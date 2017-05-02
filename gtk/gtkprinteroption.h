/* GTK - The GIMP Toolkit
 * gtkprinteroption.h: printer option
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

#ifndef __GTK_PRINTER_OPTION_H__
#define __GTK_PRINTER_OPTION_H__

/* This is a "semi-private" header; it is meant only for
 * alternate GtkPrintDialog backend modules; no stability guarantees 
 * are made at this point
 */
#ifndef GTK_PRINT_BACKEND_ENABLE_UNSUPPORTED
#error "GtkPrintBackend is not supported API for general use"
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_OPTION             (gtk_printer_option_get_type ())
#define GTK_PRINTER_OPTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_OPTION, GtkPrinterOption))
#define GTK_IS_PRINTER_OPTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_OPTION))

typedef struct _GtkPrinterOption       GtkPrinterOption;
typedef struct _GtkPrinterOptionClass  GtkPrinterOptionClass;

#define GTK_PRINTER_OPTION_GROUP_IMAGE_QUALITY "ImageQuality"
#define GTK_PRINTER_OPTION_GROUP_FINISHING "Finishing"

typedef enum {
  GTK_PRINTER_OPTION_TYPE_BOOLEAN,
  GTK_PRINTER_OPTION_TYPE_PICKONE,
  GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD,
  GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE,
  GTK_PRINTER_OPTION_TYPE_PICKONE_REAL,
  GTK_PRINTER_OPTION_TYPE_PICKONE_INT,
  GTK_PRINTER_OPTION_TYPE_PICKONE_STRING,
  GTK_PRINTER_OPTION_TYPE_ALTERNATIVE,
  GTK_PRINTER_OPTION_TYPE_STRING,
  GTK_PRINTER_OPTION_TYPE_FILESAVE
} GtkPrinterOptionType;

struct _GtkPrinterOption
{
  GObject parent_instance;

  char *name;
  char *display_text;
  GtkPrinterOptionType type;

  char *value;
  
  int num_choices;
  char **choices;
  char **choices_display;
  
  gboolean activates_default;

  gboolean has_conflict;
  char *group;
};

struct _GtkPrinterOptionClass
{
  GObjectClass parent_class;

  void (*changed) (GtkPrinterOption *option);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
};

GType   gtk_printer_option_get_type       (void) G_GNUC_CONST;

GtkPrinterOption *gtk_printer_option_new                    (const char           *name,
							     const char           *display_text,
							     GtkPrinterOptionType  type);
void              gtk_printer_option_set                    (GtkPrinterOption     *option,
							     const char           *value);
void              gtk_printer_option_set_has_conflict       (GtkPrinterOption     *option,
							     gboolean              has_conflict);
void              gtk_printer_option_clear_has_conflict     (GtkPrinterOption     *option);
void              gtk_printer_option_set_boolean            (GtkPrinterOption     *option,
							     gboolean              value);
void              gtk_printer_option_allocate_choices       (GtkPrinterOption     *option,
							     int                   num);
void              gtk_printer_option_choices_from_array     (GtkPrinterOption     *option,
							     int                   num_choices,
							     char                 *choices[],
							     char                 *choices_display[]);
gboolean          gtk_printer_option_has_choice             (GtkPrinterOption     *option,
							    const char           *choice);
void              gtk_printer_option_set_activates_default (GtkPrinterOption     *option,
							    gboolean              activates);
gboolean          gtk_printer_option_get_activates_default (GtkPrinterOption     *option);


G_END_DECLS

#endif /* __GTK_PRINTER_OPTION_H__ */


