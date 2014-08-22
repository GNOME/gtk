/* GTK - The GIMP Toolkit
 * gtkprintbackendcups.h: Default implementation of GtkPrintBackend
 * for the Common Unix Print System (CUPS)
 * Copyright (C) 2006, 2007 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
/* Cups 1.6 deprecates ppdFindAttr(), ppdFindCustomOption(),
 * ppdFirstCustomParam(), and ppdNextCustomParam() among others. This
 * turns off the warning so that it will compile.
 */
#ifdef HAVE_CUPS_API_1_6
# define _PPD_DEPRECATED
#endif

#include <cups/cups.h>
#include <cups/language.h>
#include <cups/http.h>
#include <cups/ipp.h>
#include <errno.h>
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>

#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <gtk/gtk.h>
#include <gtk/gtkprintbackend.h>
#include <gtk/gtkunixprint.h>
#include <gtk/gtkprinter-private.h>

#include "gtkprintbackendcups.h"
#include "gtkprintercups.h"

#include "gtkcupsutils.h"
#include "gtkcupssecretsutils.h"

#ifdef HAVE_COLORD
#include <colord.h>
#endif

typedef struct _GtkPrintBackendCupsClass GtkPrintBackendCupsClass;

#define GTK_PRINT_BACKEND_CUPS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_BACKEND_CUPS, GtkPrintBackendCupsClass))
#define GTK_IS_PRINT_BACKEND_CUPS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_BACKEND_CUPS))
#define GTK_PRINT_BACKEND_CUPS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_BACKEND_CUPS, GtkPrintBackendCupsClass))

#define _CUPS_MAX_ATTEMPTS 10
#define _CUPS_MAX_CHUNK_SIZE 8192

#ifdef HAVE_CUPS_API_1_6
#define AVAHI_IF_UNSPEC -1
#define AVAHI_PROTO_INET 0
#define AVAHI_PROTO_INET6 1
#define AVAHI_PROTO_UNSPEC -1

#define AVAHI_BUS "org.freedesktop.Avahi"
#define AVAHI_SERVER_IFACE "org.freedesktop.Avahi.Server"
#define AVAHI_SERVICE_BROWSER_IFACE "org.freedesktop.Avahi.ServiceBrowser"
#define AVAHI_SERVICE_RESOLVER_IFACE "org.freedesktop.Avahi.ServiceResolver"
#endif

/* define this to see warnings about ignored ppd options */
#undef PRINT_IGNORED_OPTIONS

#define _CUPS_MAP_ATTR_INT(attr, v, a) {if (!g_ascii_strcasecmp (attr->name, (a))) v = attr->values[0].integer;}
#define _CUPS_MAP_ATTR_STR(attr, v, a) {if (!g_ascii_strcasecmp (attr->name, (a))) v = attr->values[0].string.text;}

static GType print_backend_cups_type = 0;

typedef void (* GtkPrintCupsResponseCallbackFunc) (GtkPrintBackend *print_backend,
                                                   GtkCupsResult   *result,
                                                   gpointer         user_data);

typedef enum
{
  DISPATCH_SETUP,
  DISPATCH_REQUEST,
  DISPATCH_SEND,
  DISPATCH_CHECK,
  DISPATCH_READ,
  DISPATCH_ERROR
} GtkPrintCupsDispatchState;

typedef struct
{
  GSource source;

  http_t *http;
  GtkCupsRequest *request;
  GtkCupsPollState poll_state;
  GPollFD *data_poll;
  GtkPrintBackendCups *backend;
  GtkPrintCupsResponseCallbackFunc callback;
  gpointer                         callback_data;

} GtkPrintCupsDispatchWatch;

struct _GtkPrintBackendCupsClass
{
  GtkPrintBackendClass parent_class;
};

struct _GtkPrintBackendCups
{
  GtkPrintBackend parent_instance;

  char *default_printer;

  guint list_printers_poll;
  guint list_printers_pending : 1;
  gint  list_printers_attempts;
  guint got_default_printer   : 1;
  guint default_printer_poll;
  GtkCupsConnectionTest *cups_connection_test;
  gint  reading_ppds;

  char **covers;
  int    number_of_covers;

  GList      *requests;
  GHashTable *auth;
  gchar      *username;
  gboolean    authentication_lock;
#ifdef HAVE_COLORD
  CdClient   *colord_client;
#endif
#ifdef HAVE_CUPS_API_1_6
  GDBusConnection *dbus_connection;
  gchar           *avahi_default_printer;
  guint            avahi_service_browser_subscription_id;
  guint            avahi_service_browser_subscription_ids[2];
  gchar           *avahi_service_browser_paths[2];
  GCancellable    *avahi_cancellable;
#endif
  gboolean      secrets_service_available;
  guint         secrets_service_watch_id;
  GCancellable *secrets_service_cancellable;
};

static GObjectClass *backend_parent_class;

static void                 gtk_print_backend_cups_class_init      (GtkPrintBackendCupsClass          *class);
static void                 gtk_print_backend_cups_init            (GtkPrintBackendCups               *impl);
static void                 gtk_print_backend_cups_finalize        (GObject                           *object);
static void                 gtk_print_backend_cups_dispose         (GObject                           *object);
static void                 cups_get_printer_list                  (GtkPrintBackend                   *print_backend);
static void                 cups_get_default_printer               (GtkPrintBackendCups               *print_backend);
static void                 cups_get_local_default_printer         (GtkPrintBackendCups               *print_backend);
static void                 cups_request_execute                   (GtkPrintBackendCups               *print_backend,
								    GtkCupsRequest                    *request,
								    GtkPrintCupsResponseCallbackFunc   callback,
								    gpointer                           user_data,
								    GDestroyNotify                     notify);
static void                 cups_printer_get_settings_from_options (GtkPrinter                        *printer,
								    GtkPrinterOptionSet               *options,
								    GtkPrintSettings                  *settings);
static gboolean             cups_printer_mark_conflicts            (GtkPrinter                        *printer,
								    GtkPrinterOptionSet               *options);
static GtkPrinterOptionSet *cups_printer_get_options               (GtkPrinter                        *printer,
								    GtkPrintSettings                  *settings,
								    GtkPageSetup                      *page_setup,
                                                                    GtkPrintCapabilities               capabilities);
static void                 cups_printer_prepare_for_print         (GtkPrinter                        *printer,
								    GtkPrintJob                       *print_job,
								    GtkPrintSettings                  *settings,
								    GtkPageSetup                      *page_setup);
static GList *              cups_printer_list_papers               (GtkPrinter                        *printer);
static GtkPageSetup *       cups_printer_get_default_page_size     (GtkPrinter                        *printer);
static void                 cups_printer_request_details           (GtkPrinter                        *printer);
static gboolean             cups_request_default_printer           (GtkPrintBackendCups               *print_backend);
static gboolean             cups_request_ppd                       (GtkPrinter                        *printer);
static gboolean             cups_printer_get_hard_margins          (GtkPrinter                        *printer,
								    gdouble                           *top,
								    gdouble                           *bottom,
								    gdouble                           *left,
								    gdouble                           *right);
static GtkPrintCapabilities cups_printer_get_capabilities          (GtkPrinter                        *printer);
static void                 set_option_from_settings               (GtkPrinterOption                  *option,
								    GtkPrintSettings                  *setting);
static void                 cups_begin_polling_info                (GtkPrintBackendCups               *print_backend,
								    GtkPrintJob                       *job,
								    int                                job_id);
static gboolean             cups_job_info_poll_timeout             (gpointer                           user_data);
static void                 gtk_print_backend_cups_print_stream    (GtkPrintBackend                   *backend,
								    GtkPrintJob                       *job,
								    GIOChannel                        *data_io,
								    GtkPrintJobCompleteFunc            callback,
								    gpointer                           user_data,
								    GDestroyNotify                     dnotify);
static cairo_surface_t *    cups_printer_create_cairo_surface      (GtkPrinter                        *printer,
								    GtkPrintSettings                  *settings,
								    gdouble                            width,
								    gdouble                            height,
								    GIOChannel                        *cache_io);

static void                 gtk_print_backend_cups_set_password    (GtkPrintBackend                   *backend,
                                                                    gchar                            **auth_info_required,
                                                                    gchar                            **auth_info,
                                                                    gboolean                           store_auth_info);

void                        overwrite_and_free                      (gpointer                          data);
static gboolean             is_address_local                        (const gchar                      *address);
static gboolean             request_auth_info                       (gpointer                          data);
static void                 lookup_auth_info                        (gpointer                          data);

#ifdef HAVE_CUPS_API_1_6
static void                 avahi_request_printer_list              (GtkPrintBackendCups              *cups_backend);
#endif

static void                 secrets_service_appeared_cb             (GDBusConnection *connection,
                                                                     const gchar *name,
                                                                     const gchar *name_owner,
                                                                     gpointer user_data);
static void                 secrets_service_vanished_cb             (GDBusConnection *connection,
                                                                     const gchar *name,
                                                                     gpointer user_data);

static void
gtk_print_backend_cups_register_type (GTypeModule *module)
{
  const GTypeInfo print_backend_cups_info =
  {
    sizeof (GtkPrintBackendCupsClass),
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) gtk_print_backend_cups_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof (GtkPrintBackendCups),
    0,	          	/* n_preallocs */
    (GInstanceInitFunc) gtk_print_backend_cups_init
  };

  print_backend_cups_type = g_type_module_register_type (module,
                                                         GTK_TYPE_PRINT_BACKEND,
                                                         "GtkPrintBackendCups",
                                                         &print_backend_cups_info, 0);
}

G_MODULE_EXPORT void
pb_module_init (GTypeModule *module)
{
  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: Initializing the CUPS print backend module\n"));

  gtk_print_backend_cups_register_type (module);
  gtk_printer_cups_register_type (module);
}

G_MODULE_EXPORT void
pb_module_exit (void)
{

}

G_MODULE_EXPORT GtkPrintBackend *
pb_module_create (void)
{
  return gtk_print_backend_cups_new ();
}
/* CUPS 1.6 Getter/Setter Functions CUPS 1.6 makes private most of the
 * IPP structures and enforces access via new getter functions, which
 * are unfortunately not available in earlier versions. We define
 * below those getter functions as macros for use when building
 * against earlier CUPS versions.
 */
#ifndef HAVE_CUPS_API_1_6
#define ippGetOperation(ipp_request) ipp_request->request.op.operation_id
#define ippGetInteger(attr, index) attr->values[index].integer
#define ippGetBoolean(attr, index) attr->values[index].boolean
#define ippGetString(attr, index, foo) attr->values[index].string.text
#define ippGetValueTag(attr) attr->value_tag
#define ippGetName(attr) attr->name
#define ippGetCount(attr) attr->num_values
#define ippGetGroupTag(attr) attr->group_tag

static int
ippGetRange (ipp_attribute_t *attr,
             int element,
             int *upper)
{
  *upper = attr->values[element].range.upper;
  return (attr->values[element].range.lower);
}
#endif
/*
 * GtkPrintBackendCups
 */
GType
gtk_print_backend_cups_get_type (void)
{
  return print_backend_cups_type;
}

/**
 * gtk_print_backend_cups_new:
 *
 * Creates a new #GtkPrintBackendCups object. #GtkPrintBackendCups
 * implements the #GtkPrintBackend interface with direct access to
 * the filesystem using Unix/Linux API calls
 *
 * Returns: the new #GtkPrintBackendCups object
 */
GtkPrintBackend *
gtk_print_backend_cups_new (void)
{
  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: Creating a new CUPS print backend object\n"));

  return g_object_new (GTK_TYPE_PRINT_BACKEND_CUPS, NULL);
}

static void
gtk_print_backend_cups_class_init (GtkPrintBackendCupsClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkPrintBackendClass *backend_class = GTK_PRINT_BACKEND_CLASS (class);

  backend_parent_class = g_type_class_peek_parent (class);

  gobject_class->finalize = gtk_print_backend_cups_finalize;
  gobject_class->dispose = gtk_print_backend_cups_dispose;

  backend_class->request_printer_list = cups_get_printer_list;
  backend_class->print_stream = gtk_print_backend_cups_print_stream;
  backend_class->printer_request_details = cups_printer_request_details;
  backend_class->printer_create_cairo_surface = cups_printer_create_cairo_surface;
  backend_class->printer_get_options = cups_printer_get_options;
  backend_class->printer_mark_conflicts = cups_printer_mark_conflicts;
  backend_class->printer_get_settings_from_options = cups_printer_get_settings_from_options;
  backend_class->printer_prepare_for_print = cups_printer_prepare_for_print;
  backend_class->printer_list_papers = cups_printer_list_papers;
  backend_class->printer_get_default_page_size = cups_printer_get_default_page_size;
  backend_class->printer_get_hard_margins = cups_printer_get_hard_margins;
  backend_class->printer_get_capabilities = cups_printer_get_capabilities;
  backend_class->set_password = gtk_print_backend_cups_set_password;
}

static cairo_status_t
_cairo_write_to_cups (void                *closure,
                      const unsigned char *data,
                      unsigned int         length)
{
  GIOChannel *io = (GIOChannel *)closure;
  gsize written;
  GError *error;

  error = NULL;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: Writing %i byte chunk to temp file\n", length));

  while (length > 0)
    {
      g_io_channel_write_chars (io, (gchar *)data, length, &written, &error);

      if (error != NULL)
	{
	  GTK_NOTE (PRINTING,
                    g_print ("CUPS Backend: Error writing to temp file, %s\n",
                             error->message));

          g_error_free (error);
	  return CAIRO_STATUS_WRITE_ERROR;
	}

      GTK_NOTE (PRINTING,
                g_print ("CUPS Backend: Wrote %"G_GSIZE_FORMAT" bytes to temp file\n", written));

      data += written;
      length -= written;
    }

  return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
cups_printer_create_cairo_surface (GtkPrinter       *printer,
				   GtkPrintSettings *settings,
				   gdouble           width,
				   gdouble           height,
				   GIOChannel       *cache_io)
{
  cairo_surface_t *surface;
  ppd_file_t      *ppd_file = NULL;
  ppd_attr_t      *ppd_attr = NULL;
  ppd_attr_t      *ppd_attr_res = NULL;
  ppd_attr_t      *ppd_attr_screen_freq = NULL;
  ppd_attr_t      *ppd_attr_res_screen_freq = NULL;
  gchar           *res_string = NULL;
  gint             level = 2;

  if (gtk_printer_accepts_pdf (printer))
    surface = cairo_pdf_surface_create_for_stream (_cairo_write_to_cups, cache_io, width, height);
  else
    surface = cairo_ps_surface_create_for_stream  (_cairo_write_to_cups, cache_io, width, height);

  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));

  if (ppd_file != NULL)
    {
      ppd_attr = ppdFindAttr (ppd_file, "LanguageLevel", NULL);

      if (ppd_attr != NULL)
        level = atoi (ppd_attr->value);

      if (gtk_print_settings_get_resolution (settings) == 0)
        {
          ppd_attr_res = ppdFindAttr (ppd_file, "DefaultResolution", NULL);

          if (ppd_attr_res != NULL)
            {
              int res, res_x, res_y;

              if (sscanf (ppd_attr_res->value, "%dx%ddpi", &res_x, &res_y) == 2)
                {
                  if (res_x > 0 && res_y > 0)
                    gtk_print_settings_set_resolution_xy (settings, res_x, res_y);
                }
              else if (sscanf (ppd_attr_res->value, "%ddpi", &res) == 1)
                {
                  if (res > 0)
                    gtk_print_settings_set_resolution (settings, res);
                }
            }
        }

      res_string = g_strdup_printf ("%ddpi",
                                    gtk_print_settings_get_resolution (settings));
      ppd_attr_res_screen_freq = ppdFindAttr (ppd_file, "ResScreenFreq", res_string);
      g_free (res_string);

      if (ppd_attr_res_screen_freq == NULL)
        {
          res_string = g_strdup_printf ("%dx%ddpi",
                                        gtk_print_settings_get_resolution_x (settings),
                                        gtk_print_settings_get_resolution_y (settings));
          ppd_attr_res_screen_freq = ppdFindAttr (ppd_file, "ResScreenFreq", res_string);
          g_free (res_string);
        }

      ppd_attr_screen_freq = ppdFindAttr (ppd_file, "ScreenFreq", NULL);

      if (ppd_attr_res_screen_freq != NULL && atof (ppd_attr_res_screen_freq->value) > 0.0)
        gtk_print_settings_set_printer_lpi (settings, atof (ppd_attr_res_screen_freq->value));
      else if (ppd_attr_screen_freq != NULL && atof (ppd_attr_screen_freq->value) > 0.0)
        gtk_print_settings_set_printer_lpi (settings, atof (ppd_attr_screen_freq->value));
    }

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_PS)
    {
      if (level == 2)
        cairo_ps_surface_restrict_to_level (surface, CAIRO_PS_LEVEL_2);

      if (level == 3)
        cairo_ps_surface_restrict_to_level (surface, CAIRO_PS_LEVEL_3);
    }

  cairo_surface_set_fallback_resolution (surface,
                                         2.0 * gtk_print_settings_get_printer_lpi (settings),
                                         2.0 * gtk_print_settings_get_printer_lpi (settings));

  return surface;
}

typedef struct {
  GtkPrintJobCompleteFunc callback;
  GtkPrintJob *job;
  gpointer user_data;
  GDestroyNotify dnotify;
  http_t *http;
} CupsPrintStreamData;

static void
cups_free_print_stream_data (CupsPrintStreamData *data)
{
  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  if (data->dnotify)
    data->dnotify (data->user_data);
  g_object_unref (data->job);
  if (data->http != NULL)
    httpClose (data->http);
  g_free (data);
}

static void
cups_print_cb (GtkPrintBackendCups *print_backend,
               GtkCupsResult       *result,
               gpointer             user_data)
{
  GError *error = NULL;
  CupsPrintStreamData *ps = user_data;

  gdk_threads_enter ();

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  if (gtk_cups_result_is_error (result))
    error = g_error_new_literal (gtk_print_error_quark (),
                                 GTK_PRINT_ERROR_INTERNAL_ERROR,
                                 gtk_cups_result_get_error_string (result));

  if (ps->callback)
    ps->callback (ps->job, ps->user_data, error);

  if (error == NULL)
    {
      int job_id = 0;
      ipp_attribute_t *attr;		/* IPP job-id attribute */
      ipp_t *response = gtk_cups_result_get_response (result);

      if ((attr = ippFindAttribute (response, "job-id", IPP_TAG_INTEGER)) != NULL)
	job_id = ippGetInteger (attr, 0);

      if (!gtk_print_job_get_track_print_status (ps->job) || job_id == 0)
	gtk_print_job_set_status (ps->job, GTK_PRINT_STATUS_FINISHED);
      else
	{
	  gtk_print_job_set_status (ps->job, GTK_PRINT_STATUS_PENDING);
	  cups_begin_polling_info (print_backend, ps->job, job_id);
	}
    }
  else
    gtk_print_job_set_status (ps->job, GTK_PRINT_STATUS_FINISHED_ABORTED);


  if (error)
    g_error_free (error);

  gdk_threads_leave ();
}

typedef struct {
  GtkCupsRequest *request;
  GtkPrinterCups *printer;
} CupsOptionsData;

static void
add_cups_options (const gchar *key,
		  const gchar *value,
		  gpointer     user_data)
{
  CupsOptionsData *data = (CupsOptionsData *) user_data;
  GtkCupsRequest *request = data->request;
  GtkPrinterCups *printer = data->printer;
  gboolean custom_value = FALSE;
  gchar *new_value = NULL;
  gint i;

  if (!key || !value)
    return;

  if (!g_str_has_prefix (key, "cups-"))
    return;

  if (strcmp (value, "gtk-ignore-value") == 0)
    return;

  key = key + strlen ("cups-");

  if (printer && printer->ppd_file)
    {
      ppd_coption_t *coption;
      gboolean       found = FALSE;
      gboolean       custom_values_enabled = FALSE;

      coption = ppdFindCustomOption (printer->ppd_file, key);
      if (coption && coption->option)
        {
          for (i = 0; i < coption->option->num_choices; i++)
            {
              /* Are custom values enabled ? */
              if (g_str_equal (coption->option->choices[i].choice, "Custom"))
                custom_values_enabled = TRUE;

              /* Is the value among available choices ? */
              if (g_str_equal (coption->option->choices[i].choice, value))
                found = TRUE;
            }

          if (custom_values_enabled && !found)
            custom_value = TRUE;
        }
    }

  /* Add "Custom." prefix to custom values if not already added. */
  if (custom_value && !g_str_has_prefix (value, "Custom."))
    {
      new_value = g_strdup_printf ("Custom.%s", value);
      gtk_cups_request_encode_option (request, key, new_value);
      g_free (new_value);
    }
  else
    gtk_cups_request_encode_option (request, key, value);
}

static void
gtk_print_backend_cups_print_stream (GtkPrintBackend         *print_backend,
                                     GtkPrintJob             *job,
				     GIOChannel              *data_io,
				     GtkPrintJobCompleteFunc  callback,
				     gpointer                 user_data,
				     GDestroyNotify           dnotify)
{
  GtkPrinterCups *cups_printer;
  CupsPrintStreamData *ps;
  CupsOptionsData *options_data;
  GtkCupsRequest *request = NULL;
  GtkPrintSettings *settings;
  const gchar *title;
  char  printer_absolute_uri[HTTP_MAX_URI];
  http_t *http = NULL;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  cups_printer = GTK_PRINTER_CUPS (gtk_print_job_get_printer (job));
  settings = gtk_print_job_get_settings (job);

#ifdef HAVE_CUPS_API_1_6
  if (cups_printer->avahi_browsed)
    {
      http = httpConnect (cups_printer->hostname, cups_printer->port);
      if (http)
        {
          request = gtk_cups_request_new_with_username (http,
                                                        GTK_CUPS_POST,
                                                        IPP_PRINT_JOB,
                                                        data_io,
                                                        cups_printer->hostname,
                                                        cups_printer->device_uri,
                                                        GTK_PRINT_BACKEND_CUPS (print_backend)->username);
          g_snprintf (printer_absolute_uri, HTTP_MAX_URI, "%s", cups_printer->printer_uri);
        }
      else
        {
          GError *error = NULL;

          GTK_NOTE (PRINTING,
                    g_warning ("CUPS Backend: Error connecting to %s:%d",
                               cups_printer->hostname,
                               cups_printer->port));

          error = g_error_new (gtk_print_error_quark (),
                               GTK_CUPS_ERROR_GENERAL,
                               "Error connecting to %s",
                               cups_printer->hostname);

          gtk_print_job_set_status (job, GTK_PRINT_STATUS_FINISHED_ABORTED);

          if (callback)
            {
              callback (job, user_data, error);
            }

          g_clear_error (&error);

          return;
        }
    }
  else
#endif
    {
      request = gtk_cups_request_new_with_username (NULL,
                                                    GTK_CUPS_POST,
                                                    IPP_PRINT_JOB,
                                                    data_io,
                                                    NULL,
                                                    cups_printer->device_uri,
                                                    GTK_PRINT_BACKEND_CUPS (print_backend)->username);

      httpAssembleURIf (HTTP_URI_CODING_ALL,
                        printer_absolute_uri,
                        sizeof (printer_absolute_uri),
                        "ipp",
                        NULL,
                        "localhost",
                        ippPort (),
                        "/printers/%s",
                        gtk_printer_get_name (gtk_print_job_get_printer (job)));
    }

  gtk_cups_request_set_ipp_version (request,
                                    cups_printer->ipp_version_major,
                                    cups_printer->ipp_version_minor);

  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION,
                                   IPP_TAG_URI, "printer-uri",
                                   NULL, printer_absolute_uri);

  title = gtk_print_job_get_title (job);
  if (title)
    gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION,
                                     IPP_TAG_NAME, "job-name",
                                     NULL, title);

  options_data = g_new0 (CupsOptionsData, 1);
  options_data->request = request;
  options_data->printer = cups_printer;
  gtk_print_settings_foreach (settings, add_cups_options, options_data);
  g_free (options_data);

  ps = g_new0 (CupsPrintStreamData, 1);
  ps->callback = callback;
  ps->user_data = user_data;
  ps->dnotify = dnotify;
  ps->job = g_object_ref (job);
  ps->http = http;

  request->need_auth_info = cups_printer->auth_info_required != NULL;
  request->auth_info_required = g_strdupv (cups_printer->auth_info_required);

  cups_request_execute (GTK_PRINT_BACKEND_CUPS (print_backend),
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_print_cb,
                        ps,
                        (GDestroyNotify)cups_free_print_stream_data);
}

void overwrite_and_free (gpointer data)
{
  gchar *password = (gchar *) data;

  if (password != NULL)
    {
      memset (password, 0, strlen (password));
      g_free (password);
    }
}

static void
gtk_print_backend_cups_init (GtkPrintBackendCups *backend_cups)
{
#ifdef HAVE_CUPS_API_1_6
  gint i;
#endif

  backend_cups->list_printers_poll = FALSE;
  backend_cups->got_default_printer = FALSE;
  backend_cups->list_printers_pending = FALSE;
  backend_cups->list_printers_attempts = 0;
  backend_cups->reading_ppds = 0;

  backend_cups->requests = NULL;
  backend_cups->auth = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, overwrite_and_free);
  backend_cups->authentication_lock = FALSE;

  backend_cups->covers = NULL;
  backend_cups->number_of_covers = 0;

  backend_cups->default_printer_poll = 0;
  backend_cups->cups_connection_test = NULL;

  backend_cups->username = NULL;

#ifdef HAVE_COLORD
  backend_cups->colord_client = cd_client_new ();
#endif

#ifdef HAVE_CUPS_API_1_6
  backend_cups->dbus_connection = NULL;
  backend_cups->avahi_default_printer = NULL;
  backend_cups->avahi_service_browser_subscription_id = 0;
  for (i = 0; i < 2; i++)
    {
      backend_cups->avahi_service_browser_paths[i] = NULL;
      backend_cups->avahi_service_browser_subscription_ids[i] = 0;
    }
#endif

  cups_get_local_default_printer (backend_cups);

  backend_cups->secrets_service_available = FALSE;
  backend_cups->secrets_service_cancellable = g_cancellable_new ();
  backend_cups->secrets_service_watch_id =
    gtk_cups_secrets_service_watch (secrets_service_appeared_cb,
                                    secrets_service_vanished_cb,
                                    backend_cups);
}

static void
gtk_print_backend_cups_finalize (GObject *object)
{
  GtkPrintBackendCups *backend_cups;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: finalizing CUPS backend module\n"));

  backend_cups = GTK_PRINT_BACKEND_CUPS (object);

  g_free (backend_cups->default_printer);
  backend_cups->default_printer = NULL;

  g_strfreev (backend_cups->covers);
  backend_cups->number_of_covers = 0;

  gtk_cups_connection_test_free (backend_cups->cups_connection_test);
  backend_cups->cups_connection_test = NULL;

  g_hash_table_destroy (backend_cups->auth);

  g_free (backend_cups->username);

#ifdef HAVE_COLORD
  g_object_unref (backend_cups->colord_client);
#endif

#ifdef HAVE_CUPS_API_1_6
  g_clear_object (&backend_cups->avahi_cancellable);
  g_clear_pointer (&backend_cups->avahi_default_printer, g_free);
  g_clear_object (&backend_cups->dbus_connection);
#endif

  g_clear_object (&backend_cups->secrets_service_cancellable);
  if (backend_cups->secrets_service_watch_id != 0)
    {
      g_bus_unwatch_name (backend_cups->secrets_service_watch_id);
    }

  backend_parent_class->finalize (object);
}

static void
gtk_print_backend_cups_dispose (GObject *object)
{
  GtkPrintBackendCups *backend_cups;
#ifdef HAVE_CUPS_API_1_6
  gint                 i;
#endif

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  backend_cups = GTK_PRINT_BACKEND_CUPS (object);

  if (backend_cups->list_printers_poll > 0)
    g_source_remove (backend_cups->list_printers_poll);
  backend_cups->list_printers_poll = 0;
  backend_cups->list_printers_attempts = 0;

  if (backend_cups->default_printer_poll > 0)
    g_source_remove (backend_cups->default_printer_poll);
  backend_cups->default_printer_poll = 0;

#ifdef HAVE_CUPS_API_1_6
  g_cancellable_cancel (backend_cups->avahi_cancellable);

  for (i = 0; i < 2; i++)
    {
      if (backend_cups->avahi_service_browser_subscription_ids[i] > 0)
        {
          g_dbus_connection_signal_unsubscribe (backend_cups->dbus_connection,
                                                backend_cups->avahi_service_browser_subscription_ids[i]);
          backend_cups->avahi_service_browser_subscription_ids[i] = 0;
        }

      if (backend_cups->avahi_service_browser_paths[i])
        {
          g_dbus_connection_call (backend_cups->dbus_connection,
                                  AVAHI_BUS,
                                  backend_cups->avahi_service_browser_paths[i],
                                  AVAHI_SERVICE_BROWSER_IFACE,
                                  "Free",
                                  NULL,
                                  NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  NULL,
                                  NULL);
          g_clear_pointer (&backend_cups->avahi_service_browser_paths[i], g_free);
        }
    }

  if (backend_cups->avahi_service_browser_subscription_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (backend_cups->dbus_connection,
                                            backend_cups->avahi_service_browser_subscription_id);
      backend_cups->avahi_service_browser_subscription_id = 0;
    }
#endif

  backend_parent_class->dispose (object);
}

static gboolean
is_address_local (const gchar *address)
{
  if (address[0] == '/' ||
      strcmp (address, "127.0.0.1") == 0 ||
      strcmp (address, "[::1]") == 0)
    return TRUE;
  else
    return FALSE;
}

static void
gtk_print_backend_cups_set_password (GtkPrintBackend  *backend,
                                     gchar           **auth_info_required,
                                     gchar           **auth_info,
                                     gboolean          store_auth_info)
{
  GtkPrintBackendCups *cups_backend = GTK_PRINT_BACKEND_CUPS (backend);
  GList *l;
  char   dispatch_hostname[HTTP_MAX_URI];
  gchar *username = NULL;
  gchar *hostname = NULL;
  gchar *password = NULL;
  gint   length;
  gint   i;

  length = g_strv_length (auth_info_required);

  if (auth_info != NULL)
    for (i = 0; i < length; i++)
      {
        if (g_strcmp0 (auth_info_required[i], "username") == 0)
          username = g_strdup (auth_info[i]);
        else if (g_strcmp0 (auth_info_required[i], "hostname") == 0)
          hostname = g_strdup (auth_info[i]);
        else if (g_strcmp0 (auth_info_required[i], "password") == 0)
          password = g_strdup (auth_info[i]);
      }

  if (hostname != NULL && username != NULL && password != NULL)
    {
      gchar *key = g_strconcat (username, "@", hostname, NULL);
      g_hash_table_insert (cups_backend->auth, key, g_strdup (password));
      GTK_NOTE (PRINTING,
                g_print ("CUPS backend: caching password for %s\n", key));
    }

  g_free (cups_backend->username);
  cups_backend->username = g_strdup (username);


  for (l = cups_backend->requests; l; l = l->next)
    {
      GtkPrintCupsDispatchWatch *dispatch = l->data;

      httpGetHostname (dispatch->request->http, dispatch_hostname, sizeof (dispatch_hostname));
      if (is_address_local (dispatch_hostname))
        strcpy (dispatch_hostname, "localhost");

      if (dispatch->request->need_auth_info)
        {
          if (auth_info != NULL)
            {
              dispatch->request->auth_info = g_new0 (gchar *, length + 1);
              for (i = 0; i < length; i++)
                dispatch->request->auth_info[i] = g_strdup (auth_info[i]);
            }
          /* Save the password if the user requested it */
          if (password != NULL && store_auth_info)
            {
              const gchar *printer_uri =
                  gtk_cups_request_ipp_get_string (dispatch->request,
                                                   IPP_TAG_URI,
                                                   "printer-uri");

              gtk_cups_secrets_service_store (auth_info, auth_info_required,
                                              printer_uri);
            }
          dispatch->backend->authentication_lock = FALSE;
          dispatch->request->need_auth_info = FALSE;
        }
      else if (dispatch->request->password_state == GTK_CUPS_PASSWORD_REQUESTED || auth_info == NULL)
        {
          overwrite_and_free (dispatch->request->password);
          dispatch->request->password = g_strdup (password);
          g_free (dispatch->request->username);
          dispatch->request->username = g_strdup (username);
          dispatch->request->password_state = GTK_CUPS_PASSWORD_HAS;
          dispatch->backend->authentication_lock = FALSE;
        }
    }
}

static gboolean
request_password (gpointer data)
{
  GtkPrintCupsDispatchWatch *dispatch = data;
  const gchar               *username;
  gchar                     *password;
  gchar                     *prompt = NULL;
  gchar                     *key = NULL;
  char                       hostname[HTTP_MAX_URI];
  gchar                    **auth_info_required;
  gchar                    **auth_info_default;
  gchar                    **auth_info_display;
  gboolean                  *auth_info_visible;
  gint                       length = 3;
  gint                       i;

  if (dispatch->backend->authentication_lock)
    return G_SOURCE_REMOVE;

  httpGetHostname (dispatch->request->http, hostname, sizeof (hostname));
  if (is_address_local (hostname))
    strcpy (hostname, "localhost");

  if (dispatch->backend->username != NULL)
    username = dispatch->backend->username;
  else
    username = cupsUser ();

  auth_info_required = g_new0 (gchar*, length + 1);
  auth_info_required[0] = g_strdup ("hostname");
  auth_info_required[1] = g_strdup ("username");
  auth_info_required[2] = g_strdup ("password");

  auth_info_default = g_new0 (gchar*, length + 1);
  auth_info_default[0] = g_strdup (hostname);
  auth_info_default[1] = g_strdup (username);

  auth_info_display = g_new0 (gchar*, length + 1);
  auth_info_display[1] = g_strdup (_("Username:"));
  auth_info_display[2] = g_strdup (_("Password:"));

  auth_info_visible = g_new0 (gboolean, length + 1);
  auth_info_visible[1] = TRUE;

  key = g_strconcat (username, "@", hostname, NULL);
  password = g_hash_table_lookup (dispatch->backend->auth, key);

  if (password && dispatch->request->password_state != GTK_CUPS_PASSWORD_NOT_VALID)
    {
      GTK_NOTE (PRINTING,
                g_print ("CUPS backend: using stored password for %s\n", key));

      overwrite_and_free (dispatch->request->password);
      dispatch->request->password = g_strdup (password);
      g_free (dispatch->request->username);
      dispatch->request->username = g_strdup (username);
      dispatch->request->password_state = GTK_CUPS_PASSWORD_HAS;
    }
  else
    {
      const char *job_title = gtk_cups_request_ipp_get_string (dispatch->request, IPP_TAG_NAME, "job-name");
      const char *printer_uri = gtk_cups_request_ipp_get_string (dispatch->request, IPP_TAG_URI, "printer-uri");
      char *printer_name = NULL;

      if (printer_uri != NULL && strrchr (printer_uri, '/') != NULL)
        printer_name = g_strdup (strrchr (printer_uri, '/') + 1);

      if (dispatch->request->password_state == GTK_CUPS_PASSWORD_NOT_VALID)
        g_hash_table_remove (dispatch->backend->auth, key);

      dispatch->request->password_state = GTK_CUPS_PASSWORD_REQUESTED;

      dispatch->backend->authentication_lock = TRUE;

      switch (ippGetOperation (dispatch->request->ipp_request))
        {
          case IPP_PRINT_JOB:
            if (job_title != NULL && printer_name != NULL)
              prompt = g_strdup_printf ( _("Authentication is required to print document “%s” on printer %s"), job_title, printer_name);
            else
              prompt = g_strdup_printf ( _("Authentication is required to print a document on %s"), hostname);
            break;
          case IPP_GET_JOB_ATTRIBUTES:
            if (job_title != NULL)
              prompt = g_strdup_printf ( _("Authentication is required to get attributes of job “%s”"), job_title);
            else
              prompt = g_strdup ( _("Authentication is required to get attributes of a job"));
            break;
          case IPP_GET_PRINTER_ATTRIBUTES:
            if (printer_name != NULL)
              prompt = g_strdup_printf ( _("Authentication is required to get attributes of printer %s"), printer_name);
            else
              prompt = g_strdup ( _("Authentication is required to get attributes of a printer"));
            break;
          case CUPS_GET_DEFAULT:
            prompt = g_strdup_printf ( _("Authentication is required to get default printer of %s"), hostname);
            break;
          case CUPS_GET_PRINTERS:
            prompt = g_strdup_printf ( _("Authentication is required to get printers from %s"), hostname);
            break;
          default:
            /* work around gcc warning about 0 not being a value for this enum */
            if (ippGetOperation (dispatch->request->ipp_request) == 0)
              prompt = g_strdup_printf ( _("Authentication is required to get a file from %s"), hostname);
            else
              prompt = g_strdup_printf ( _("Authentication is required on %s"), hostname);
            break;
        }

      g_free (printer_name);

      g_signal_emit_by_name (dispatch->backend, "request-password",
                             auth_info_required, auth_info_default,
                             auth_info_display, auth_info_visible, prompt,
                             FALSE); /* Cups password is only cached not stored. */

      g_free (prompt);
    }

  for (i = 0; i < length; i++)
    {
      g_free (auth_info_required[i]);
      g_free (auth_info_default[i]);
      g_free (auth_info_display[i]);
    }

  g_free (auth_info_required);
  g_free (auth_info_default);
  g_free (auth_info_display);
  g_free (auth_info_visible);
  g_free (key);

  return G_SOURCE_REMOVE;
}

static void
cups_dispatch_add_poll (GSource *source)
{
  GtkPrintCupsDispatchWatch *dispatch;
  GtkCupsPollState poll_state;

  dispatch = (GtkPrintCupsDispatchWatch *) source;

  poll_state = gtk_cups_request_get_poll_state (dispatch->request);

  /* Remove the old source if the poll state changed. */
  if (poll_state != dispatch->poll_state && dispatch->data_poll != NULL)
    {
      g_source_remove_poll (source, dispatch->data_poll);
      g_free (dispatch->data_poll);
      dispatch->data_poll = NULL;
    }

  if (dispatch->request->http != NULL)
    {
      if (dispatch->data_poll == NULL)
        {
	  dispatch->data_poll = g_new0 (GPollFD, 1);
	  dispatch->poll_state = poll_state;

	  if (poll_state == GTK_CUPS_HTTP_READ)
	    dispatch->data_poll->events = G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI;
	  else if (poll_state == GTK_CUPS_HTTP_WRITE)
	    dispatch->data_poll->events = G_IO_OUT | G_IO_ERR;
	  else
	    dispatch->data_poll->events = 0;

          dispatch->data_poll->fd = httpGetFd (dispatch->request->http);
          g_source_add_poll (source, dispatch->data_poll);
        }
    }
}

static gboolean
check_auth_info (gpointer user_data)
{
  GtkPrintCupsDispatchWatch *dispatch;
  dispatch = (GtkPrintCupsDispatchWatch *) user_data;

  if (!dispatch->request->need_auth_info)
    {
      if (dispatch->request->auth_info == NULL)
        {
          dispatch->callback (GTK_PRINT_BACKEND (dispatch->backend),
                              gtk_cups_request_get_result (dispatch->request),
                              dispatch->callback_data);
          g_source_destroy ((GSource *) dispatch);
        }
      else
        {
          gint length;
          gint i;

          length = g_strv_length (dispatch->request->auth_info_required);

          gtk_cups_request_ipp_add_strings (dispatch->request,
                                            IPP_TAG_JOB,
                                            IPP_TAG_TEXT,
                                            "auth-info",
                                            length,
                                            NULL,
                                            (const char * const *) dispatch->request->auth_info);

          g_source_attach ((GSource *) dispatch, NULL);
          g_source_unref ((GSource *) dispatch);

          for (i = 0; i < length; i++)
            overwrite_and_free (dispatch->request->auth_info[i]);
          g_free (dispatch->request->auth_info);
          dispatch->request->auth_info = NULL;
        }

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
lookup_auth_info_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GTask                      *task;
  GtkPrintCupsDispatchWatch  *dispatch;
  gchar                     **auth_info;
  GError                     *error = NULL;
  gint                        i;

  task = (GTask *) res;
  dispatch = user_data;
  auth_info = g_task_propagate_pointer (task, &error);

  if (auth_info == NULL)
    {
      if (error != NULL)
        {
          GTK_NOTE (PRINTING,
                    g_print ("Failed to look up auth info: %s\n", error->message));
          g_error_free (error);
        }
      else
        {
          /* Error note should have been shown by the function causing this */
          GTK_NOTE (PRINTING, g_print ("Failed to look up auth info.\n"));
        }
      dispatch->backend->authentication_lock = FALSE;
      g_object_unref (task);
      request_auth_info (dispatch);
      return;
    }

  gtk_print_backend_cups_set_password (GTK_PRINT_BACKEND (dispatch->backend),
                                       dispatch->request->auth_info_required, auth_info,
                                       FALSE);
  for (i = 0; auth_info[i] != NULL; i++)
    {
      overwrite_and_free (auth_info[i]);
      auth_info[i] = NULL;
    }
  g_clear_pointer (auth_info, g_free);

  g_object_unref (task);
}

static void
lookup_auth_info (gpointer user_data)
{
  GtkPrintCupsDispatchWatch  *dispatch;
  gsize                       length,
                              i;
  gboolean                    need_secret_auth_info = FALSE;
  const gchar                *printer_uri;

  dispatch = user_data;

  if (dispatch->backend->authentication_lock)
    return;

  length = g_strv_length (dispatch->request->auth_info_required);

  for (i = 0; i < length; i++)
    {
      if (g_strcmp0 (dispatch->request->auth_info_required[i], "password") == 0)
        {
          need_secret_auth_info = TRUE;
          break;
        }
    }

  g_idle_add (check_auth_info, user_data);

  if (dispatch->backend->secrets_service_available && need_secret_auth_info)
    {
      dispatch->backend->authentication_lock = TRUE;
      printer_uri = gtk_cups_request_ipp_get_string (dispatch->request,
                                                     IPP_TAG_URI,
                                                     "printer-uri");
      gtk_cups_secrets_service_query_task (dispatch->backend,
                                           dispatch->backend->secrets_service_cancellable,
                                           lookup_auth_info_cb,
                                           dispatch,
                                           printer_uri,
                                           dispatch->request->auth_info_required);
      return;
    }

  request_auth_info (user_data);
}

static gboolean
request_auth_info (gpointer user_data)
{
  GtkPrintCupsDispatchWatch  *dispatch;
  const char                 *job_title;
  const char                 *printer_uri;
  gchar                      *prompt = NULL;
  char                       *printer_name = NULL;
  gint                        length;
  gint                        i;
  gboolean                   *auth_info_visible = NULL;
  gchar                     **auth_info_default = NULL;
  gchar                     **auth_info_display = NULL;

  dispatch = (GtkPrintCupsDispatchWatch *) user_data;

  if (dispatch->backend->authentication_lock)
    return FALSE;

  job_title = gtk_cups_request_ipp_get_string (dispatch->request, IPP_TAG_NAME, "job-name");
  printer_uri = gtk_cups_request_ipp_get_string (dispatch->request, IPP_TAG_URI, "printer-uri");
  length = g_strv_length (dispatch->request->auth_info_required);

  auth_info_visible = g_new0 (gboolean, length);
  auth_info_default = g_new0 (gchar *, length + 1);
  auth_info_display = g_new0 (gchar *, length + 1);

  for (i = 0; i < length; i++)
    {
      if (g_strcmp0 (dispatch->request->auth_info_required[i], "domain") == 0)
        {
          auth_info_display[i] = g_strdup (_("Domain:"));
          auth_info_default[i] = g_strdup ("WORKGROUP");
          auth_info_visible[i] = TRUE;
        }
      else if (g_strcmp0 (dispatch->request->auth_info_required[i], "username") == 0)
        {
          auth_info_display[i] = g_strdup (_("Username:"));
          if (dispatch->backend->username != NULL)
            auth_info_default[i] = g_strdup (dispatch->backend->username);
          else
            auth_info_default[i] = g_strdup (cupsUser ());
          auth_info_visible[i] = TRUE;
        }
      else if (g_strcmp0 (dispatch->request->auth_info_required[i], "password") == 0)
        {
          auth_info_display[i] = g_strdup (_("Password:"));
          auth_info_visible[i] = FALSE;
        }
    }

  if (printer_uri != NULL && strrchr (printer_uri, '/') != NULL)
    printer_name = g_strdup (strrchr (printer_uri, '/') + 1);

  dispatch->backend->authentication_lock = TRUE;

  if (job_title != NULL)
    {
      if (printer_name != NULL)
        prompt = g_strdup_printf ( _("Authentication is required to print document “%s” on printer %s"), job_title, printer_name);
      else
        prompt = g_strdup_printf ( _("Authentication is required to print document “%s”"), job_title);
    }
  else
    {
      if (printer_name != NULL)
        prompt = g_strdup_printf ( _("Authentication is required to print this document on printer %s"), printer_name);
      else
        prompt = g_strdup ( _("Authentication is required to print this document"));
    }

  g_signal_emit_by_name (dispatch->backend, "request-password",
                         dispatch->request->auth_info_required,
                         auth_info_default,
                         auth_info_display,
                         auth_info_visible,
                         prompt,
                         dispatch->backend->secrets_service_available);

  for (i = 0; i < length; i++)
    {
      g_free (auth_info_default[i]);
      g_free (auth_info_display[i]);
    }

  g_free (auth_info_default);
  g_free (auth_info_display);
  g_free (printer_name);
  g_free (prompt);

  return FALSE;
}

static gboolean
cups_dispatch_watch_check (GSource *source)
{
  GtkPrintCupsDispatchWatch *dispatch;
  GtkCupsPollState poll_state;
  gboolean result;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s <source %p>\n", G_STRFUNC, source));

  dispatch = (GtkPrintCupsDispatchWatch *) source;

  poll_state = gtk_cups_request_get_poll_state (dispatch->request);

  if (poll_state != GTK_CUPS_HTTP_IDLE && !dispatch->request->need_password)
    if (!(dispatch->data_poll->revents & dispatch->data_poll->events))
       return FALSE;

  result = gtk_cups_request_read_write (dispatch->request, FALSE);
  if (result && dispatch->data_poll != NULL)
    {
      g_source_remove_poll (source, dispatch->data_poll);
      g_free (dispatch->data_poll);
      dispatch->data_poll = NULL;
    }

  if (dispatch->request->need_password && dispatch->request->password_state != GTK_CUPS_PASSWORD_REQUESTED)
    {
      dispatch->request->need_password = FALSE;
      g_idle_add (request_password, dispatch);
      result = FALSE;
    }

  return result;
}

static gboolean
cups_dispatch_watch_prepare (GSource *source,
			     gint    *timeout_)
{
  GtkPrintCupsDispatchWatch *dispatch;
  gboolean result;

  dispatch = (GtkPrintCupsDispatchWatch *) source;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s <source %p>\n", G_STRFUNC, source));

  *timeout_ = -1;

  result = gtk_cups_request_read_write (dispatch->request, TRUE);

  cups_dispatch_add_poll (source);

  return result;
}

static gboolean
cups_dispatch_watch_dispatch (GSource     *source,
			      GSourceFunc  callback,
			      gpointer     user_data)
{
  GtkPrintCupsDispatchWatch *dispatch;
  GtkPrintCupsResponseCallbackFunc ep_callback;
  GtkCupsResult *result;

  g_assert (callback != NULL);

  ep_callback = (GtkPrintCupsResponseCallbackFunc) callback;

  dispatch = (GtkPrintCupsDispatchWatch *) source;

  result = gtk_cups_request_get_result (dispatch->request);

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s <source %p>\n", G_STRFUNC, source));

  if (gtk_cups_result_is_error (result))
    {
      GTK_NOTE (PRINTING,
                g_print("Error result: %s (type %i, status %i, code %i)\n",
                        gtk_cups_result_get_error_string (result),
                        gtk_cups_result_get_error_type (result),
                        gtk_cups_result_get_error_status (result),
                        gtk_cups_result_get_error_code (result)));
     }

  ep_callback (GTK_PRINT_BACKEND (dispatch->backend), result, user_data);

  return FALSE;
}

static void
cups_dispatch_watch_finalize (GSource *source)
{
  GtkPrintCupsDispatchWatch *dispatch;
  GtkCupsResult *result;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s <source %p>\n", G_STRFUNC, source));

  dispatch = (GtkPrintCupsDispatchWatch *) source;

  result = gtk_cups_request_get_result (dispatch->request);
  if (gtk_cups_result_get_error_type (result) == GTK_CUPS_ERROR_AUTH)
    {
      const gchar *username;
      gchar        hostname[HTTP_MAX_URI];
      gchar       *key;

      httpGetHostname (dispatch->request->http, hostname, sizeof (hostname));
      if (is_address_local (hostname))
        strcpy (hostname, "localhost");

      if (dispatch->backend->username != NULL)
        username = dispatch->backend->username;
      else
        username = cupsUser ();

      key = g_strconcat (username, "@", hostname, NULL);
      GTK_NOTE (PRINTING,
                g_print ("CUPS backend: removing stored password for %s\n", key));
      g_hash_table_remove (dispatch->backend->auth, key);
      g_free (key);

      if (dispatch->backend)
        dispatch->backend->authentication_lock = FALSE;
    }

  gtk_cups_request_free (dispatch->request);

  if (dispatch->backend)
    {
      /* We need to unref this at idle time, because it might be the
       * last reference to this module causing the code to be
       * unloaded (including this particular function!)
       * Update: Doing this at idle caused a deadlock taking the
       * mainloop context lock while being in a GSource callout for
       * multithreaded apps. So, for now we just disable unloading
       * of print backends. See _gtk_print_backend_create for the
       * disabling.
       */

      dispatch->backend->requests = g_list_remove (dispatch->backend->requests, dispatch);


      g_object_unref (dispatch->backend);
      dispatch->backend = NULL;
    }

  if (dispatch->data_poll)
    {
      g_source_remove_poll (source, dispatch->data_poll);
      g_free (dispatch->data_poll);
      dispatch->data_poll = NULL;
    }
}

static GSourceFuncs _cups_dispatch_watch_funcs = {
  cups_dispatch_watch_prepare,
  cups_dispatch_watch_check,
  cups_dispatch_watch_dispatch,
  cups_dispatch_watch_finalize
};


static void
cups_request_execute (GtkPrintBackendCups              *print_backend,
                      GtkCupsRequest                   *request,
                      GtkPrintCupsResponseCallbackFunc  callback,
                      gpointer                          user_data,
                      GDestroyNotify                    notify)
{
  GtkPrintCupsDispatchWatch *dispatch;

  dispatch = (GtkPrintCupsDispatchWatch *) g_source_new (&_cups_dispatch_watch_funcs,
                                                         sizeof (GtkPrintCupsDispatchWatch));
  g_source_set_name (&dispatch->source, "GTK+ CUPS backend");

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s <source %p> - Executing cups request on server '%s' and resource '%s'\n", G_STRFUNC, dispatch, request->server, request->resource));

  dispatch->request = request;
  dispatch->backend = g_object_ref (print_backend);
  dispatch->poll_state = GTK_CUPS_HTTP_IDLE;
  dispatch->data_poll = NULL;
  dispatch->callback = NULL;
  dispatch->callback_data = NULL;

  print_backend->requests = g_list_prepend (print_backend->requests, dispatch);

  g_source_set_callback ((GSource *) dispatch, (GSourceFunc) callback, user_data, notify);

  if (request->need_auth_info)
    {
      dispatch->callback = callback;
      dispatch->callback_data = user_data;
      lookup_auth_info (dispatch);
    }
  else
    {
      g_source_attach ((GSource *) dispatch, NULL);
      g_source_unref ((GSource *) dispatch);
    }
}

#if 0
static void
cups_request_printer_info_cb (GtkPrintBackendCups *backend,
                              GtkCupsResult       *result,
                              gpointer             user_data)
{
  ipp_attribute_t *attr;
  ipp_t *response;
  gchar *printer_name;
  GtkPrinterCups *cups_printer;
  GtkPrinter *printer;
  gchar *loc;
  gchar *desc;
  gchar *state_msg;
  int job_count;
  gboolean status_changed;

  g_assert (GTK_IS_PRINT_BACKEND_CUPS (backend));

  printer_name = (gchar *)user_data;
  printer = gtk_print_backend_find_printer (GTK_PRINT_BACKEND (backend),
					    printer_name);

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s - Got printer info for printer '%s'\n", G_STRFUNC, printer_name));

  if (!printer)
    {
      GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: Could not find printer called '%s'\n", printer_name));
      return;
    }

  cups_printer = GTK_PRINTER_CUPS (printer);

  if (gtk_cups_result_is_error (result))
    {
      if (gtk_printer_is_new (printer))
	{
	  gtk_print_backend_remove_printer (GTK_PRINT_BACKEND (backend),
					    printer);
	  return;
	}
      else
	return; /* TODO: mark as inactive printer */
    }

  response = gtk_cups_result_get_response (result);

  /* TODO: determine printer type and use correct icon */
  gtk_printer_set_icon_name (printer, "printer");

  state_msg = "";
  loc = "";
  desc = "";
  job_count = 0;
  for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
      if (!attr->name)
        continue;

      _CUPS_MAP_ATTR_STR (attr, loc, "printer-location");
      _CUPS_MAP_ATTR_STR (attr, desc, "printer-info");
      _CUPS_MAP_ATTR_STR (attr, state_msg, "printer-state-message");
      _CUPS_MAP_ATTR_INT (attr, cups_printer->state, "printer-state");
      _CUPS_MAP_ATTR_INT (attr, job_count, "queued-job-count");
    }

  status_changed = gtk_printer_set_job_count (printer, job_count);

  status_changed |= gtk_printer_set_location (printer, loc);
  status_changed |= gtk_printer_set_description (printer, desc);
  status_changed |= gtk_printer_set_state_message (printer, state_msg);

  if (status_changed)
    g_signal_emit_by_name (GTK_PRINT_BACKEND (backend),
			   "printer-status-changed", printer);
}

static void
cups_request_printer_info (GtkPrintBackendCups *print_backend,
                           const gchar         *printer_name)
{
  GtkCupsRequest *request;
  gchar *printer_uri;
  static const char * const pattrs[] =	/* Attributes we're interested in */
    {
      "printer-location",
      "printer-info",
      "printer-state-message",
      "printer-state",
      "queued-job-count",
      "job-sheets-supported",
      "job-sheets-default"
    };

  request = gtk_cups_request_new_with_username (NULL,
                                                GTK_CUPS_POST,
                                                IPP_GET_PRINTER_ATTRIBUTES,
                                                NULL,
                                                NULL,
                                                NULL,
                                                print_backend->username);

  printer_uri = g_strdup_printf ("ipp://localhost/printers/%s",
                                  printer_name);
  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                                   "printer-uri", NULL, printer_uri);

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s - Requesting printer info for URI '%s'\n", G_STRFUNC, printer_uri));

  g_free (printer_uri);

  gtk_cups_request_ipp_add_strings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
				    "requested-attributes", G_N_ELEMENTS (pattrs),
				    NULL, pattrs);

  cups_request_execute (print_backend,
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_request_printer_info_cb,
                        g_strdup (printer_name),
                        (GDestroyNotify) g_free);
}
#endif

typedef struct {
  GtkPrintBackendCups *print_backend;
  GtkPrintJob *job;
  int job_id;
  int counter;
} CupsJobPollData;

static void
job_object_died	(gpointer  user_data,
		 GObject  *where_the_object_was)
{
  CupsJobPollData *data = user_data;
  data->job = NULL;
}

static void
cups_job_poll_data_free (CupsJobPollData *data)
{
  if (data->job)
    g_object_weak_unref (G_OBJECT (data->job), job_object_died, data);

  g_free (data);
}

static void
cups_request_job_info_cb (GtkPrintBackendCups *print_backend,
			  GtkCupsResult       *result,
			  gpointer             user_data)
{
  CupsJobPollData *data = user_data;
  ipp_attribute_t *attr;
  ipp_t *response;
  int state;
  gboolean done;

  gdk_threads_enter ();

  if (data->job == NULL)
    {
      cups_job_poll_data_free (data);
      goto done;
    }

  data->counter++;

  response = gtk_cups_result_get_response (result);

  state = 0;

#ifdef HAVE_CUPS_API_1_6
  attr = ippFindAttribute (response, "job-state", IPP_TAG_ENUM);
  state = ippGetInteger (attr, 0);
#else
  for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
      if (!attr->name)
        continue;

      _CUPS_MAP_ATTR_INT (attr, state, "job-state");
    }
#endif

  done = FALSE;
  switch (state)
    {
    case IPP_JOB_PENDING:
    case IPP_JOB_HELD:
    case IPP_JOB_STOPPED:
      gtk_print_job_set_status (data->job,
				GTK_PRINT_STATUS_PENDING);
      break;
    case IPP_JOB_PROCESSING:
      gtk_print_job_set_status (data->job,
				GTK_PRINT_STATUS_PRINTING);
      break;
    default:
    case IPP_JOB_CANCELLED:
    case IPP_JOB_ABORTED:
      gtk_print_job_set_status (data->job,
				GTK_PRINT_STATUS_FINISHED_ABORTED);
      done = TRUE;
      break;
    case 0:
    case IPP_JOB_COMPLETED:
      gtk_print_job_set_status (data->job,
				GTK_PRINT_STATUS_FINISHED);
      done = TRUE;
      break;
    }

  if (!done && data->job != NULL)
    {
      guint32 timeout;
      guint id;

      if (data->counter < 5)
	timeout = 100;
      else if (data->counter < 10)
	timeout = 500;
      else
	timeout = 1000;

      id = g_timeout_add (timeout, cups_job_info_poll_timeout, data);
      g_source_set_name_by_id (id, "[gtk+] cups_job_info_poll_timeout");
    }
  else
    cups_job_poll_data_free (data);

done:
  gdk_threads_leave ();
}

static void
cups_request_job_info (CupsJobPollData *data)
{
  GtkCupsRequest *request;
  gchar *job_uri;

  request = gtk_cups_request_new_with_username (NULL,
                                                GTK_CUPS_POST,
                                                IPP_GET_JOB_ATTRIBUTES,
                                                NULL,
                                                NULL,
                                                NULL,
                                                data->print_backend->username);

  job_uri = g_strdup_printf ("ipp://localhost/jobs/%d", data->job_id);
  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                                   "job-uri", NULL, job_uri);
  g_free (job_uri);

  cups_request_execute (data->print_backend,
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_request_job_info_cb,
                        data,
                        NULL);
}

static gboolean
cups_job_info_poll_timeout (gpointer user_data)
{
  CupsJobPollData *data = user_data;

  if (data->job == NULL)
    cups_job_poll_data_free (data);
  else
    cups_request_job_info (data);

  return G_SOURCE_REMOVE;
}

static void
cups_begin_polling_info (GtkPrintBackendCups *print_backend,
			 GtkPrintJob         *job,
			 gint                 job_id)
{
  CupsJobPollData *data;

  data = g_new0 (CupsJobPollData, 1);

  data->print_backend = print_backend;
  data->job = job;
  data->job_id = job_id;
  data->counter = 0;

  g_object_weak_ref (G_OBJECT (job), job_object_died, data);

  cups_request_job_info (data);
}

static void
mark_printer_inactive (GtkPrinter      *printer,
                       GtkPrintBackend *backend)
{
  gtk_printer_set_is_active (printer, FALSE);
  g_signal_emit_by_name (backend, "printer-removed", printer);
}

static gint
find_printer (GtkPrinter  *printer,
	      const gchar *find_name)
{
  const gchar *printer_name;

  printer_name = gtk_printer_get_name (printer);
  return g_ascii_strcasecmp (printer_name, find_name);
}
/* Printer messages we're interested in */
static const char * const printer_messages[] =
  {
    "toner-low",
    "toner-empty",
    "developer-low",
    "developer-empty",
    "marker-supply-low",
    "marker-supply-empty",
    "cover-open",
    "door-open",
    "media-low",
    "media-empty",
    "offline",
    "other"
  };
/* Our translatable versions of the printer messages */
static const char * printer_strings[] =
  {
    N_("Printer “%s” is low on toner."),
    N_("Printer “%s” has no toner left."),
    /* Translators: "Developer" like on photo development context */
    N_("Printer “%s” is low on developer."),
    /* Translators: "Developer" like on photo development context */
    N_("Printer “%s” is out of developer."),
    /* Translators: "marker" is one color bin of the printer */
    N_("Printer “%s” is low on at least one marker supply."),
    /* Translators: "marker" is one color bin of the printer */
    N_("Printer “%s” is out of at least one marker supply."),
    N_("The cover is open on printer “%s”."),
    N_("The door is open on printer “%s”."),
    N_("Printer “%s” is low on paper."),
    N_("Printer “%s” is out of paper."),
    N_("Printer “%s” is currently offline."),
    N_("There is a problem on printer “%s”.")
  };

/* Attributes we're interested in for printers */
static const char * const printer_attrs[] =
  {
    "printer-name",
    "printer-uri-supported",
    "member-uris",
    "printer-location",
    "printer-info",
    "printer-state-message",
    "printer-state-reasons",
    "printer-state",
    "queued-job-count",
    "printer-is-accepting-jobs",
    "job-sheets-supported",
    "job-sheets-default",
    "printer-type",
    "auth-info-required",
    "number-up-default",
    "ipp-versions-supported",
    "multiple-document-handling-supported",
    "copies-supported",
    "number-up-supported"
  };

typedef enum
  {
    GTK_PRINTER_STATE_LEVEL_NONE = 0,
    GTK_PRINTER_STATE_LEVEL_INFO = 1,
    GTK_PRINTER_STATE_LEVEL_WARNING = 2,
    GTK_PRINTER_STATE_LEVEL_ERROR = 3
  } PrinterStateLevel;

typedef struct
{
  const gchar *printer_name;
  const gchar *printer_uri;
  const gchar *member_uris;
  const gchar *location;
  const gchar *description;
  gchar *state_msg;
  const gchar *reason_msg;
  PrinterStateLevel reason_level;
  gint state;
  gint job_count;
  gboolean is_paused;
  gboolean is_accepting_jobs;
  const gchar *default_cover_before;
  const gchar *default_cover_after;
  gboolean default_printer;
  gboolean got_printer_type;
  gboolean remote_printer;
#ifdef HAVE_CUPS_API_1_6
  gboolean avahi_printer;
#endif
  gchar  **auth_info_required;
  gint     default_number_up;
  guchar   ipp_version_major;
  guchar   ipp_version_minor;
  gboolean supports_copies;
  gboolean supports_collate;
  gboolean supports_number_up;
} PrinterSetupInfo;

static void
get_ipp_version (const char *ipp_version_string,
                 guchar     *ipp_version_major,
                 guchar     *ipp_version_minor)
{
  gchar **ipp_version_strv;
  gchar  *endptr;

  *ipp_version_major = 1;
  *ipp_version_minor = 1;

  if (ipp_version_string)
    {
      ipp_version_strv = g_strsplit (ipp_version_string, ".", 0);

      if (ipp_version_strv)
        {
          if (g_strv_length (ipp_version_strv) == 2)
            {
              *ipp_version_major = (guchar) g_ascii_strtoull (ipp_version_strv[0], &endptr, 10);
              if (endptr == ipp_version_strv[0])
                *ipp_version_major = 1;

              *ipp_version_minor = (guchar) g_ascii_strtoull (ipp_version_strv[1], &endptr, 10);
              if (endptr == ipp_version_strv[1])
                *ipp_version_minor = 1;
            }

          g_strfreev (ipp_version_strv);
        }
    }
}

static void
get_server_ipp_version (guchar *ipp_version_major,
                        guchar *ipp_version_minor)
{
  *ipp_version_major = 1;
  *ipp_version_minor = 1;

  if (IPP_VERSION && strlen (IPP_VERSION) == 2)
    {
      *ipp_version_major = (unsigned char) IPP_VERSION[0];
      *ipp_version_minor = (unsigned char) IPP_VERSION[1];
    }
}

static gint
ipp_version_cmp (guchar ipp_version_major1,
                 guchar ipp_version_minor1,
                 guchar ipp_version_major2,
                 guchar ipp_version_minor2)
{
  if (ipp_version_major1 == ipp_version_major2 &&
      ipp_version_minor1 == ipp_version_minor2)
    {
      return 0;
    }
  else if (ipp_version_major1 < ipp_version_major2 ||
           (ipp_version_major1 == ipp_version_major2 &&
            ipp_version_minor1 < ipp_version_minor2))
    {
      return -1;
    }
  else
    {
      return 1;
    }
}

static void
cups_printer_handle_attribute (GtkPrintBackendCups *cups_backend,
			       ipp_attribute_t *attr,
			       PrinterSetupInfo *info)
{
  gint i, j;
  if (strcmp (ippGetName (attr), "printer-name") == 0 &&
      ippGetValueTag (attr) == IPP_TAG_NAME)
    info->printer_name = ippGetString (attr, 0, NULL);
  else if (strcmp (ippGetName (attr), "printer-uri-supported") == 0 &&
	   ippGetValueTag (attr) == IPP_TAG_URI)
    info->printer_uri = ippGetString (attr, 0, NULL);
  else if (strcmp (ippGetName (attr), "member-uris") == 0 &&
	   ippGetValueTag (attr) == IPP_TAG_URI)
    info->member_uris = ippGetString (attr, 0, NULL);
  else if (strcmp (ippGetName (attr), "printer-location") == 0)
    info->location = ippGetString (attr, 0, NULL);
  else if (strcmp (ippGetName (attr), "printer-info") == 0)
    info->description = ippGetString (attr, 0, NULL);
  else if (strcmp (ippGetName (attr), "printer-state-message") == 0)
    info->state_msg = g_strdup (ippGetString (attr, 0, NULL));
  else if (strcmp (ippGetName (attr), "printer-state-reasons") == 0)
    /* Store most important reason to reason_msg and set
       its importance at printer_state_reason_level */
    {
      for (i = 0; i < ippGetCount (attr); i++)
	{
	  if (strcmp (ippGetString (attr, i, NULL), "none") != 0)
	    {
	      gboolean interested_in = FALSE;
	      /* Sets is_paused flag for paused printer. */
	      if (strcmp (ippGetString (attr, i, NULL), "paused") == 0)
		{
		  info->is_paused = TRUE;
		}

	      for (j = 0; j < G_N_ELEMENTS (printer_messages); j++)
		if (strncmp (ippGetString (attr, i, NULL), printer_messages[j],
			     strlen (printer_messages[j])) == 0)
		  {
		    interested_in = TRUE;
		    break;
		  }

	      if (interested_in)
		{
		  if (g_str_has_suffix (ippGetString (attr, i, NULL), "-report"))
		    {
		      if (info->reason_level <= GTK_PRINTER_STATE_LEVEL_INFO)
			{
			  info->reason_msg = ippGetString (attr, i, NULL);
			  info->reason_level = GTK_PRINTER_STATE_LEVEL_INFO;
			}
		    }
		  else if (g_str_has_suffix (ippGetString (attr, i, NULL), "-warning"))
		    {
		      if (info->reason_level <= GTK_PRINTER_STATE_LEVEL_WARNING)
			{
			  info->reason_msg = ippGetString (attr, i, NULL);
			  info->reason_level = GTK_PRINTER_STATE_LEVEL_WARNING;
			}
		    }
		  else  /* It is error in the case of no suffix. */
		    {
		      info->reason_msg = ippGetString (attr, i, NULL);
		      info->reason_level = GTK_PRINTER_STATE_LEVEL_ERROR;
		    }
		}
	    }
	}
    }
  else if (strcmp (ippGetName (attr), "printer-state") == 0)
    info->state = ippGetInteger (attr, 0);
  else if (strcmp (ippGetName (attr), "queued-job-count") == 0)
    info->job_count = ippGetInteger (attr, 0);
  else if (strcmp (ippGetName (attr), "printer-is-accepting-jobs") == 0)
    {
      if (ippGetBoolean (attr, 0) == 1)
	info->is_accepting_jobs = TRUE;
      else
	info->is_accepting_jobs = FALSE;
    }
  else if (strcmp (ippGetName (attr), "job-sheets-supported") == 0)
    {
      if (cups_backend->covers == NULL)
	{
	  cups_backend->number_of_covers = ippGetCount (attr);
	  cups_backend->covers = g_new (char *, cups_backend->number_of_covers + 1);
	  for (i = 0; i < cups_backend->number_of_covers; i++)
	    cups_backend->covers[i] = g_strdup (ippGetString (attr, i, NULL));
	  cups_backend->covers[cups_backend->number_of_covers] = NULL;
	}
    }
  else if (strcmp (ippGetName (attr), "job-sheets-default") == 0)
    {
      if (ippGetCount (attr) == 2)
	{
	  info->default_cover_before = ippGetString (attr, 0, NULL);
	  info->default_cover_after = ippGetString (attr, 1, NULL);
	}
    }
  else if (strcmp (ippGetName (attr), "printer-type") == 0)
    {
      info->got_printer_type = TRUE;
      if (ippGetInteger (attr, 0) & 0x00020000)
	info->default_printer = TRUE;
      else
	info->default_printer = FALSE;

      if (ippGetInteger (attr, 0) & 0x00000002)
	info->remote_printer = TRUE;
      else
	info->remote_printer = FALSE;
    }
  else if (strcmp (ippGetName (attr), "auth-info-required") == 0)
    {
      if (strcmp (ippGetString (attr, 0, NULL), "none") != 0)
	{
	  info->auth_info_required = g_new0 (gchar *, ippGetCount (attr) + 1);
	  for (i = 0; i < ippGetCount (attr); i++)
	    info->auth_info_required[i] = g_strdup (ippGetString (attr, i, NULL));
	}
    }
  else if (strcmp (ippGetName (attr), "number-up-default") == 0)
    {
      info->default_number_up = ippGetInteger (attr, 0);
    }
  else if (g_strcmp0 (ippGetName (attr), "ipp-versions-supported") == 0)
    {
      guchar server_ipp_version_major;
      guchar server_ipp_version_minor;
      guchar ipp_version_major;
      guchar ipp_version_minor;

      get_server_ipp_version (&server_ipp_version_major,
                              &server_ipp_version_minor);

      for (i = 0; i < ippGetCount (attr); i++)
        {
          get_ipp_version (ippGetString (attr, i, NULL),
                           &ipp_version_major,
                           &ipp_version_minor);

          if (ipp_version_cmp (ipp_version_major,
                               ipp_version_minor,
                               info->ipp_version_major,
                               info->ipp_version_minor) > 0 &&
              ipp_version_cmp (ipp_version_major,
                               ipp_version_minor,
                               server_ipp_version_major,
                               server_ipp_version_minor) <= 0)
            {
              info->ipp_version_major = ipp_version_major;
              info->ipp_version_minor = ipp_version_minor;
            }
        }
    }
  else if (g_strcmp0 (ippGetName (attr), "number-up-supported") == 0)
    {
      if (ippGetCount (attr) == 6)
        {
          info->supports_number_up = TRUE;
        }
    }
  else if (g_strcmp0 (ippGetName (attr), "copies-supported") == 0)
    {
      int upper = 1;

      ippGetRange (attr, 0, &upper);
      if (upper > 1)
        {
          info->supports_copies = TRUE;
        }
    }
  else if (g_strcmp0 (ippGetName (attr), "multiple-document-handling-supported") == 0)
    {
      for (i = 0; i < ippGetCount (attr); i++)
        {
          if (g_strcmp0 (ippGetString (attr, i, NULL), "separate-documents-collated-copies") == 0)
            {
              info->supports_collate = TRUE;
            }
        }
    }
  else
    {
      GTK_NOTE (PRINTING,
		g_print ("CUPS Backend: Attribute %s ignored", ippGetName (attr)));
    }
}

static GtkPrinter*
cups_create_printer (GtkPrintBackendCups *cups_backend,
		     PrinterSetupInfo *info)
{
  GtkPrinterCups *cups_printer;
  GtkPrinter *printer;
  GtkPrintBackend *backend = GTK_PRINT_BACKEND (cups_backend);
  char uri[HTTP_MAX_URI];	/* Printer URI */
  char method[HTTP_MAX_URI];	/* Method/scheme name */
  char username[HTTP_MAX_URI];	/* Username:password */
  char hostname[HTTP_MAX_URI];	/* Hostname */
  char resource[HTTP_MAX_URI];	/* Resource name */
  int  port;			/* Port number */
  char *cups_server;            /* CUPS server */

#ifdef HAVE_COLORD
#ifdef HAVE_CUPS_API_1_6
  if (info->avahi_printer)
    cups_printer = gtk_printer_cups_new (info->printer_name,
					 backend,
					 NULL);
  else
#endif
    cups_printer = gtk_printer_cups_new (info->printer_name,
					 backend,
					 cups_backend->colord_client);
#else
  cups_printer = gtk_printer_cups_new (info->printer_name, backend, NULL);
#endif

  cups_printer->device_uri = g_strdup_printf ("/printers/%s",
					      info->printer_name);

  /* Check to see if we are looking at a class */
  if (info->member_uris)
    {
      cups_printer->printer_uri = g_strdup (info->member_uris);
      /* TODO if member_uris is a class we need to recursivly find a printer */
      GTK_NOTE (PRINTING,
		g_print ("CUPS Backend: Found class with printer %s\n",
			 info->member_uris));
    }
  else
    {
      cups_printer->printer_uri = g_strdup (info->printer_uri);
      GTK_NOTE (PRINTING,
		g_print ("CUPS Backend: Found printer %s\n",
			 info->printer_uri));
    }

  httpSeparateURI (HTTP_URI_CODING_ALL, cups_printer->printer_uri,
		   method, sizeof (method),
		   username, sizeof (username),
		   hostname, sizeof (hostname),
		   &port,
		   resource, sizeof (resource));

  if (strncmp (resource, "/printers/", 10) == 0)
    {
      cups_printer->ppd_name = g_strdup (resource + 10);
      GTK_NOTE (PRINTING,
		g_print ("CUPS Backend: Setting ppd name '%s' for printer/class '%s'\n", cups_printer->ppd_name, info->printer_name));
    }

  gethostname (uri, sizeof (uri));
  cups_server = g_strdup (cupsServer());

  if (strcasecmp (uri, hostname) == 0)
    strcpy (hostname, "localhost");

  /* if the cups server is local and listening at a unix domain socket
   * then use the socket connection
   */
  if ((strstr (hostname, "localhost") != NULL) &&
      (cups_server[0] == '/'))
    strcpy (hostname, cups_server);

  g_free (cups_server);

  cups_printer->default_cover_before = g_strdup (info->default_cover_before);
  cups_printer->default_cover_after = g_strdup (info->default_cover_after);

  if (info->default_number_up > 0)
    cups_printer->default_number_up = info->default_number_up;

  cups_printer->hostname = g_strdup (hostname);
  cups_printer->port = port;

  cups_printer->auth_info_required = g_strdupv (info->auth_info_required);
  g_strfreev (info->auth_info_required);

  printer = GTK_PRINTER (cups_printer);

  if (cups_backend->default_printer != NULL &&
      strcmp (cups_backend->default_printer, gtk_printer_get_name (printer)) == 0)
    gtk_printer_set_is_default (printer, TRUE);

#ifdef HAVE_CUPS_API_1_6
  cups_printer->avahi_browsed = info->avahi_printer;
#endif

  gtk_print_backend_add_printer (backend, printer);
  return printer;
}

static void
set_printer_icon_name_from_info (GtkPrinter       *printer,
                                 PrinterSetupInfo *info)
{
  /* Set printer icon according to importance
     (none, report, warning, error - report is omitted). */
  if (info->reason_level == GTK_PRINTER_STATE_LEVEL_ERROR)
    gtk_printer_set_icon_name (printer, "printer-error");
  else if (info->reason_level == GTK_PRINTER_STATE_LEVEL_WARNING)
    gtk_printer_set_icon_name (printer, "printer-warning");
  else if (gtk_printer_is_paused (printer))
    gtk_printer_set_icon_name (printer, "printer-paused");
  else
    gtk_printer_set_icon_name (printer, "printer");
}

static void
set_info_state_message (PrinterSetupInfo *info)
{
  gint i;

  if (info->state_msg && strlen (info->state_msg) == 0)
    {
      gchar *tmp_msg2 = NULL;
      if (info->is_paused && !info->is_accepting_jobs)
        /* Translators: this is a printer status. */
        tmp_msg2 = g_strdup ( _("Paused; Rejecting Jobs"));
      if (info->is_paused && info->is_accepting_jobs)
        /* Translators: this is a printer status. */
        tmp_msg2 = g_strdup ( _("Paused"));
      if (!info->is_paused && !info->is_accepting_jobs)
        /* Translators: this is a printer status. */
        tmp_msg2 = g_strdup ( _("Rejecting Jobs"));

      if (tmp_msg2 != NULL)
        {
          g_free (info->state_msg);
          info->state_msg = tmp_msg2;
        }
    }

  /* Set description of the reason and combine it with printer-state-message. */
  if (info->reason_msg)
    {
      gchar *reason_msg_desc = NULL;
      gboolean found = FALSE;

      for (i = 0; i < G_N_ELEMENTS (printer_messages); i++)
        {
          if (strncmp (info->reason_msg, printer_messages[i],
                       strlen (printer_messages[i])) == 0)
            {
              reason_msg_desc = g_strdup_printf (printer_strings[i],
                                                 info->printer_name);
              found = TRUE;
              break;
            }
        }

      if (!found)
        info->reason_level = GTK_PRINTER_STATE_LEVEL_NONE;

      if (info->reason_level >= GTK_PRINTER_STATE_LEVEL_WARNING)
        {
          if (info->state_msg == NULL || info->state_msg[0] == '\0')
            {
              g_free (info->state_msg);
              info->state_msg = reason_msg_desc;
              reason_msg_desc = NULL;
            }
          else
            {
              gchar *tmp_msg = NULL;
              /* Translators: this string connects multiple printer states together. */
              tmp_msg = g_strjoin ( _("; "), info->state_msg,
                                   reason_msg_desc, NULL);
              g_free (info->state_msg);
              info->state_msg = tmp_msg;
            }
        }

      g_free (reason_msg_desc);
    }
}

static void
set_default_printer (GtkPrintBackendCups *cups_backend,
                     const gchar         *default_printer_name)
{
  cups_backend->default_printer = g_strdup (default_printer_name);
  cups_backend->got_default_printer = TRUE;

  if (cups_backend->default_printer != NULL)
    {
      GtkPrinter *default_printer = NULL;
      default_printer = gtk_print_backend_find_printer (GTK_PRINT_BACKEND (cups_backend),
                                                        cups_backend->default_printer);
      if (default_printer != NULL)
        {
          gtk_printer_set_is_default (default_printer, TRUE);
          g_signal_emit_by_name (GTK_PRINT_BACKEND (cups_backend),
                                 "printer-status-changed", default_printer);
        }
    }
}

#ifdef HAVE_CUPS_API_1_6
static void
cups_request_avahi_printer_info_cb (GtkPrintBackendCups *cups_backend,
                                    GtkCupsResult       *result,
                                    gpointer             user_data)
{
  PrinterSetupInfo *info = g_slice_new0 (PrinterSetupInfo);
  GtkPrintBackend  *backend = GTK_PRINT_BACKEND (cups_backend);
  ipp_attribute_t  *attr;
  GtkPrinter       *printer;
  gboolean          status_changed = FALSE;
  ipp_t            *response;

  gdk_threads_enter ();

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  if (gtk_cups_result_is_error (result))
    {
      GTK_NOTE (PRINTING,
                g_warning ("CUPS Backend: Error getting printer info: %s %d %d",
                           gtk_cups_result_get_error_string (result),
                           gtk_cups_result_get_error_type (result),
                           gtk_cups_result_get_error_code (result)));

      goto done;
    }

  response = gtk_cups_result_get_response (result);
  attr = ippFirstAttribute (response);
  while (attr && ippGetGroupTag (attr) != IPP_TAG_PRINTER)
    attr = ippNextAttribute (response);

  if (attr)
    {
      while (attr && ippGetGroupTag (attr) == IPP_TAG_PRINTER)
        {
          cups_printer_handle_attribute (cups_backend, attr, info);
          attr = ippNextAttribute (response);
        }

      if (info->printer_name && info->printer_uri)
        {
          set_info_state_message (info);

          printer = gtk_print_backend_find_printer (backend, info->printer_name);
          if (printer != NULL &&
              GTK_PRINTER_CUPS (printer)->avahi_browsed)
            {
              g_object_ref (printer);
            }
          else
            {
              goto done;
            }

          if (info->got_printer_type &&
              info->default_printer &&
              cups_backend->avahi_default_printer == NULL)
            cups_backend->avahi_default_printer = g_strdup (info->printer_name);

          gtk_printer_set_is_paused (printer, info->is_paused);
          gtk_printer_set_is_accepting_jobs (printer, info->is_accepting_jobs);

          GTK_PRINTER_CUPS (printer)->remote = info->remote_printer;
          GTK_PRINTER_CUPS (printer)->state = info->state;
          GTK_PRINTER_CUPS (printer)->ipp_version_major = info->ipp_version_major;
          GTK_PRINTER_CUPS (printer)->ipp_version_minor = info->ipp_version_minor;
          GTK_PRINTER_CUPS (printer)->supports_copies = info->supports_copies;
          GTK_PRINTER_CUPS (printer)->supports_collate = info->supports_collate;
          GTK_PRINTER_CUPS (printer)->supports_number_up = info->supports_number_up;
          status_changed = gtk_printer_set_job_count (printer, info->job_count);
          status_changed |= gtk_printer_set_location (printer, info->location);
          status_changed |= gtk_printer_set_description (printer, info->description);
          status_changed |= gtk_printer_set_state_message (printer, info->state_msg);
          status_changed |= gtk_printer_set_is_accepting_jobs (printer, info->is_accepting_jobs);

          set_printer_icon_name_from_info (printer, info);

          gtk_printer_set_has_details (printer, TRUE);
          g_signal_emit_by_name (printer, "details-acquired", TRUE);

          if (status_changed)
            g_signal_emit_by_name (GTK_PRINT_BACKEND (backend),
                                   "printer-status-changed", printer);

          /* The ref is held by GtkPrintBackend, in add_printer() */
          g_object_unref (printer);
        }
    }

done:
  if (!cups_backend->got_default_printer &&
      gtk_print_backend_printer_list_is_done (backend) &&
      cups_backend->avahi_default_printer != NULL)
    {
      set_default_printer (cups_backend, cups_backend->avahi_default_printer);
    }

  g_free (info->state_msg);
  g_slice_free (PrinterSetupInfo, info);

  gdk_threads_leave ();
}

static void
cups_request_avahi_printer_info (const gchar         *printer_uri,
                                 const gchar         *host,
                                 gint                 port,
                                 GtkPrintBackendCups *backend)
{
  GtkCupsRequest *request;
  http_t         *http;

  http = httpConnect (host, port);
  if (http)
    {
      request = gtk_cups_request_new_with_username (http,
                                                    GTK_CUPS_POST,
                                                    IPP_GET_PRINTER_ATTRIBUTES,
                                                    NULL,
                                                    NULL,
                                                    NULL,
                                                    backend->username);

      gtk_cups_request_set_ipp_version (request, 1, 1);

      gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                                       "printer-uri", NULL, printer_uri);

      gtk_cups_request_ipp_add_strings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                                        "requested-attributes", G_N_ELEMENTS (printer_attrs),
                                        NULL, printer_attrs);

      cups_request_execute (backend,
                            request,
                            (GtkPrintCupsResponseCallbackFunc) cups_request_avahi_printer_info_cb,
                            http,
                            (GDestroyNotify) httpClose);
    }
}

typedef struct
{
  gchar               *printer_uri;
  gchar               *location;
  gchar               *host;
  gint                 port;
  gchar               *printer_name;
  gchar               *name;
  gboolean             got_printer_type;
  guint                printer_type;
  gboolean             got_printer_state;
  guint                printer_state;
  gchar               *type;
  gchar               *domain;
  GtkPrintBackendCups *backend;
} AvahiConnectionTestData;

/*
 *  Create new GtkPrinter from informations included in TXT records.
 */
static void
create_cups_printer_from_avahi_data (AvahiConnectionTestData *data)
{
  PrinterSetupInfo *info = g_slice_new0 (PrinterSetupInfo);
  GtkPrinter       *printer;

  info->avahi_printer = TRUE;
  info->printer_name = data->printer_name;
  info->printer_uri = data->printer_uri;

  if (data->got_printer_state)
    info->state = data->printer_state;

  info->got_printer_type = data->got_printer_type;
  if (data->got_printer_type)
    {
      if (data->printer_type & CUPS_PRINTER_DEFAULT)
        info->default_printer = TRUE;
      else
        info->default_printer = FALSE;

      if (data->printer_type & CUPS_PRINTER_REMOTE)
        info->remote_printer = TRUE;
      else
        info->remote_printer = FALSE;

      if (data->printer_type & CUPS_PRINTER_REJECTING)
        info->is_accepting_jobs = FALSE;
      else
        info->is_accepting_jobs = TRUE;

      if (info->default_printer &&
          data->backend->avahi_default_printer == NULL)
        data->backend->avahi_default_printer = g_strdup (info->printer_name);
    }

  set_info_state_message (info);

  printer = gtk_print_backend_find_printer (GTK_PRINT_BACKEND (data->backend), data->printer_name);
  if (printer == NULL)
    {
      printer = cups_create_printer (data->backend, info);

      if (data->got_printer_type)
        {
          gtk_printer_set_is_accepting_jobs (printer, info->is_accepting_jobs);
          GTK_PRINTER_CUPS (printer)->remote = info->remote_printer;

          if (info->default_printer &&
              data->backend->avahi_default_printer == NULL)
            data->backend->avahi_default_printer = g_strdup (info->printer_name);
        }

      if (data->got_printer_state)
        GTK_PRINTER_CUPS (printer)->state = info->state;

      GTK_PRINTER_CUPS (printer)->avahi_name = g_strdup (data->name);
      GTK_PRINTER_CUPS (printer)->avahi_type = g_strdup (data->type);
      GTK_PRINTER_CUPS (printer)->avahi_domain = g_strdup (data->domain);
      g_free (GTK_PRINTER_CUPS (printer)->hostname);
      GTK_PRINTER_CUPS (printer)->hostname = g_strdup (data->host);
      GTK_PRINTER_CUPS (printer)->port = data->port;
      gtk_printer_set_location (printer, data->location);
      gtk_printer_set_state_message (printer, info->state_msg);

      set_printer_icon_name_from_info (printer, info);

      if (!gtk_printer_is_active (printer))
        gtk_printer_set_is_active (printer, TRUE);

      g_signal_emit_by_name (data->backend, "printer-added", printer);
      gtk_printer_set_is_new (printer, FALSE);
      g_signal_emit_by_name (data->backend, "printer-list-changed");

      if (!data->backend->got_default_printer &&
          gtk_print_backend_printer_list_is_done (GTK_PRINT_BACKEND (data->backend)) &&
          data->backend->avahi_default_printer != NULL)
        set_default_printer (data->backend, data->backend->avahi_default_printer);

      /* The ref is held by GtkPrintBackend, in add_printer() */
      g_object_unref (printer);
    }

  g_free (info->state_msg);
  g_slice_free (PrinterSetupInfo, info);
}

static void
avahi_connection_test_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  AvahiConnectionTestData *data = (AvahiConnectionTestData *) user_data;
  GSocketConnection       *connection;

  connection = g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source_object),
                                                       res,
                                                       NULL);
  g_object_unref (source_object);

  if (connection != NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
      g_object_unref (connection);

      create_cups_printer_from_avahi_data (data);
    }

  g_free (data->printer_uri);
  g_free (data->location);
  g_free (data->host);
  g_free (data->printer_name);
  g_free (data->name);
  g_free (data->type);
  g_free (data->domain);
  g_free (data);
}

gboolean
avahi_txt_get_key_value_pair (const gchar  *entry,
                              gchar       **key,
                              gchar       **value)
{
  const gchar *equal_sign;

  *key = NULL;
  *value = NULL;

  if (entry != NULL)
    {
      /* See RFC 6763 section 6.3 */
      equal_sign = strstr (entry, "=");

      if (equal_sign != NULL)
        {
          *key = g_strndup (entry, equal_sign - entry);
          *value = g_strdup (equal_sign + 1);

          return TRUE;
        }
    }

  return FALSE;
}

static void
avahi_service_resolver_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  AvahiConnectionTestData *data;
  GtkPrintBackendCups     *backend;
  const gchar             *name;
  const gchar             *host;
  const gchar             *type;
  const gchar             *domain;
  const gchar             *address;
  const gchar             *protocol_string;
  GVariant                *output;
  GVariant                *txt;
  GVariant                *child;
  guint32                  flags;
  guint16                  port;
  GError                  *error = NULL;
  gchar                   *queue_name = NULL;
  gchar                   *tmp;
  gchar                   *printer_name;
  gchar                   *endptr;
  gchar                   *key;
  gchar                   *value;
  gsize                    length;
  gint                     interface;
  gint                     protocol;
  gint                     aprotocol;
  gint                     i;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output)
    {
      backend = GTK_PRINT_BACKEND_CUPS (user_data);

      g_variant_get (output, "(ii&s&s&s&si&sq@aayu)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &host,
                     &aprotocol,
                     &address,
                     &port,
                     &txt,
                     &flags);

      data = g_new0 (AvahiConnectionTestData, 1);

      for (i = 0; i < g_variant_n_children (txt); i++)
        {
          child = g_variant_get_child_value (txt, i);

          length = g_variant_get_size (child);
          if (length > 0)
            {
              tmp = g_strndup (g_variant_get_data (child), length);
              g_variant_unref (child);

              if (!avahi_txt_get_key_value_pair (tmp, &key, &value))
                {
                  g_free (tmp);
                  continue;
                }

              if (g_strcmp0 (key, "rp") == 0)
                {
                  queue_name = g_strdup (value);

                  printer_name = g_strrstr (queue_name, "/");
                  if (printer_name != NULL)
                    data->printer_name = g_strdup (printer_name + 1);
                  else
                    data->printer_name = g_strdup (queue_name);
                }
              else if (g_strcmp0 (key, "note") == 0)
                {
                  data->location = g_strdup (value);
                }
              else if (g_strcmp0 (key, "printer-type") == 0)
                {
                  endptr = NULL;
                  data->printer_type = g_ascii_strtoull (value, &endptr, 16);
                  if (data->printer_type != 0 || endptr != value)
                    data->got_printer_type = TRUE;
                }
              else if (g_strcmp0 (key, "printer-state") == 0)
                {
                  endptr = NULL;
                  data->printer_state = g_ascii_strtoull (value, &endptr, 10);
                  if (data->printer_state != 0 || endptr != value)
                    data->got_printer_state = TRUE;
                }

              g_clear_pointer (&key, g_free);
              g_clear_pointer (&value, g_free);
              g_free (tmp);
            }
          else
            {
              g_variant_unref (child);
            }
        }

      if (queue_name)
        {
          if (g_strcmp0 (type, "_ipp._tcp") == 0)
            protocol_string = "ipp";
          else
            protocol_string = "ipps";

          if (aprotocol == AVAHI_PROTO_INET6)
            data->printer_uri = g_strdup_printf ("%s://[%s]:%u/%s", protocol_string, address, port, queue_name);
          else
            data->printer_uri = g_strdup_printf ("%s://%s:%u/%s", protocol_string, address, port, queue_name);

          data->host = g_strdup (address);
          data->port = port;

          data->name = g_strdup (name);
          data->type = g_strdup (type);
          data->domain = g_strdup (domain);
          data->backend = backend;

          /* It can happen that the address is not reachable */
          g_socket_client_connect_to_host_async (g_socket_client_new (),
                                                 address,
                                                 port,
                                                 backend->avahi_cancellable,
                                                 avahi_connection_test_cb,
                                                 data);
          g_free (queue_name);
        }
      else
        {
          g_free (data->printer_name);
          g_free (data->location);
          g_free (data);
        }

      g_variant_unref (txt);
      g_variant_unref (output);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static void
avahi_service_browser_signal_handler (GDBusConnection *connection,
                                      const gchar     *sender_name,
                                      const gchar     *object_path,
                                      const gchar     *interface_name,
                                      const gchar     *signal_name,
                                      GVariant        *parameters,
                                      gpointer         user_data)
{
  GtkPrintBackendCups *backend = GTK_PRINT_BACKEND_CUPS (user_data);
  gchar               *name;
  gchar               *type;
  gchar               *domain;
  guint                flags;
  gint                 interface;
  gint                 protocol;

  if (g_strcmp0 (signal_name, "ItemNew") == 0)
    {
      g_variant_get (parameters, "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      if (g_strcmp0 (type, "_ipp._tcp") == 0 ||
          g_strcmp0 (type, "_ipps._tcp") == 0)
        {
          g_dbus_connection_call (backend->dbus_connection,
                                  AVAHI_BUS,
                                  "/",
                                  AVAHI_SERVER_IFACE,
                                  "ResolveService",
                                  g_variant_new ("(iisssiu)",
                                                 interface,
                                                 protocol,
                                                 name,
                                                 type,
                                                 domain,
                                                 AVAHI_PROTO_UNSPEC,
                                                 0),
                                  G_VARIANT_TYPE ("(iissssisqaayu)"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  backend->avahi_cancellable,
                                  avahi_service_resolver_cb,
                                  user_data);
        }
    }
  else if (g_strcmp0 (signal_name, "ItemRemove") == 0)
    {
      g_variant_get (parameters, "(ii&s&s&su)",
                     &interface,
                     &protocol,
                     &name,
                     &type,
                     &domain,
                     &flags);

      if (g_strcmp0 (type, "_ipp._tcp") == 0 ||
          g_strcmp0 (type, "_ipps._tcp") == 0)
        {
          GtkPrinterCups *printer;
          GList          *list;
          GList          *iter;

          list = gtk_print_backend_get_printer_list (GTK_PRINT_BACKEND (backend));
          for (iter = list; iter; iter = iter->next)
            {
              printer = GTK_PRINTER_CUPS (iter->data);
              if (g_strcmp0 (printer->avahi_name, name) == 0 &&
                  g_strcmp0 (printer->avahi_type, type) == 0 &&
                  g_strcmp0 (printer->avahi_domain, domain) == 0)
                {
                  if (g_strcmp0 (gtk_printer_get_name (GTK_PRINTER (printer)),
                                 backend->avahi_default_printer) == 0)
                    g_clear_pointer (&backend->avahi_default_printer, g_free);

                  g_signal_emit_by_name (backend, "printer-removed", printer);
                  gtk_print_backend_remove_printer (GTK_PRINT_BACKEND (backend),
                                                    GTK_PRINTER (printer));
                  g_signal_emit_by_name (backend, "printer-list-changed");
                  break;
                }
            }

          g_list_free (list);
        }
    }
}

static void
avahi_service_browser_new_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GtkPrintBackendCups *cups_backend;
  GVariant            *output;
  GError              *error = NULL;
  gint                 i;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  if (output)
    {
      cups_backend = GTK_PRINT_BACKEND_CUPS (user_data);
      i = cups_backend->avahi_service_browser_paths[0] ? 1 : 0;

      g_variant_get (output, "(o)", &cups_backend->avahi_service_browser_paths[i]);

      cups_backend->avahi_service_browser_subscription_ids[i] =
        g_dbus_connection_signal_subscribe (cups_backend->dbus_connection,
                                            NULL,
                                            AVAHI_SERVICE_BROWSER_IFACE,
                                            NULL,
                                            cups_backend->avahi_service_browser_paths[i],
                                            NULL,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            avahi_service_browser_signal_handler,
                                            user_data,
                                            NULL);

      /*
       * The general subscription for all service browsers is not needed
       * now because we are already subscribed to service browsers
       * specific to _ipp._tcp and _ipps._tcp services.
       */
      if (cups_backend->avahi_service_browser_paths[0] &&
          cups_backend->avahi_service_browser_paths[1] &&
          cups_backend->avahi_service_browser_subscription_id > 0)
        {
          g_dbus_connection_signal_unsubscribe (cups_backend->dbus_connection,
                                                cups_backend->avahi_service_browser_subscription_id);
          cups_backend->avahi_service_browser_subscription_id = 0;
        }

      g_variant_unref (output);
    }
  else
    {
      /*
       * The creation of ServiceBrowser fails with G_IO_ERROR_DBUS_ERROR
       * if Avahi is disabled.
       */
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static void
avahi_create_browsers (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GDBusConnection     *dbus_connection;
  GtkPrintBackendCups *cups_backend;
  GError              *error = NULL;

  dbus_connection = g_bus_get_finish (res, &error);
  if (!dbus_connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Couldn't connect to D-Bus system bus, %s", error->message);

      g_error_free (error);
      return;
    }

  cups_backend = GTK_PRINT_BACKEND_CUPS (user_data);
  cups_backend->dbus_connection = dbus_connection;

  /*
   * We need to subscribe to signals of service browser before
   * we actually create it because it starts to emit them right
   * after its creation.
   */
  cups_backend->avahi_service_browser_subscription_id =
    g_dbus_connection_signal_subscribe  (cups_backend->dbus_connection,
                                         NULL,
                                         AVAHI_SERVICE_BROWSER_IFACE,
                                         NULL,
                                         NULL,
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         avahi_service_browser_signal_handler,
                                         cups_backend,
                                         NULL);

  /*
   * Create service browsers for _ipp._tcp and _ipps._tcp services.
   */
  g_dbus_connection_call (cups_backend->dbus_connection,
                          AVAHI_BUS,
                          "/",
                          AVAHI_SERVER_IFACE,
                          "ServiceBrowserNew",
                          g_variant_new ("(iissu)",
                                         AVAHI_IF_UNSPEC,
                                         AVAHI_PROTO_UNSPEC,
                                         "_ipp._tcp",
                                         "",
                                         0),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cups_backend->avahi_cancellable,
                          avahi_service_browser_new_cb,
                          cups_backend);

  g_dbus_connection_call (cups_backend->dbus_connection,
                          AVAHI_BUS,
                          "/",
                          AVAHI_SERVER_IFACE,
                          "ServiceBrowserNew",
                          g_variant_new ("(iissu)",
                                         AVAHI_IF_UNSPEC,
                                         AVAHI_PROTO_UNSPEC,
                                         "_ipps._tcp",
                                         "",
                                         0),
                          G_VARIANT_TYPE ("(o)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          cups_backend->avahi_cancellable,
                          avahi_service_browser_new_cb,
                          cups_backend);
}

static void
avahi_request_printer_list (GtkPrintBackendCups *cups_backend)
{
  cups_backend->avahi_cancellable = g_cancellable_new ();
  g_bus_get (G_BUS_TYPE_SYSTEM, cups_backend->avahi_cancellable, avahi_create_browsers, cups_backend);
}
#endif

static void
cups_request_printer_list_cb (GtkPrintBackendCups *cups_backend,
                              GtkCupsResult       *result,
                              gpointer             user_data)
{
  GtkPrintBackend *backend = GTK_PRINT_BACKEND (cups_backend);
  ipp_attribute_t *attr;
  ipp_t *response;
  gboolean list_has_changed;
  GList *removed_printer_checklist;
  gchar *remote_default_printer = NULL;
  GList *iter;

  gdk_threads_enter ();

  list_has_changed = FALSE;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  cups_backend->list_printers_pending = FALSE;

  if (gtk_cups_result_is_error (result))
    {
      GTK_NOTE (PRINTING,
                g_warning ("CUPS Backend: Error getting printer list: %s %d %d",
                           gtk_cups_result_get_error_string (result),
                           gtk_cups_result_get_error_type (result),
                           gtk_cups_result_get_error_code (result)));

      if (gtk_cups_result_get_error_type (result) == GTK_CUPS_ERROR_AUTH &&
          gtk_cups_result_get_error_code (result) == 1)
        {
          /* Canceled by user, stop popping up more password dialogs */
          if (cups_backend->list_printers_poll > 0)
            g_source_remove (cups_backend->list_printers_poll);
          cups_backend->list_printers_poll = 0;
          cups_backend->list_printers_attempts = 0;
        }

      goto done;
    }

  /* Gather the names of the printers in the current queue
   * so we may check to see if they were removed
   */
  removed_printer_checklist = gtk_print_backend_get_printer_list (backend);

  response = gtk_cups_result_get_response (result);
#ifdef HAVE_CUPS_API_1_6
  for (attr = ippFirstAttribute (response); attr != NULL;
       attr = ippNextAttribute (response))
    {
      GtkPrinter *printer;
      gboolean status_changed = FALSE;
      GList *node;
      PrinterSetupInfo *info = g_slice_new0 (PrinterSetupInfo);

      /* Skip leading attributes until we hit a printer...
       */
      while (attr != NULL && ippGetGroupTag (attr) != IPP_TAG_PRINTER)
        attr = ippNextAttribute (response);

      if (attr == NULL)
        break;
      while (attr != NULL && ippGetGroupTag (attr) == IPP_TAG_PRINTER)
      {
	cups_printer_handle_attribute (cups_backend, attr, info);
        attr = ippNextAttribute (response);
      }
#else
  for (attr = response->attrs; attr != NULL; attr = attr->next)
    {
      GtkPrinter *printer;
      gboolean status_changed = FALSE;
      GList *node;
      PrinterSetupInfo *info = g_slice_new0 (PrinterSetupInfo);
      info->default_number_up = 1;

      /* Skip leading attributes until we hit a printer...
       */
      while (attr != NULL && ippGetGroupTag (attr) != IPP_TAG_PRINTER)
        attr = attr->next;

      if (attr == NULL)
        break;
      while (attr != NULL && ippGetGroupTag (attr) == IPP_TAG_PRINTER)
      {
	cups_printer_handle_attribute (cups_backend, attr, info);
        attr = attr->next;
      }
#endif

      if (info->printer_name == NULL ||
	  (info->printer_uri == NULL && info->member_uris == NULL))
      {
        if (attr == NULL)
	  break;
	else
          continue;
      }

      if (info->got_printer_type)
        {
          if (info->default_printer && !cups_backend->got_default_printer)
            {
              if (!info->remote_printer)
                {
                  cups_backend->got_default_printer = TRUE;
                  cups_backend->default_printer = g_strdup (info->printer_name);
                }
              else
                {
                  if (remote_default_printer == NULL)
                    remote_default_printer = g_strdup (info->printer_name);
                }
            }
        }
      else
        {
          if (!cups_backend->got_default_printer)
            cups_get_default_printer (cups_backend);
        }

      /* remove name from checklist if it was found */
      node = g_list_find_custom (removed_printer_checklist,
				 info->printer_name,
				 (GCompareFunc) find_printer);
      removed_printer_checklist = g_list_delete_link (removed_printer_checklist,
						      node);

      printer = gtk_print_backend_find_printer (backend, info->printer_name);
      if (!printer)
	{
	  printer = cups_create_printer (cups_backend, info);
	  list_has_changed = TRUE;
	}

      else
	g_object_ref (printer);

      GTK_PRINTER_CUPS (printer)->remote = info->remote_printer;

      gtk_printer_set_is_paused (printer, info->is_paused);
      gtk_printer_set_is_accepting_jobs (printer, info->is_accepting_jobs);

      if (!gtk_printer_is_active (printer))
        {
	  gtk_printer_set_is_active (printer, TRUE);
	  gtk_printer_set_is_new (printer, TRUE);
          list_has_changed = TRUE;
        }

      if (gtk_printer_is_new (printer))
        {
	  g_signal_emit_by_name (backend, "printer-added", printer);

	  gtk_printer_set_is_new (printer, FALSE);
        }

#if 0
      /* Getting printer info with separate requests overwhelms cups
       * when the printer list has more than a handful of printers.
       */
      cups_request_printer_info (cups_backend, gtk_printer_get_name (printer));
#endif

      GTK_PRINTER_CUPS (printer)->state = info->state;
      GTK_PRINTER_CUPS (printer)->ipp_version_major = info->ipp_version_major;
      GTK_PRINTER_CUPS (printer)->ipp_version_minor = info->ipp_version_minor;
      GTK_PRINTER_CUPS (printer)->supports_copies = info->supports_copies;
      GTK_PRINTER_CUPS (printer)->supports_collate = info->supports_collate;
      GTK_PRINTER_CUPS (printer)->supports_number_up = info->supports_number_up;
      status_changed = gtk_printer_set_job_count (printer, info->job_count);
      status_changed |= gtk_printer_set_location (printer, info->location);
      status_changed |= gtk_printer_set_description (printer,
						     info->description);

      set_info_state_message (info);

      status_changed |= gtk_printer_set_state_message (printer, info->state_msg);
      status_changed |= gtk_printer_set_is_accepting_jobs (printer, info->is_accepting_jobs);

      set_printer_icon_name_from_info (printer, info);

      if (status_changed)
        g_signal_emit_by_name (GTK_PRINT_BACKEND (backend),
                               "printer-status-changed", printer);

      /* The ref is held by GtkPrintBackend, in add_printer() */
      g_object_unref (printer);
      g_free (info->state_msg);
      g_slice_free (PrinterSetupInfo, info);

      if (attr == NULL)
        break;
    }

  /* look at the removed printers checklist and mark any printer
     as inactive if it is in the list, emitting a printer_removed signal */
  if (removed_printer_checklist != NULL)
    {
      for (iter = removed_printer_checklist; iter; iter = iter->next)
        {
#ifdef HAVE_CUPS_API_1_6
          if (!GTK_PRINTER_CUPS (iter->data)->avahi_browsed)
#endif
            {
              mark_printer_inactive (GTK_PRINTER (iter->data), backend);
              list_has_changed = TRUE;
            }
        }

      g_list_free (removed_printer_checklist);
    }

done:
  if (list_has_changed)
    g_signal_emit_by_name (backend, "printer-list-changed");

  gtk_print_backend_set_list_done (backend);

  if (!cups_backend->got_default_printer && remote_default_printer != NULL)
    {
      set_default_printer (cups_backend, remote_default_printer);
      g_free (remote_default_printer);
    }

#ifdef HAVE_CUPS_API_1_6
  if (!cups_backend->got_default_printer && cups_backend->avahi_default_printer != NULL)
    {
      set_default_printer (cups_backend, cups_backend->avahi_default_printer);
    }
#endif

  gdk_threads_leave ();
}

static void
update_backend_status (GtkPrintBackendCups    *cups_backend,
                       GtkCupsConnectionState  state)
{
  switch (state)
    {
    case GTK_CUPS_CONNECTION_NOT_AVAILABLE:
      g_object_set (cups_backend, "status", GTK_PRINT_BACKEND_STATUS_UNAVAILABLE, NULL);
      break;
    case GTK_CUPS_CONNECTION_AVAILABLE:
      g_object_set (cups_backend, "status", GTK_PRINT_BACKEND_STATUS_OK, NULL);
      break;
    default: ;
    }
}

static gboolean
cups_request_printer_list (GtkPrintBackendCups *cups_backend)
{
  GtkCupsConnectionState state;
  GtkCupsRequest *request;

  if (cups_backend->reading_ppds > 0 || cups_backend->list_printers_pending)
    return TRUE;

  state = gtk_cups_connection_test_get_state (cups_backend->cups_connection_test);
  update_backend_status (cups_backend, state);

  if (cups_backend->list_printers_attempts == 60)
    {
      cups_backend->list_printers_attempts = -1;
      if (cups_backend->list_printers_poll > 0)
        g_source_remove (cups_backend->list_printers_poll);
      cups_backend->list_printers_poll = gdk_threads_add_timeout (200,
                                           (GSourceFunc) cups_request_printer_list,
                                           cups_backend);
      g_source_set_name_by_id (cups_backend->list_printers_poll, "[gtk+] cups_request_printer_list");
    }
  else if (cups_backend->list_printers_attempts != -1)
    cups_backend->list_printers_attempts++;

  if (state == GTK_CUPS_CONNECTION_IN_PROGRESS || state == GTK_CUPS_CONNECTION_NOT_AVAILABLE)
    return TRUE;
  else
    if (cups_backend->list_printers_attempts > 0)
      cups_backend->list_printers_attempts = 60;

  cups_backend->list_printers_pending = TRUE;

  request = gtk_cups_request_new_with_username (NULL,
                                                GTK_CUPS_POST,
                                                CUPS_GET_PRINTERS,
                                                NULL,
                                                NULL,
                                                NULL,
                                                cups_backend->username);

  gtk_cups_request_ipp_add_strings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
				    "requested-attributes", G_N_ELEMENTS (printer_attrs),
				    NULL, printer_attrs);

  cups_request_execute (cups_backend,
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_request_printer_list_cb,
		        request,
		        NULL);

  return TRUE;
}

static void
cups_get_printer_list (GtkPrintBackend *backend)
{
  GtkPrintBackendCups *cups_backend;

  cups_backend = GTK_PRINT_BACKEND_CUPS (backend);

  if (cups_backend->cups_connection_test == NULL)
    cups_backend->cups_connection_test = gtk_cups_connection_test_new (NULL);

  if (cups_backend->list_printers_poll == 0)
    {
      if (cups_request_printer_list (cups_backend))
        {
          cups_backend->list_printers_poll = gdk_threads_add_timeout (50,
                                               (GSourceFunc) cups_request_printer_list,
                                               backend);
          g_source_set_name_by_id (cups_backend->list_printers_poll, "[gtk+] cups_request_printer_list");
        }

#ifdef HAVE_CUPS_API_1_6
      avahi_request_printer_list (cups_backend);
#endif
    }
}

typedef struct {
  GtkPrinterCups *printer;
  GIOChannel *ppd_io;
  http_t *http;
} GetPPDData;

static void
get_ppd_data_free (GetPPDData *data)
{
  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));
  httpClose (data->http);
  g_io_channel_unref (data->ppd_io);
  g_object_unref (data->printer);
  g_free (data);
}

static void
cups_request_ppd_cb (GtkPrintBackendCups *print_backend,
                     GtkCupsResult       *result,
                     GetPPDData          *data)
{
  GtkPrinter *printer;

  gdk_threads_enter ();

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  printer = GTK_PRINTER (data->printer);
  GTK_PRINTER_CUPS (printer)->reading_ppd = FALSE;
  print_backend->reading_ppds--;

  if (gtk_cups_result_is_error (result))
    {
      gboolean success = FALSE;

#ifdef HAVE_CUPS_API_1_6
      /* Standalone Avahi printers don't have PPD files. */
      if (GTK_PRINTER_CUPS (printer)->avahi_browsed)
        {
          cups_request_avahi_printer_info (GTK_PRINTER_CUPS (printer)->printer_uri,
                                           GTK_PRINTER_CUPS (printer)->hostname,
                                           GTK_PRINTER_CUPS (printer)->port,
                                           GTK_PRINT_BACKEND_CUPS (gtk_printer_get_backend (printer)));
        }
      else
#endif
        {
          /* If we get a 404 then it is just a raw printer without a ppd
             and not an error. */
          if ((gtk_cups_result_get_error_type (result) == GTK_CUPS_ERROR_HTTP) &&
              (gtk_cups_result_get_error_status (result) == HTTP_NOT_FOUND))
            {
              gtk_printer_set_has_details (printer, TRUE);
              success = TRUE;
            }

          g_signal_emit_by_name (printer, "details-acquired", success);
        }

      goto done;
    }

  /* let ppdOpenFd take over the ownership of the open file */
  g_io_channel_seek_position (data->ppd_io, 0, G_SEEK_SET, NULL);
  data->printer->ppd_file = ppdOpenFd (dup (g_io_channel_unix_get_fd (data->ppd_io)));
  ppdLocalize (data->printer->ppd_file);
  ppdMarkDefaults (data->printer->ppd_file);

  gtk_printer_set_has_details (printer, TRUE);
  g_signal_emit_by_name (printer, "details-acquired", TRUE);

done:
  gdk_threads_leave ();
}

static gboolean
cups_request_ppd (GtkPrinter *printer)
{
  GError *error;
  GtkPrintBackend *print_backend;
  GtkPrinterCups *cups_printer;
  GtkCupsRequest *request;
  char *ppd_filename = NULL;
  gchar *resource;
  http_t *http;
  GetPPDData *data;
  int fd;

  cups_printer = GTK_PRINTER_CUPS (printer);

  error = NULL;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  if (cups_printer->remote
#ifdef HAVE_CUPS_API_1_6
      && !cups_printer->avahi_browsed
#endif
      )
    {
      GtkCupsConnectionState state;

      state = gtk_cups_connection_test_get_state (cups_printer->remote_cups_connection_test);

      if (state == GTK_CUPS_CONNECTION_IN_PROGRESS)
        {
          if (cups_printer->get_remote_ppd_attempts == 60)
            {
              cups_printer->get_remote_ppd_attempts = -1;
              if (cups_printer->get_remote_ppd_poll > 0)
                g_source_remove (cups_printer->get_remote_ppd_poll);
              cups_printer->get_remote_ppd_poll = gdk_threads_add_timeout (200,
                                                    (GSourceFunc) cups_request_ppd,
                                                    printer);
              g_source_set_name_by_id (cups_printer->get_remote_ppd_poll, "[gtk+] cups_request_ppd");
            }
          else if (cups_printer->get_remote_ppd_attempts != -1)
            cups_printer->get_remote_ppd_attempts++;

          return TRUE;
        }

      gtk_cups_connection_test_free (cups_printer->remote_cups_connection_test);
      cups_printer->remote_cups_connection_test = NULL;
      cups_printer->get_remote_ppd_poll = 0;
      cups_printer->get_remote_ppd_attempts = 0;

      if (state == GTK_CUPS_CONNECTION_NOT_AVAILABLE)
        {
          g_signal_emit_by_name (printer, "details-acquired", FALSE);
          return FALSE;
        }
    }

  http = httpConnectEncrypt (cups_printer->hostname,
			     cups_printer->port,
			     cupsEncryption ());

  data = g_new0 (GetPPDData, 1);

  fd = g_file_open_tmp ("gtkprint_ppd_XXXXXX",
                        &ppd_filename,
                        &error);

#ifdef G_ENABLE_DEBUG
  /* If we are debugging printing don't delete the tmp files */
  if (!(gtk_get_debug_flags () & GTK_DEBUG_PRINTING))
    unlink (ppd_filename);
#else
  unlink (ppd_filename);
#endif /* G_ENABLE_DEBUG */

  if (error != NULL)
    {
      GTK_NOTE (PRINTING,
                g_warning ("CUPS Backend: Failed to create temp file, %s\n",
                           error->message));
      g_error_free (error);
      httpClose (http);
      g_free (ppd_filename);
      g_free (data);

      g_signal_emit_by_name (printer, "details-acquired", FALSE);
      return FALSE;
    }

  data->http = http;
  fchmod (fd, S_IRUSR | S_IWUSR);
  data->ppd_io = g_io_channel_unix_new (fd);
  g_io_channel_set_encoding (data->ppd_io, NULL, NULL);
  g_io_channel_set_close_on_unref (data->ppd_io, TRUE);

  data->printer = g_object_ref (printer);

  resource = g_strdup_printf ("/printers/%s.ppd",
                              gtk_printer_cups_get_ppd_name (GTK_PRINTER_CUPS (printer)));

  print_backend = gtk_printer_get_backend (printer);

  request = gtk_cups_request_new_with_username (data->http,
                                                GTK_CUPS_GET,
                                                0,
                                                data->ppd_io,
                                                cups_printer->hostname,
                                                resource,
                                                GTK_PRINT_BACKEND_CUPS (print_backend)->username);

  gtk_cups_request_set_ipp_version (request,
                                    cups_printer->ipp_version_major,
                                    cups_printer->ipp_version_minor);

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: Requesting resource %s to be written to temp file %s\n", resource, ppd_filename));


  cups_printer->reading_ppd = TRUE;
  GTK_PRINT_BACKEND_CUPS (print_backend)->reading_ppds++;

  cups_request_execute (GTK_PRINT_BACKEND_CUPS (print_backend),
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_request_ppd_cb,
                        data,
                        (GDestroyNotify)get_ppd_data_free);

  g_free (resource);
  g_free (ppd_filename);

  return FALSE;
}

/* Ordering matters for default preference */
static const char *lpoptions_locations[] = {
  "/etc/cups/lpoptions",
  ".lpoptions",
  ".cups/lpoptions"
};

static void
cups_parse_user_default_printer (const char  *filename,
                                 char       **printer_name)
{
  FILE *fp;
  char line[1024], *lineptr, *defname = NULL;

  if ((fp = g_fopen (filename, "r")) == NULL)
    return;

  while (fgets (line, sizeof (line), fp) != NULL)
    {
      if (strncasecmp (line, "default", 7) != 0 || !isspace (line[7]))
        continue;

      lineptr = line + 8;
      while (isspace (*lineptr))
        lineptr++;

      if (!*lineptr)
        continue;

      defname = lineptr;
      while (!isspace (*lineptr) && *lineptr && *lineptr != '/')
        lineptr++;

      *lineptr = '\0';

      if (*printer_name != NULL)
        g_free (*printer_name);

      *printer_name = g_strdup (defname);
    }

  fclose (fp);
}

static void
cups_get_user_default_printer (char **printer_name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (lpoptions_locations); i++)
    {
      if (g_path_is_absolute (lpoptions_locations[i]))
        {
          cups_parse_user_default_printer (lpoptions_locations[i],
                                           printer_name);
        }
      else
        {
          char *filename;

          filename = g_build_filename (g_get_home_dir (),
                                       lpoptions_locations[i], NULL);
          cups_parse_user_default_printer (filename, printer_name);
          g_free (filename);
        }
    }
}

static int
cups_parse_user_options (const char     *filename,
                         const char     *printer_name,
                         int             num_options,
                         cups_option_t **options)
{
  FILE *fp;
  gchar line[1024], *lineptr, *name;

  if ((fp = g_fopen (filename, "r")) == NULL)
    return num_options;

  while (fgets (line, sizeof (line), fp) != NULL)
    {
      if (strncasecmp (line, "dest", 4) == 0 && isspace (line[4]))
        lineptr = line + 4;
      else if (strncasecmp (line, "default", 7) == 0 && isspace (line[7]))
        lineptr = line + 7;
      else
        continue;

      /* Skip leading whitespace */
      while (isspace (*lineptr))
        lineptr++;

      if (!*lineptr)
        continue;

      /* NUL-terminate the name, stripping the instance name */
      name = lineptr;
      while (!isspace (*lineptr) && *lineptr)
        {
          if (*lineptr == '/')
            *lineptr = '\0';
          lineptr++;
        }

      if (!*lineptr)
        continue;

      *lineptr++ = '\0';

      if (strncasecmp (name, printer_name, strlen (printer_name)) != 0)
          continue;

      /* We found our printer, parse the options */
      num_options = cupsParseOptions (lineptr, num_options, options);
    }

  fclose (fp);

  return num_options;
}

static int
cups_get_user_options (const char     *printer_name,
                       int             num_options,
                       cups_option_t **options)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (lpoptions_locations); i++)
    {
      if (g_path_is_absolute (lpoptions_locations[i]))
        {
           num_options = cups_parse_user_options (lpoptions_locations[i],
                                                  printer_name,
                                                  num_options,
                                                  options);
        }
      else
        {
          char *filename;

          filename = g_build_filename (g_get_home_dir (),
                                       lpoptions_locations[i], NULL);
          num_options = cups_parse_user_options (filename, printer_name,
                                                 num_options, options);
          g_free (filename);
        }
    }

  return num_options;
}

/* This function requests default printer from a CUPS server in regular intervals.
 * In the case of unreachable CUPS server the request is repeated later.
 * The default printer is not requested in the case of previous success.
 */
static void
cups_get_default_printer (GtkPrintBackendCups *backend)
{
  GtkPrintBackendCups *cups_backend;

  cups_backend = backend;

  if (cups_backend->cups_connection_test == NULL)
    cups_backend->cups_connection_test = gtk_cups_connection_test_new (NULL);

  if (cups_backend->default_printer_poll == 0)
    {
      if (cups_request_default_printer (cups_backend))
        {
          cups_backend->default_printer_poll = gdk_threads_add_timeout (200,
                                                 (GSourceFunc) cups_request_default_printer,
                                                 backend);
          g_source_set_name_by_id (cups_backend->default_printer_poll, "[gtk+] cups_request_default_printer");
        }
    }
}

/* This function gets default printer from local settings.*/
static void
cups_get_local_default_printer (GtkPrintBackendCups *backend)
{
  const char *str;
  char *name = NULL;

  if ((str = g_getenv ("LPDEST")) != NULL)
    {
      backend->default_printer = g_strdup (str);
      backend->got_default_printer = TRUE;
      return;
    }
  else if ((str = g_getenv ("PRINTER")) != NULL &&
	   strcmp (str, "lp") != 0)
    {
      backend->default_printer = g_strdup (str);
      backend->got_default_printer = TRUE;
      return;
    }

  /* Figure out user setting for default printer */
  cups_get_user_default_printer (&name);
  if (name != NULL)
    {
      backend->default_printer = name;
      backend->got_default_printer = TRUE;
      return;
    }
}

static void
cups_request_default_printer_cb (GtkPrintBackendCups *print_backend,
				 GtkCupsResult       *result,
				 gpointer             user_data)
{
  ipp_t *response;
  ipp_attribute_t *attr;
  GtkPrinter *printer;

  gdk_threads_enter ();

  if (gtk_cups_result_is_error (result))
    {
      if (gtk_cups_result_get_error_type (result) == GTK_CUPS_ERROR_AUTH &&
          gtk_cups_result_get_error_code (result) == 1)
        {
          /* Canceled by user, stop popping up more password dialogs */
          if (print_backend->list_printers_poll > 0)
            g_source_remove (print_backend->list_printers_poll);
          print_backend->list_printers_poll = 0;
        }

      return;
    }

  response = gtk_cups_result_get_response (result);

  if ((attr = ippFindAttribute (response, "printer-name", IPP_TAG_NAME)) != NULL)
      print_backend->default_printer = g_strdup (ippGetString (attr, 0, NULL));

  print_backend->got_default_printer = TRUE;

  if (print_backend->default_printer != NULL)
    {
      printer = gtk_print_backend_find_printer (GTK_PRINT_BACKEND (print_backend), print_backend->default_printer);
      if (printer != NULL)
        {
          gtk_printer_set_is_default (printer, TRUE);
          g_signal_emit_by_name (GTK_PRINT_BACKEND (print_backend), "printer-status-changed", printer);
        }
    }

  /* Make sure to kick off get_printers if we are polling it,
   * as we could have blocked this reading the default printer
   */
  if (print_backend->list_printers_poll != 0)
    cups_request_printer_list (print_backend);

  gdk_threads_leave ();
}

static gboolean
cups_request_default_printer (GtkPrintBackendCups *print_backend)
{
  GtkCupsConnectionState state;
  GtkCupsRequest *request;

  state = gtk_cups_connection_test_get_state (print_backend->cups_connection_test);
  update_backend_status (print_backend, state);

  if (state == GTK_CUPS_CONNECTION_IN_PROGRESS || state == GTK_CUPS_CONNECTION_NOT_AVAILABLE)
    return TRUE;

  request = gtk_cups_request_new_with_username (NULL,
                                                GTK_CUPS_POST,
                                                CUPS_GET_DEFAULT,
                                                NULL,
                                                NULL,
                                                NULL,
                                                print_backend->username);

  cups_request_execute (print_backend,
                        request,
                        (GtkPrintCupsResponseCallbackFunc) cups_request_default_printer_cb,
		        g_object_ref (print_backend),
		        g_object_unref);

  return FALSE;
}

static void
cups_printer_request_details (GtkPrinter *printer)
{
  GtkPrinterCups *cups_printer;

  cups_printer = GTK_PRINTER_CUPS (printer);
  if (!cups_printer->reading_ppd &&
      gtk_printer_cups_get_ppd (cups_printer) == NULL)
    {
      if (cups_printer->remote
#ifdef HAVE_CUPS_API_1_6
          && !cups_printer->avahi_browsed
#endif
          )
        {
          if (cups_printer->get_remote_ppd_poll == 0)
            {
              cups_printer->remote_cups_connection_test = gtk_cups_connection_test_new (cups_printer->hostname);

              if (cups_request_ppd (printer))
                {
                  cups_printer->get_remote_ppd_poll = gdk_threads_add_timeout (50,
                                                      (GSourceFunc) cups_request_ppd,
                                                      printer);
                  g_source_set_name_by_id (cups_printer->get_remote_ppd_poll, "[gtk+] cups_request_ppd");
                }
            }
        }
      else
        cups_request_ppd (printer);
    }
}

static char *
ppd_text_to_utf8 (ppd_file_t *ppd_file,
		  const char *text)
{
  const char *encoding = NULL;
  char *res;

  if (g_ascii_strcasecmp (ppd_file->lang_encoding, "UTF-8") == 0)
    {
      return g_strdup (text);
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "ISOLatin1") == 0)
    {
      encoding = "ISO-8859-1";
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "ISOLatin2") == 0)
    {
      encoding = "ISO-8859-2";
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "ISOLatin5") == 0)
    {
      encoding = "ISO-8859-5";
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "JIS83-RKSJ") == 0)
    {
      encoding = "SHIFT-JIS";
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "MacStandard") == 0)
    {
      encoding = "MACINTOSH";
    }
  else if (g_ascii_strcasecmp (ppd_file->lang_encoding, "WindowsANSI") == 0)
    {
      encoding = "WINDOWS-1252";
    }
  else
    {
      /* Fallback, try iso-8859-1... */
      encoding = "ISO-8859-1";
    }

  res = g_convert (text, -1, "UTF-8", encoding, NULL, NULL, NULL);

  if (res == NULL)
    {
      GTK_NOTE (PRINTING,
                g_warning ("CUPS Backend: Unable to convert PPD text\n"));
      res = g_strdup ("???");
    }

  return res;
}

/* TODO: Add more translations for common settings here */

static const struct {
  const char *keyword;
  const char *translation;
} cups_option_translations[] = {
  { "Duplex", N_("Two Sided") },
  { "MediaType", N_("Paper Type") },
  { "InputSlot", N_("Paper Source") },
  { "OutputBin", N_("Output Tray") },
  { "Resolution", N_("Resolution") },
  { "PreFilter", N_("GhostScript pre-filtering") },
};


static const struct {
  const char *keyword;
  const char *choice;
  const char *translation;
} cups_choice_translations[] = {
  { "Duplex", "None", N_("One Sided") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexNoTumble", N_("Long Edge (Standard)") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexTumble", N_("Short Edge (Flip)") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Auto", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "AutoSelect", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Default", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "None", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "PrinterDefault", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Unspecified", N_("Auto Select") },
  /* Translators: this is an option of "Resolution" */
  { "Resolution", "default", N_("Printer Default") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "EmbedFonts", N_("Embed GhostScript fonts only") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level1", N_("Convert to PS level 1") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level2", N_("Convert to PS level 2") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "No", N_("No pre-filtering") },
};

static const struct {
  const char *name;
  const char *translation;
} cups_group_translations[] = {
/* Translators: "Miscellaneous" is the label for a button, that opens
   up an extra panel of settings in a print dialog. */
  { "Miscellaneous", N_("Miscellaneous") },
};

static const struct {
  const char *ppd_keyword;
  const char *name;
} ppd_option_names[] = {
  {"Duplex", "gtk-duplex" },
  {"MediaType", "gtk-paper-type"},
  {"InputSlot", "gtk-paper-source"},
  {"OutputBin", "gtk-output-tray"},
};

static const struct {
  const char *lpoption;
  const char *name;
} lpoption_names[] = {
  {"number-up", "gtk-n-up" },
  {"number-up-layout", "gtk-n-up-layout"},
  {"job-billing", "gtk-billing-info"},
  {"job-priority", "gtk-job-prio"},
};

/* keep sorted when changing */
static const char *color_option_whitelist[] = {
  "BRColorEnhancement",
  "BRColorMatching",
  "BRColorMatching",
  "BRColorMode",
  "BRGammaValue",
  "BRImprovedGray",
  "BlackSubstitution",
  "ColorModel",
  "HPCMYKInks",
  "HPCSGraphics",
  "HPCSImages",
  "HPCSText",
  "HPColorSmart",
  "RPSBlackMode",
  "RPSBlackOverPrint",
  "Rcmyksimulation",
};

/* keep sorted when changing */
static const char *color_group_whitelist[] = {
  "ColorPage",
  "FPColorWise1",
  "FPColorWise2",
  "FPColorWise3",
  "FPColorWise4",
  "FPColorWise5",
  "HPColorOptionsPanel",
};

/* keep sorted when changing */
static const char *image_quality_option_whitelist[] = {
  "BRDocument",
  "BRHalfTonePattern",
  "BRNormalPrt",
  "BRPrintQuality",
  "BitsPerPixel",
  "Darkness",
  "Dithering",
  "EconoMode",
  "Economode",
  "HPEconoMode",
  "HPEdgeControl",
  "HPGraphicsHalftone",
  "HPHalftone",
  "HPLJDensity",
  "HPPhotoHalftone",
  "OutputMode",
  "REt",
  "RPSBitsPerPixel",
  "RPSDitherType",
  "Resolution",
  "ScreenLock",
  "Smoothing",
  "TonerSaveMode",
  "UCRGCRForImage",
};

/* keep sorted when changing */
static const char *image_quality_group_whitelist[] = {
  "FPImageQuality1",
  "FPImageQuality2",
  "FPImageQuality3",
  "ImageQualityPage",
};

/* keep sorted when changing */
static const char * finishing_option_whitelist[] = {
  "BindColor",
  "BindEdge",
  "BindType",
  "BindWhen",
  "Booklet",
  "FoldType",
  "FoldWhen",
  "HPStaplerOptions",
  "Jog",
  "Slipsheet",
  "Sorter",
  "StapleLocation",
  "StapleOrientation",
  "StapleWhen",
  "StapleX",
  "StapleY",
};

/* keep sorted when changing */
static const char *finishing_group_whitelist[] = {
  "FPFinishing1",
  "FPFinishing2",
  "FPFinishing3",
  "FPFinishing4",
  "FinishingPage",
  "HPFinishingPanel",
};

/* keep sorted when changing */
static const char *cups_option_blacklist[] = {
  "Collate",
  "Copies",
  "OutputOrder",
  "PageRegion",
  "PageSize",
};

static char *
get_option_text (ppd_file_t   *ppd_file,
		 ppd_option_t *option)
{
  int i;
  char *utf8;

  for (i = 0; i < G_N_ELEMENTS (cups_option_translations); i++)
    {
      if (strcmp (cups_option_translations[i].keyword, option->keyword) == 0)
	return g_strdup (_(cups_option_translations[i].translation));
    }

  utf8 = ppd_text_to_utf8 (ppd_file, option->text);

  /* Some ppd files have spaces in the text before the colon */
  g_strchomp (utf8);

  return utf8;
}

static char *
get_choice_text (ppd_file_t   *ppd_file,
		 ppd_choice_t *choice)
{
  int i;
  ppd_option_t *option = choice->option;
  const char *keyword = option->keyword;

  for (i = 0; i < G_N_ELEMENTS (cups_choice_translations); i++)
    {
      if (strcmp (cups_choice_translations[i].keyword, keyword) == 0 &&
	  strcmp (cups_choice_translations[i].choice, choice->choice) == 0)
	return g_strdup (_(cups_choice_translations[i].translation));
    }
  return ppd_text_to_utf8 (ppd_file, choice->text);
}

static gboolean
group_has_option (ppd_group_t  *group,
		  ppd_option_t *option)
{
  int i;

  if (group == NULL)
    return FALSE;

  if (group->num_options > 0 &&
      option >= group->options && option < group->options + group->num_options)
    return TRUE;

  for (i = 0; i < group->num_subgroups; i++)
    {
      if (group_has_option (&group->subgroups[i],option))
	return TRUE;
    }
  return FALSE;
}

static void
set_option_off (GtkPrinterOption *option)
{
  /* Any of these will do, _set only applies the value
   * if its allowed of the option */
  gtk_printer_option_set (option, "False");
  gtk_printer_option_set (option, "Off");
  gtk_printer_option_set (option, "None");
}

static gboolean
value_is_off (const char *value)
{
  return  (strcasecmp (value, "None") == 0 ||
	   strcasecmp (value, "Off") == 0 ||
	   strcasecmp (value, "False") == 0);
}

static char *
ppd_group_name (ppd_group_t *group)
{
  return group->name;
}

static int
available_choices (ppd_file_t     *ppd,
		   ppd_option_t   *option,
		   ppd_choice_t ***available,
		   gboolean        keep_if_only_one_option)
{
  ppd_option_t *other_option;
  int i, j;
  gchar *conflicts;
  ppd_const_t *constraint;
  const char *choice, *other_choice;
  ppd_option_t *option1, *option2;
  ppd_group_t *installed_options;
  int num_conflicts;
  gboolean all_default;
  int add_auto;

  if (available)
    *available = NULL;

  conflicts = g_new0 (char, option->num_choices);

  installed_options = NULL;
  for (i = 0; i < ppd->num_groups; i++)
    {
      char *name;

      name = ppd_group_name (&ppd->groups[i]);
      if (strcmp (name, "InstallableOptions") == 0)
	{
	  installed_options = &ppd->groups[i];
	  break;
	}
    }

  for (i = ppd->num_consts, constraint = ppd->consts; i > 0; i--, constraint++)
    {
      option1 = ppdFindOption (ppd, constraint->option1);
      if (option1 == NULL)
	continue;

      option2 = ppdFindOption (ppd, constraint->option2);
      if (option2 == NULL)
	continue;

      if (option == option1)
	{
	  choice = constraint->choice1;
	  other_option = option2;
	  other_choice = constraint->choice2;
	}
      else if (option == option2)
	{
	  choice = constraint->choice2;
	  other_option = option1;
	  other_choice = constraint->choice1;
	}
      else
	continue;

      /* We only care of conflicts with installed_options and PageSize */
      if (!group_has_option (installed_options, other_option) &&
	  (strcmp (other_option->keyword, "PageSize") != 0))
	continue;

      if (*other_choice == 0)
	{
	  /* Conflict only if the installed option is not off */
	  if (value_is_off (other_option->defchoice))
	    continue;
	}
      /* Conflict if the installed option has the specified default */
      else if (strcasecmp (other_choice, other_option->defchoice) != 0)
	continue;

      if (*choice == 0)
	{
	  /* Conflict with all non-off choices */
	  for (j = 0; j < option->num_choices; j++)
	    {
	      if (!value_is_off (option->choices[j].choice))
		conflicts[j] = 1;
	    }
	}
      else
	{
	  for (j = 0; j < option->num_choices; j++)
	    {
	      if (strcasecmp (option->choices[j].choice, choice) == 0)
		conflicts[j] = 1;
	    }
	}
    }

  num_conflicts = 0;
  all_default = TRUE;
  for (j = 0; j < option->num_choices; j++)
    {
      if (conflicts[j])
	num_conflicts++;
      else if (strcmp (option->choices[j].choice, option->defchoice) != 0)
	all_default = FALSE;
    }

  if ((all_default && !keep_if_only_one_option) ||
      (num_conflicts == option->num_choices))
    {
      g_free (conflicts);

      return 0;
    }

  /* Some ppds don't have a "use printer default" option for
   * InputSlot. This means you always have to select a particular slot,
   * and you can't auto-pick source based on the paper size. To support
   * this we always add an auto option if there isn't one already. If
   * the user chooses the generated option we don't send any InputSlot
   * value when printing. The way we detect existing auto-cases is based
   * on feedback from Michael Sweet of cups fame.
   */
  add_auto = 0;
  if (strcmp (option->keyword, "InputSlot") == 0)
    {
      gboolean found_auto = FALSE;
      for (j = 0; j < option->num_choices; j++)
	{
	  if (!conflicts[j])
	    {
	      if (strcmp (option->choices[j].choice, "Auto") == 0 ||
		  strcmp (option->choices[j].choice, "AutoSelect") == 0 ||
		  strcmp (option->choices[j].choice, "Default") == 0 ||
		  strcmp (option->choices[j].choice, "None") == 0 ||
		  strcmp (option->choices[j].choice, "PrinterDefault") == 0 ||
		  strcmp (option->choices[j].choice, "Unspecified") == 0 ||
		  option->choices[j].code == NULL ||
		  option->choices[j].code[0] == 0)
		{
		  found_auto = TRUE;
		  break;
		}
	    }
	}

      if (!found_auto)
	add_auto = 1;
    }

  if (available)
    {
      *available = g_new (ppd_choice_t *, option->num_choices - num_conflicts + add_auto);

      i = 0;
      for (j = 0; j < option->num_choices; j++)
	{
	  if (!conflicts[j])
	    (*available)[i++] = &option->choices[j];
	}

      if (add_auto)
	(*available)[i++] = NULL;
    }

  g_free (conflicts);

  return option->num_choices - num_conflicts + add_auto;
}

static GtkPrinterOption *
create_pickone_option (ppd_file_t   *ppd_file,
		       ppd_option_t *ppd_option,
		       const gchar  *gtk_name)
{
  GtkPrinterOption *option;
  ppd_choice_t **available;
  char *label;
  int n_choices;
  int i;
  ppd_coption_t *coption;

  g_assert (ppd_option->ui == PPD_UI_PICKONE);

  option = NULL;

  n_choices = available_choices (ppd_file, ppd_option, &available, g_str_has_prefix (gtk_name, "gtk-"));
  if (n_choices > 0)
    {

      /* right now only support one parameter per custom option
       * if more than one print warning and only offer the default choices
       */

      label = get_option_text (ppd_file, ppd_option);

      coption = ppdFindCustomOption (ppd_file, ppd_option->keyword);

      if (coption)
        {
	  ppd_cparam_t *cparam;

          cparam = ppdFirstCustomParam (coption);

          if (ppdNextCustomParam (coption) == NULL)
	    {
              switch (cparam->type)
	        {
                case PPD_CUSTOM_INT:
		  option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE_INT);
		  break;
                case PPD_CUSTOM_PASSCODE:
		  option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE_PASSCODE);
		  break;
                case PPD_CUSTOM_PASSWORD:
		    option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE_PASSWORD);
		  break;
               case PPD_CUSTOM_REAL:
		    option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE_REAL);
		  break;
                case PPD_CUSTOM_STRING:
		  option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE_STRING);
		  break;
#ifdef PRINT_IGNORED_OPTIONS
                case PPD_CUSTOM_POINTS:
		  g_warning ("CUPS Backend: PPD Custom Points Option not supported");
		  break;
                case PPD_CUSTOM_CURVE:
                  g_warning ("CUPS Backend: PPD Custom Curve Option not supported");
		  break;
                case PPD_CUSTOM_INVCURVE:
		  g_warning ("CUPS Backend: PPD Custom Inverse Curve Option not supported");
		  break;
#endif
                default:
                  break;
		}
	    }
#ifdef PRINT_IGNORED_OPTIONS
	  else
	    g_warning ("CUPS Backend: Multi-parameter PPD Custom Option not supported");
#endif
	}

      if (!option)
        option = gtk_printer_option_new (gtk_name, label,
				         GTK_PRINTER_OPTION_TYPE_PICKONE);
      g_free (label);

      gtk_printer_option_allocate_choices (option, n_choices);
      for (i = 0; i < n_choices; i++)
	{
	  if (available[i] == NULL)
	    {
	      /* This was auto-added */
	      option->choices[i] = g_strdup ("gtk-ignore-value");
	      option->choices_display[i] = g_strdup (_("Printer Default"));
	    }
	  else
	    {
	      option->choices[i] = g_strdup (available[i]->choice);
	      option->choices_display[i] = get_choice_text (ppd_file, available[i]);
	    }
	}

      if (option->type != GTK_PRINTER_OPTION_TYPE_PICKONE)
        {
          if (g_str_has_prefix (ppd_option->defchoice, "Custom."))
            gtk_printer_option_set (option, ppd_option->defchoice + 7);
          else
            gtk_printer_option_set (option, ppd_option->defchoice);
        }
      else
        {
          gtk_printer_option_set (option, ppd_option->defchoice);
        }
    }
#ifdef PRINT_IGNORED_OPTIONS
  else
    g_warning ("CUPS Backend: Ignoring pickone %s\n", ppd_option->text);
#endif
  g_free (available);

  return option;
}

static GtkPrinterOption *
create_boolean_option (ppd_file_t   *ppd_file,
		       ppd_option_t *ppd_option,
		       const gchar  *gtk_name)
{
  GtkPrinterOption *option;
  ppd_choice_t **available;
  char *label;
  int n_choices;

  g_assert (ppd_option->ui == PPD_UI_BOOLEAN);

  option = NULL;

  n_choices = available_choices (ppd_file, ppd_option, &available, g_str_has_prefix (gtk_name, "gtk-"));
  if (n_choices == 2)
    {
      label = get_option_text (ppd_file, ppd_option);
      option = gtk_printer_option_new (gtk_name, label,
				       GTK_PRINTER_OPTION_TYPE_BOOLEAN);
      g_free (label);

      gtk_printer_option_allocate_choices (option, 2);
      option->choices[0] = g_strdup ("True");
      option->choices_display[0] = g_strdup ("True");
      option->choices[1] = g_strdup ("False");
      option->choices_display[1] = g_strdup ("False");

      gtk_printer_option_set (option, ppd_option->defchoice);
    }
#ifdef PRINT_IGNORED_OPTIONS
  else
    g_warning ("CUPS Backend: Ignoring boolean %s\n", ppd_option->text);
#endif
  g_free (available);

  return option;
}

static gchar *
get_ppd_option_name (const gchar *keyword)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (ppd_option_names); i++)
    if (strcmp (ppd_option_names[i].ppd_keyword, keyword) == 0)
      return g_strdup (ppd_option_names[i].name);

  return g_strdup_printf ("cups-%s", keyword);
}

static gchar *
get_lpoption_name (const gchar *lpoption)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (ppd_option_names); i++)
    if (strcmp (ppd_option_names[i].ppd_keyword, lpoption) == 0)
      return g_strdup (ppd_option_names[i].name);

  for (i = 0; i < G_N_ELEMENTS (lpoption_names); i++)
    if (strcmp (lpoption_names[i].lpoption, lpoption) == 0)
      return g_strdup (lpoption_names[i].name);

  return g_strdup_printf ("cups-%s", lpoption);
}

static int
strptr_cmp (const void *a,
	    const void *b)
{
  char **aa = (char **)a;
  char **bb = (char **)b;
  return strcmp (*aa, *bb);
}


static gboolean
string_in_table (gchar       *str,
		 const gchar *table[],
		 gint         table_len)
{
  return bsearch (&str, table, table_len, sizeof (char *), (void *)strptr_cmp) != NULL;
}

#define STRING_IN_TABLE(_str, _table) (string_in_table (_str, _table, G_N_ELEMENTS (_table)))

static void
handle_option (GtkPrinterOptionSet *set,
	       ppd_file_t          *ppd_file,
	       ppd_option_t        *ppd_option,
	       ppd_group_t         *toplevel_group,
	       GtkPrintSettings    *settings)
{
  GtkPrinterOption *option;
  char *name;
  int i;

  if (STRING_IN_TABLE (ppd_option->keyword, cups_option_blacklist))
    return;

  name = get_ppd_option_name (ppd_option->keyword);

  option = NULL;
  if (ppd_option->ui == PPD_UI_PICKONE)
    {
      option = create_pickone_option (ppd_file, ppd_option, name);
    }
  else if (ppd_option->ui == PPD_UI_BOOLEAN)
    {
      option = create_boolean_option (ppd_file, ppd_option, name);
    }
#ifdef PRINT_IGNORED_OPTIONS
  else
    g_warning ("CUPS Backend: Ignoring pickmany setting %s\n", ppd_option->text);
#endif

  if (option)
    {
      char *name;

      name = ppd_group_name (toplevel_group);
      if (STRING_IN_TABLE (name,
			   color_group_whitelist) ||
	  STRING_IN_TABLE (ppd_option->keyword,
			   color_option_whitelist))
	{
	  option->group = g_strdup ("ColorPage");
	}
      else if (STRING_IN_TABLE (name,
				image_quality_group_whitelist) ||
	       STRING_IN_TABLE (ppd_option->keyword,
				image_quality_option_whitelist))
	{
	  option->group = g_strdup ("ImageQualityPage");
	}
      else if (STRING_IN_TABLE (name,
				finishing_group_whitelist) ||
	       STRING_IN_TABLE (ppd_option->keyword,
				finishing_option_whitelist))
	{
	  option->group = g_strdup ("FinishingPage");
	}
      else
	{
	  for (i = 0; i < G_N_ELEMENTS (cups_group_translations); i++)
	    {
	      if (strcmp (cups_group_translations[i].name, toplevel_group->name) == 0)
		{
		  option->group = g_strdup (_(cups_group_translations[i].translation));
		  break;
		}
	    }

	  if (i == G_N_ELEMENTS (cups_group_translations))
	    option->group = g_strdup (toplevel_group->text);
	}

      set_option_from_settings (option, settings);

      gtk_printer_option_set_add (set, option);
    }

  g_free (name);
}

static void
handle_group (GtkPrinterOptionSet *set,
	      ppd_file_t          *ppd_file,
	      ppd_group_t         *group,
	      ppd_group_t         *toplevel_group,
	      GtkPrintSettings    *settings)
{
  gint i;
  gchar *name;

  /* Ignore installable options */
  name = ppd_group_name (toplevel_group);
  if (strcmp (name, "InstallableOptions") == 0)
    return;

  for (i = 0; i < group->num_options; i++)
    handle_option (set, ppd_file, &group->options[i], toplevel_group, settings);

  for (i = 0; i < group->num_subgroups; i++)
    handle_group (set, ppd_file, &group->subgroups[i], toplevel_group, settings);

}

#ifdef HAVE_COLORD

typedef struct {
        GtkPrintSettings     *settings;
        GtkPrinter           *printer;
} GtkPrintBackendCupsColordHelper;

static void
colord_printer_option_set_changed_cb (GtkPrinterOptionSet *set,
                                      GtkPrintBackendCupsColordHelper *helper)
{
  gtk_printer_cups_update_settings (GTK_PRINTER_CUPS (helper->printer),
                                    helper->settings,
                                    set);
}
#endif

static GtkPrinterOptionSet *
cups_printer_get_options (GtkPrinter           *printer,
			  GtkPrintSettings     *settings,
			  GtkPageSetup         *page_setup,
			  GtkPrintCapabilities  capabilities)
{
  GtkPrinterOptionSet *set;
  GtkPrinterOption *option;
  ppd_file_t *ppd_file;
  int i;
  char *print_at[] = { "now", "at", "on-hold" };
  char *n_up[] = {"1", "2", "4", "6", "9", "16" };
  char *prio[] = {"100", "80", "50", "30" };
  /* Translators: These strings name the possible values of the
   * job priority option in the print dialog
   */
  char *prio_display[] = {N_("Urgent"), N_("High"), N_("Medium"), N_("Low") };
  char *n_up_layout[] = { "lrtb", "lrbt", "rltb", "rlbt", "tblr", "tbrl", "btlr", "btrl" };
  /* Translators: These strings name the possible arrangements of
   * multiple pages on a sheet when printing
   */
  char *n_up_layout_display[] = { N_("Left to right, top to bottom"), N_("Left to right, bottom to top"),
                                  N_("Right to left, top to bottom"), N_("Right to left, bottom to top"),
                                  N_("Top to bottom, left to right"), N_("Top to bottom, right to left"),
                                  N_("Bottom to top, left to right"), N_("Bottom to top, right to left") };
  char *name;
  int num_opts;
  cups_option_t *opts = NULL;
  GtkPrintBackendCups *backend;
  GtkTextDirection text_direction;
  GtkPrinterCups *cups_printer = NULL;
#ifdef HAVE_COLORD
  GtkPrintBackendCupsColordHelper *helper;
#endif
  char *default_number_up;

  set = gtk_printer_option_set_new ();

  /* Cups specific, non-ppd related settings */

  for (i = 0; i < G_N_ELEMENTS(prio_display); i++)
    prio_display[i] = _(prio_display[i]);

  /* Translators, this string is used to label the job priority option
   * in the print dialog
   */
  option = gtk_printer_option_new ("gtk-job-prio", _("Job Priority"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (prio),
					 prio, prio_display);
  gtk_printer_option_set (option, "50");
  set_option_from_settings (option, settings);
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  /* Translators, this string is used to label the billing info entry
   * in the print dialog
   */
  option = gtk_printer_option_new ("gtk-billing-info", _("Billing Info"), GTK_PRINTER_OPTION_TYPE_STRING);
  gtk_printer_option_set (option, "");
  set_option_from_settings (option, settings);
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  backend = GTK_PRINT_BACKEND_CUPS (gtk_printer_get_backend (printer));
  cups_printer = GTK_PRINTER_CUPS (printer);

  if (backend != NULL && printer != NULL)
    {
      char *cover_default[] = {"none", "classified", "confidential", "secret", "standard", "topsecret", "unclassified" };
      /* Translators, these strings are names for various 'standard' cover
       * pages that the printing system may support.
       */
      char *cover_display_default[] = {N_("None"), N_("Classified"), N_("Confidential"), N_("Secret"), N_("Standard"), N_("Top Secret"), N_("Unclassified"),};
      char **cover = NULL;
      char **cover_display = NULL;
      char **cover_display_translated = NULL;
      gint num_of_covers = 0;
      gpointer value;
      gint j;

       /* Translators, this string is used to label the pages-per-sheet option
        * in the print dialog
        */
      option = gtk_printer_option_new ("gtk-n-up", _("Pages per Sheet"), GTK_PRINTER_OPTION_TYPE_PICKONE);
      gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up),
					     n_up, n_up);
      default_number_up = g_strdup_printf ("%d", cups_printer->default_number_up);
      gtk_printer_option_set (option, default_number_up);
      g_free (default_number_up);
      set_option_from_settings (option, settings);
      gtk_printer_option_set_add (set, option);
      g_object_unref (option);

      if (cups_printer_get_capabilities (printer) & GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT)
        {
          for (i = 0; i < G_N_ELEMENTS (n_up_layout_display); i++)
            n_up_layout_display[i] = _(n_up_layout_display[i]);

           /* Translators, this string is used to label the option in the print
            * dialog that controls in what order multiple pages are arranged
            */
          option = gtk_printer_option_new ("gtk-n-up-layout", _("Page Ordering"), GTK_PRINTER_OPTION_TYPE_PICKONE);
          gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (n_up_layout),
                                                 n_up_layout, n_up_layout_display);

          text_direction = gtk_widget_get_default_direction ();
          if (text_direction == GTK_TEXT_DIR_LTR)
            gtk_printer_option_set (option, "lrtb");
          else
            gtk_printer_option_set (option, "rltb");

          set_option_from_settings (option, settings);
          gtk_printer_option_set_add (set, option);
          g_object_unref (option);
        }

      num_of_covers = backend->number_of_covers;
      cover = g_new (char *, num_of_covers + 1);
      cover[num_of_covers] = NULL;
      cover_display = g_new (char *, num_of_covers + 1);
      cover_display[num_of_covers] = NULL;
      cover_display_translated = g_new (char *, num_of_covers + 1);
      cover_display_translated[num_of_covers] = NULL;

      for (i = 0; i < num_of_covers; i++)
        {
          cover[i] = g_strdup (backend->covers[i]);
          value = NULL;
          for (j = 0; j < G_N_ELEMENTS (cover_default); j++)
            if (strcmp (cover_default[j], cover[i]) == 0)
              {
                value = cover_display_default[j];
                break;
              }
          cover_display[i] = (value != NULL) ? g_strdup (value) : g_strdup (backend->covers[i]);
        }

      for (i = 0; i < num_of_covers; i++)
        cover_display_translated[i] = _(cover_display[i]);

      /* Translators, this is the label used for the option in the print
       * dialog that controls the front cover page.
       */
      option = gtk_printer_option_new ("gtk-cover-before", _("Before"), GTK_PRINTER_OPTION_TYPE_PICKONE);
      gtk_printer_option_choices_from_array (option, num_of_covers,
					 cover, cover_display_translated);

      if (cups_printer->default_cover_before != NULL)
        gtk_printer_option_set (option, cups_printer->default_cover_before);
      else
        gtk_printer_option_set (option, "none");
      set_option_from_settings (option, settings);
      gtk_printer_option_set_add (set, option);
      g_object_unref (option);

      /* Translators, this is the label used for the option in the print
       * dialog that controls the back cover page.
       */
      option = gtk_printer_option_new ("gtk-cover-after", _("After"), GTK_PRINTER_OPTION_TYPE_PICKONE);
      gtk_printer_option_choices_from_array (option, num_of_covers,
					 cover, cover_display_translated);
      if (cups_printer->default_cover_after != NULL)
        gtk_printer_option_set (option, cups_printer->default_cover_after);
      else
        gtk_printer_option_set (option, "none");
      set_option_from_settings (option, settings);
      gtk_printer_option_set_add (set, option);
      g_object_unref (option);

      g_strfreev (cover);
      g_strfreev (cover_display);
      g_free (cover_display_translated);
    }

  /* Translators: this is the name of the option that controls when
   * a print job is printed. Possible values are 'now', a specified time,
   * or 'on hold'
   */
  option = gtk_printer_option_new ("gtk-print-time", _("Print at"), GTK_PRINTER_OPTION_TYPE_PICKONE);
  gtk_printer_option_choices_from_array (option, G_N_ELEMENTS (print_at),
					 print_at, print_at);
  gtk_printer_option_set (option, "now");
  set_option_from_settings (option, settings);
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  /* Translators: this is the name of the option that allows the user
   * to specify a time when a print job will be printed.
   */
  option = gtk_printer_option_new ("gtk-print-time-text", _("Print at time"), GTK_PRINTER_OPTION_TYPE_STRING);
  gtk_printer_option_set (option, "");
  set_option_from_settings (option, settings);
  gtk_printer_option_set_add (set, option);
  g_object_unref (option);

  /* Printer (ppd) specific settings */
  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));
  if (ppd_file)
    {
      GtkPaperSize *paper_size;
      ppd_option_t *option;
      const gchar  *ppd_name;

      ppdMarkDefaults (ppd_file);

      paper_size = gtk_page_setup_get_paper_size (page_setup);

      option = ppdFindOption (ppd_file, "PageSize");
      if (option)
	{
	  ppd_name = gtk_paper_size_get_ppd_name (paper_size);

	  if (ppd_name)
	    strncpy (option->defchoice, ppd_name, PPD_MAX_NAME);
	  else
	    {
	      gchar *custom_name;
	      char width[G_ASCII_DTOSTR_BUF_SIZE];
	      char height[G_ASCII_DTOSTR_BUF_SIZE];

	      g_ascii_formatd (width, sizeof (width), "%.2f",
			       gtk_paper_size_get_width (paper_size,
							 GTK_UNIT_POINTS));
	      g_ascii_formatd (height, sizeof (height), "%.2f",
			       gtk_paper_size_get_height (paper_size,
							  GTK_UNIT_POINTS));
	      /* Translators: this format is used to display a custom
	       * paper size. The two placeholders are replaced with
	       * the width and height in points. E.g: "Custom
	       * 230.4x142.9"
               */
	      custom_name = g_strdup_printf (_("Custom %s×%s"), width, height);
	      strncpy (option->defchoice, custom_name, PPD_MAX_NAME);
	      g_free (custom_name);
	    }
	}

      for (i = 0; i < ppd_file->num_groups; i++)
        handle_group (set, ppd_file, &ppd_file->groups[i], &ppd_file->groups[i], settings);
    }

  /* Now honor the user set defaults for this printer */
  num_opts = cups_get_user_options (gtk_printer_get_name (printer), 0, &opts);

  for (i = 0; i < num_opts; i++)
    {
      if (STRING_IN_TABLE (opts[i].name, cups_option_blacklist))
        continue;

      name = get_lpoption_name (opts[i].name);
      if (strcmp (name, "cups-job-sheets") == 0)
        {
          gchar **values;
          gint    num_values;

          values = g_strsplit (opts[i].value, ",", 2);
          num_values = g_strv_length (values);

          option = gtk_printer_option_set_lookup (set, "gtk-cover-before");
          if (option && num_values > 0)
            gtk_printer_option_set (option, g_strstrip (values[0]));

          option = gtk_printer_option_set_lookup (set, "gtk-cover-after");
          if (option && num_values > 1)
            gtk_printer_option_set (option, g_strstrip (values[1]));

          g_strfreev (values);
        }
      else if (strcmp (name, "cups-job-hold-until") == 0)
        {
          GtkPrinterOption *option2 = NULL;

          option = gtk_printer_option_set_lookup (set, "gtk-print-time-text");
          if (option && opts[i].value)
            {
              option2 = gtk_printer_option_set_lookup (set, "gtk-print-time");
              if (option2)
                {
                  if (strcmp (opts[i].value, "indefinite") == 0)
                    gtk_printer_option_set (option2, "on-hold");
                  else
                    {
                      gtk_printer_option_set (option2, "at");
                      gtk_printer_option_set (option, opts[i].value);
                    }
                }
            }
        }
      else if (strcmp (name, "cups-sides") == 0)
        {
          option = gtk_printer_option_set_lookup (set, "gtk-duplex");
          if (option && opts[i].value)
            {
              if (strcmp (opts[i].value, "two-sided-short-edge") == 0)
                gtk_printer_option_set (option, "DuplexTumble");
              else if (strcmp (opts[i].value, "two-sided-long-edge") == 0)
                gtk_printer_option_set (option, "DuplexNoTumble");
            }
        }
      else
        {
          option = gtk_printer_option_set_lookup (set, name);
          if (option)
            gtk_printer_option_set (option, opts[i].value);
        }
      g_free (name);
    }

  cupsFreeOptions (num_opts, opts);

#ifdef HAVE_COLORD
  /* TRANSLATORS: this this the ICC color profile to use for this job */
  option = gtk_printer_option_new ("colord-profile",
                                   _("Printer Profile"),
                                   GTK_PRINTER_OPTION_TYPE_INFO);

  /* assign it to the color page */
  option->group = g_strdup ("ColorPage");

  /* TRANSLATORS: this is when color profile information is unavailable */
  gtk_printer_option_set (option, _("Unavailable"));
  gtk_printer_option_set_add (set, option);

  /* watch to see if the user changed the options */
  helper = g_new (GtkPrintBackendCupsColordHelper, 1);
  helper->printer = printer;
  helper->settings = settings;
  g_signal_connect_data (set, "changed",
                         G_CALLBACK (colord_printer_option_set_changed_cb),
                         helper,
                         (GClosureNotify) g_free,
                         0);

  /* initial coldplug */
  gtk_printer_cups_update_settings (GTK_PRINTER_CUPS (printer),
                                    settings, set);
  g_object_bind_property (printer, "profile-title",
                          option, "value",
                          G_BINDING_DEFAULT);

#endif

  return set;
}


static void
mark_option_from_set (GtkPrinterOptionSet *set,
		      ppd_file_t          *ppd_file,
		      ppd_option_t        *ppd_option)
{
  GtkPrinterOption *option;
  char *name = get_ppd_option_name (ppd_option->keyword);

  option = gtk_printer_option_set_lookup (set, name);

  if (option)
    ppdMarkOption (ppd_file, ppd_option->keyword, option->value);

  g_free (name);
}


static void
mark_group_from_set (GtkPrinterOptionSet *set,
		     ppd_file_t          *ppd_file,
		     ppd_group_t         *group)
{
  int i;

  for (i = 0; i < group->num_options; i++)
    mark_option_from_set (set, ppd_file, &group->options[i]);

  for (i = 0; i < group->num_subgroups; i++)
    mark_group_from_set (set, ppd_file, &group->subgroups[i]);
}

static void
set_conflicts_from_option (GtkPrinterOptionSet *set,
			   ppd_file_t          *ppd_file,
			   ppd_option_t        *ppd_option)
{
  GtkPrinterOption *option;
  char *name;

  if (ppd_option->conflicted)
    {
      name = get_ppd_option_name (ppd_option->keyword);
      option = gtk_printer_option_set_lookup (set, name);

      if (option)
	gtk_printer_option_set_has_conflict (option, TRUE);
#ifdef PRINT_IGNORED_OPTIONS
      else
	g_warning ("CUPS Backend: Ignoring conflict for option %s", ppd_option->keyword);
#endif

      g_free (name);
    }
}

static void
set_conflicts_from_group (GtkPrinterOptionSet *set,
			  ppd_file_t          *ppd_file,
			  ppd_group_t         *group)
{
  int i;

  for (i = 0; i < group->num_options; i++)
    set_conflicts_from_option (set, ppd_file, &group->options[i]);

  for (i = 0; i < group->num_subgroups; i++)
    set_conflicts_from_group (set, ppd_file, &group->subgroups[i]);
}

static gboolean
cups_printer_mark_conflicts (GtkPrinter          *printer,
			     GtkPrinterOptionSet *options)
{
  ppd_file_t *ppd_file;
  int num_conflicts;
  int i;

  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));

  if (ppd_file == NULL)
    return FALSE;

  ppdMarkDefaults (ppd_file);

  for (i = 0; i < ppd_file->num_groups; i++)
    mark_group_from_set (options, ppd_file, &ppd_file->groups[i]);

  num_conflicts = ppdConflicts (ppd_file);

  if (num_conflicts > 0)
    {
      for (i = 0; i < ppd_file->num_groups; i++)
	set_conflicts_from_group (options, ppd_file, &ppd_file->groups[i]);
    }

  return num_conflicts > 0;
}

struct OptionData {
  GtkPrinter *printer;
  GtkPrinterOptionSet *options;
  GtkPrintSettings *settings;
  ppd_file_t *ppd_file;
};

typedef struct {
  const char *cups;
  const char *standard;
} NameMapping;

static void
map_settings_to_option (GtkPrinterOption  *option,
			const NameMapping  table[],
			gint               n_elements,
			GtkPrintSettings  *settings,
			const gchar       *standard_name,
			const gchar       *cups_name)
{
  int i;
  char *name;
  const char *cups_value;
  const char *standard_value;

  /* If the cups-specific setting is set, always use that */
  name = g_strdup_printf ("cups-%s", cups_name);
  cups_value = gtk_print_settings_get (settings, name);
  g_free (name);

  if (cups_value != NULL)
    {
      gtk_printer_option_set (option, cups_value);
      return;
    }

  /* Otherwise we try to convert from the general setting */
  standard_value = gtk_print_settings_get (settings, standard_name);
  if (standard_value == NULL)
    return;

  for (i = 0; i < n_elements; i++)
    {
      if (table[i].cups == NULL && table[i].standard == NULL)
	{
	  gtk_printer_option_set (option, standard_value);
	  break;
	}
      else if (table[i].cups == NULL &&
	       strcmp (table[i].standard, standard_value) == 0)
	{
	  set_option_off (option);
	  break;
	}
      else if (strcmp (table[i].standard, standard_value) == 0)
	{
	  gtk_printer_option_set (option, table[i].cups);
	  break;
	}
    }
}

static void
map_option_to_settings (const gchar       *value,
			const NameMapping  table[],
			gint               n_elements,
			GtkPrintSettings  *settings,
			const gchar       *standard_name,
			const gchar       *cups_name)
{
  int i;
  char *name;

  for (i = 0; i < n_elements; i++)
    {
      if (table[i].cups == NULL && table[i].standard == NULL)
	{
	  gtk_print_settings_set (settings,
				  standard_name,
				  value);
	  break;
	}
      else if (table[i].cups == NULL && table[i].standard != NULL)
	{
	  if (value_is_off (value))
	    {
	      gtk_print_settings_set (settings,
				      standard_name,
				      table[i].standard);
	      break;
	    }
	}
      else if (strcmp (table[i].cups, value) == 0)
	{
	  gtk_print_settings_set (settings,
				  standard_name,
				  table[i].standard);
	  break;
	}
    }

  /* Always set the corresponding cups-specific setting */
  name = g_strdup_printf ("cups-%s", cups_name);
  gtk_print_settings_set (settings, name, value);
  g_free (name);
}


static const NameMapping paper_source_map[] = {
  { "Lower", "lower"},
  { "Middle", "middle"},
  { "Upper", "upper"},
  { "Rear", "rear"},
  { "Envelope", "envelope"},
  { "Cassette", "cassette"},
  { "LargeCapacity", "large-capacity"},
  { "AnySmallFormat", "small-format"},
  { "AnyLargeFormat", "large-format"},
  { NULL, NULL}
};

static const NameMapping output_tray_map[] = {
  { "Upper", "upper"},
  { "Lower", "lower"},
  { "Rear", "rear"},
  { NULL, NULL}
};

static const NameMapping duplex_map[] = {
  { "DuplexTumble", "vertical" },
  { "DuplexNoTumble", "horizontal" },
  { NULL, "simplex" }
};

static const NameMapping output_mode_map[] = {
  { "Standard", "normal" },
  { "Normal", "normal" },
  { "Draft", "draft" },
  { "Fast", "draft" },
};

static const NameMapping media_type_map[] = {
  { "Transparency", "transparency"},
  { "Standard", "stationery"},
  { NULL, NULL}
};

static const NameMapping all_map[] = {
  { NULL, NULL}
};


static void
set_option_from_settings (GtkPrinterOption *option,
			  GtkPrintSettings *settings)
{
  const char *cups_value;
  char *value;

  if (settings == NULL)
    return;

  if (strcmp (option->name, "gtk-paper-source") == 0)
    map_settings_to_option (option, paper_source_map, G_N_ELEMENTS (paper_source_map),
			     settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE, "InputSlot");
  else if (strcmp (option->name, "gtk-output-tray") == 0)
    map_settings_to_option (option, output_tray_map, G_N_ELEMENTS (output_tray_map),
			    settings, GTK_PRINT_SETTINGS_OUTPUT_BIN, "OutputBin");
  else if (strcmp (option->name, "gtk-duplex") == 0)
    map_settings_to_option (option, duplex_map, G_N_ELEMENTS (duplex_map),
			    settings, GTK_PRINT_SETTINGS_DUPLEX, "Duplex");
  else if (strcmp (option->name, "cups-OutputMode") == 0)
    map_settings_to_option (option, output_mode_map, G_N_ELEMENTS (output_mode_map),
			    settings, GTK_PRINT_SETTINGS_QUALITY, "OutputMode");
  else if (strcmp (option->name, "cups-Resolution") == 0)
    {
      cups_value = gtk_print_settings_get (settings, option->name);
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
      else
	{
	  if (gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION, -1) != -1 ||
	      gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION_X, -1) != -1 ||
	      gtk_print_settings_get_int_with_default (settings, GTK_PRINT_SETTINGS_RESOLUTION_Y, -1) != -1 ||
	      option->value == NULL || option->value[0] == '\0')
	    {
              int res = gtk_print_settings_get_resolution (settings);
              int res_x = gtk_print_settings_get_resolution_x (settings);
              int res_y = gtk_print_settings_get_resolution_y (settings);

              if (res_x != res_y)
                {
                  value = g_strdup_printf ("%dx%ddpi", res_x, res_y);
                  gtk_printer_option_set (option, value);
                  g_free (value);
                }
              else if (res != 0)
                {
                  value = g_strdup_printf ("%ddpi", res);
                  gtk_printer_option_set (option, value);
                  g_free (value);
                }
            }
        }
    }
  else if (strcmp (option->name, "gtk-paper-type") == 0)
    map_settings_to_option (option, media_type_map, G_N_ELEMENTS (media_type_map),
			    settings, GTK_PRINT_SETTINGS_MEDIA_TYPE, "MediaType");
  else if (strcmp (option->name, "gtk-n-up") == 0)
    {
      map_settings_to_option (option, all_map, G_N_ELEMENTS (all_map),
			      settings, GTK_PRINT_SETTINGS_NUMBER_UP, "number-up");
    }
  else if (strcmp (option->name, "gtk-n-up-layout") == 0)
    {
      map_settings_to_option (option, all_map, G_N_ELEMENTS (all_map),
			      settings, GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT, "number-up-layout");
    }
  else if (strcmp (option->name, "gtk-billing-info") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "cups-job-billing");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (strcmp (option->name, "gtk-job-prio") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "cups-job-priority");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (strcmp (option->name, "gtk-cover-before") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "cover-before");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (strcmp (option->name, "gtk-cover-after") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "cover-after");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (strcmp (option->name, "gtk-print-time") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "print-at");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (strcmp (option->name, "gtk-print-time-text") == 0)
    {
      cups_value = gtk_print_settings_get (settings, "print-at-time");
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
  else if (g_str_has_prefix (option->name, "cups-"))
    {
      cups_value = gtk_print_settings_get (settings, option->name);
      if (cups_value)
	gtk_printer_option_set (option, cups_value);
    }
}

static void
foreach_option_get_settings (GtkPrinterOption *option,
			     gpointer          user_data)
{
  struct OptionData *data = user_data;
  GtkPrintSettings *settings = data->settings;
  const char *value;

  value = option->value;

  if (strcmp (option->name, "gtk-paper-source") == 0)
    map_option_to_settings (value, paper_source_map, G_N_ELEMENTS (paper_source_map),
			    settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE, "InputSlot");
  else if (strcmp (option->name, "gtk-output-tray") == 0)
    map_option_to_settings (value, output_tray_map, G_N_ELEMENTS (output_tray_map),
			    settings, GTK_PRINT_SETTINGS_OUTPUT_BIN, "OutputBin");
  else if (strcmp (option->name, "gtk-duplex") == 0)
    map_option_to_settings (value, duplex_map, G_N_ELEMENTS (duplex_map),
			    settings, GTK_PRINT_SETTINGS_DUPLEX, "Duplex");
  else if (strcmp (option->name, "cups-OutputMode") == 0)
    map_option_to_settings (value, output_mode_map, G_N_ELEMENTS (output_mode_map),
			    settings, GTK_PRINT_SETTINGS_QUALITY, "OutputMode");
  else if (strcmp (option->name, "cups-Resolution") == 0)
    {
      int res, res_x, res_y;

      if (sscanf (value, "%dx%ddpi", &res_x, &res_y) == 2)
        {
          if (res_x > 0 && res_y > 0)
            gtk_print_settings_set_resolution_xy (settings, res_x, res_y);
        }
      else if (sscanf (value, "%ddpi", &res) == 1)
        {
          if (res > 0)
            gtk_print_settings_set_resolution (settings, res);
        }

      gtk_print_settings_set (settings, option->name, value);
    }
  else if (strcmp (option->name, "gtk-paper-type") == 0)
    map_option_to_settings (value, media_type_map, G_N_ELEMENTS (media_type_map),
			    settings, GTK_PRINT_SETTINGS_MEDIA_TYPE, "MediaType");
  else if (strcmp (option->name, "gtk-n-up") == 0)
    map_option_to_settings (value, all_map, G_N_ELEMENTS (all_map),
			    settings, GTK_PRINT_SETTINGS_NUMBER_UP, "number-up");
  else if (strcmp (option->name, "gtk-n-up-layout") == 0)
    map_option_to_settings (value, all_map, G_N_ELEMENTS (all_map),
			    settings, GTK_PRINT_SETTINGS_NUMBER_UP_LAYOUT, "number-up-layout");
  else if (strcmp (option->name, "gtk-billing-info") == 0 && strlen (value) > 0)
    gtk_print_settings_set (settings, "cups-job-billing", value);
  else if (strcmp (option->name, "gtk-job-prio") == 0)
    gtk_print_settings_set (settings, "cups-job-priority", value);
  else if (strcmp (option->name, "gtk-cover-before") == 0)
    gtk_print_settings_set (settings, "cover-before", value);
  else if (strcmp (option->name, "gtk-cover-after") == 0)
    gtk_print_settings_set (settings, "cover-after", value);
  else if (strcmp (option->name, "gtk-print-time") == 0)
    gtk_print_settings_set (settings, "print-at", value);
  else if (strcmp (option->name, "gtk-print-time-text") == 0)
    gtk_print_settings_set (settings, "print-at-time", value);
  else if (g_str_has_prefix (option->name, "cups-"))
    gtk_print_settings_set (settings, option->name, value);
}

static gboolean
supports_am_pm (void)
{
  struct tm tmp_tm = { 0 };
  char   time[8];
  int    length;

  length = strftime (time, sizeof (time), "%p", &tmp_tm);

  return length != 0;
}

/* Converts local time to UTC time. Local time has to be in one of these
 * formats:  HH:MM:SS, HH:MM, HH:MM:SS {am, pm}, HH:MM {am, pm}, HH {am, pm},
 * {am, pm} HH:MM:SS, {am, pm} HH:MM, {am, pm} HH.
 * Returns a newly allocated string holding UTC time in HH:MM:SS format
 * or NULL.
 */
gchar *
localtime_to_utctime (const char *local_time)
{
  const char *formats_0[] = {" %I : %M : %S %p ", " %p %I : %M : %S ",
                             " %H : %M : %S ",
                             " %I : %M %p ", " %p %I : %M ",
                             " %H : %M ",
                             " %I %p ", " %p %I "};
  const char *formats_1[] = {" %H : %M : %S ", " %H : %M "};
  const char *end = NULL;
  struct tm  *actual_local_time;
  struct tm  *actual_utc_time;
  struct tm   local_print_time;
  struct tm   utc_print_time;
  struct tm   diff_time;
  gchar      *utc_time = NULL;
  int         i, n;

  if (local_time == NULL || local_time[0] == '\0')
    return NULL;

  n = supports_am_pm () ? G_N_ELEMENTS (formats_0) : G_N_ELEMENTS (formats_1);

  for (i = 0; i < n; i++)
    {
      local_print_time.tm_hour = 0;
      local_print_time.tm_min  = 0;
      local_print_time.tm_sec  = 0;

      if (supports_am_pm ())
        end = strptime (local_time, formats_0[i], &local_print_time);
      else
        end = strptime (local_time, formats_1[i], &local_print_time);

      if (end != NULL && end[0] == '\0')
        break;
    }

  if (end != NULL && end[0] == '\0')
    {
      time_t rawtime;
      time (&rawtime);

      actual_utc_time = g_memdup (gmtime (&rawtime), sizeof (struct tm));
      actual_local_time = g_memdup (localtime (&rawtime), sizeof (struct tm));

      diff_time.tm_hour = actual_utc_time->tm_hour - actual_local_time->tm_hour;
      diff_time.tm_min  = actual_utc_time->tm_min  - actual_local_time->tm_min;
      diff_time.tm_sec  = actual_utc_time->tm_sec  - actual_local_time->tm_sec;

      utc_print_time.tm_hour = ((local_print_time.tm_hour + diff_time.tm_hour) + 24) % 24;
      utc_print_time.tm_min  = ((local_print_time.tm_min  + diff_time.tm_min)  + 60) % 60;
      utc_print_time.tm_sec  = ((local_print_time.tm_sec  + diff_time.tm_sec)  + 60) % 60;

      utc_time = g_strdup_printf ("%02d:%02d:%02d",
                                  utc_print_time.tm_hour,
                                  utc_print_time.tm_min,
                                  utc_print_time.tm_sec);
    }

  return utc_time;
}

static void
cups_printer_get_settings_from_options (GtkPrinter          *printer,
					GtkPrinterOptionSet *options,
					GtkPrintSettings    *settings)
{
  struct OptionData data;
  const char *print_at, *print_at_time;

  data.printer = printer;
  data.options = options;
  data.settings = settings;
  data.ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));

  gtk_printer_option_set_foreach (options, foreach_option_get_settings, &data);
  if (data.ppd_file != NULL)
    {
      GtkPrinterOption *cover_before, *cover_after;

      cover_before = gtk_printer_option_set_lookup (options, "gtk-cover-before");
      cover_after = gtk_printer_option_set_lookup (options, "gtk-cover-after");
      if (cover_before && cover_after)
	{
	  char *value = g_strdup_printf ("%s,%s", cover_before->value, cover_after->value);
	  gtk_print_settings_set (settings, "cups-job-sheets", value);
	  g_free (value);
	}

      print_at = gtk_print_settings_get (settings, "print-at");
      print_at_time = gtk_print_settings_get (settings, "print-at-time");

      if (strcmp (print_at, "at") == 0)
        {
          gchar *utc_time = NULL;

          utc_time = localtime_to_utctime (print_at_time);

          if (utc_time != NULL)
            {
              gtk_print_settings_set (settings, "cups-job-hold-until", utc_time);
              g_free (utc_time);
            }
          else
            gtk_print_settings_set (settings, "cups-job-hold-until", print_at_time);
        }
      else if (strcmp (print_at, "on-hold") == 0)
	gtk_print_settings_set (settings, "cups-job-hold-until", "indefinite");
    }
}

static void
cups_printer_prepare_for_print (GtkPrinter       *printer,
				GtkPrintJob      *print_job,
				GtkPrintSettings *settings,
				GtkPageSetup     *page_setup)
{
  GtkPrintPages pages;
  GtkPageRange *ranges;
  gint n_ranges;
  GtkPageSet page_set;
  GtkPaperSize *paper_size;
  const char *ppd_paper_name;
  double scale;
  GtkPrintCapabilities  capabilities;

  capabilities = cups_printer_get_capabilities (printer);
  pages = gtk_print_settings_get_print_pages (settings);
  gtk_print_job_set_pages (print_job, pages);

  if (pages == GTK_PRINT_PAGES_RANGES)
    ranges = gtk_print_settings_get_page_ranges (settings, &n_ranges);
  else
    {
      ranges = NULL;
      n_ranges = 0;
    }

  gtk_print_job_set_page_ranges (print_job, ranges, n_ranges);

  if (capabilities & GTK_PRINT_CAPABILITY_COLLATE)
    {
      if (gtk_print_settings_get_collate (settings))
        gtk_print_settings_set (settings, "cups-Collate", "True");
      gtk_print_job_set_collate (print_job, FALSE);
    }
  else
    {
      gtk_print_job_set_collate (print_job, gtk_print_settings_get_collate (settings));
    }

  if (capabilities & GTK_PRINT_CAPABILITY_REVERSE)
    {
      if (gtk_print_settings_get_reverse (settings))
        gtk_print_settings_set (settings, "cups-OutputOrder", "Reverse");
      gtk_print_job_set_reverse (print_job, FALSE);
    }
  else
    {
      gtk_print_job_set_reverse (print_job, gtk_print_settings_get_reverse (settings));
    }

  if (capabilities & GTK_PRINT_CAPABILITY_COPIES)
    {
      if (gtk_print_settings_get_n_copies (settings) > 1)
        gtk_print_settings_set_int (settings, "cups-copies",
                                    gtk_print_settings_get_n_copies (settings));
      gtk_print_job_set_num_copies (print_job, 1);
    }
  else
    {
      gtk_print_job_set_num_copies (print_job, gtk_print_settings_get_n_copies (settings));
    }

  scale = gtk_print_settings_get_scale (settings);
  if (scale != 100.0)
    gtk_print_job_set_scale (print_job, scale / 100.0);

  page_set = gtk_print_settings_get_page_set (settings);
  if (page_set == GTK_PAGE_SET_EVEN)
    gtk_print_settings_set (settings, "cups-page-set", "even");
  else if (page_set == GTK_PAGE_SET_ODD)
    gtk_print_settings_set (settings, "cups-page-set", "odd");
  gtk_print_job_set_page_set (print_job, GTK_PAGE_SET_ALL);

  paper_size = gtk_page_setup_get_paper_size (page_setup);
  ppd_paper_name = gtk_paper_size_get_ppd_name (paper_size);
  if (ppd_paper_name != NULL)
    gtk_print_settings_set (settings, "cups-PageSize", ppd_paper_name);
  else
    {
      char width[G_ASCII_DTOSTR_BUF_SIZE];
      char height[G_ASCII_DTOSTR_BUF_SIZE];
      char *custom_name;

      g_ascii_formatd (width, sizeof (width), "%.2f", gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS));
      g_ascii_formatd (height, sizeof (height), "%.2f", gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS));
      custom_name = g_strdup_printf (("Custom.%sx%s"), width, height);
      gtk_print_settings_set (settings, "cups-PageSize", custom_name);
      g_free (custom_name);
    }

  if (gtk_print_settings_get_number_up (settings) > 1)
    {
      GtkNumberUpLayout  layout = gtk_print_settings_get_number_up_layout (settings);
      GEnumClass        *enum_class;
      GEnumValue        *enum_value;

      switch (gtk_page_setup_get_orientation (page_setup))
        {
          case GTK_PAGE_ORIENTATION_PORTRAIT:
            break;
          case GTK_PAGE_ORIENTATION_LANDSCAPE:
            if (layout < 4)
              layout = layout + 2 + 4 * (1 - layout / 2);
            else
              layout = layout - 3 - 2 * (layout % 2);
            break;
          case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
            layout = (layout + 3 - 2 * (layout % 2)) % 4 + 4 * (layout / 4);
            break;
          case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
            if (layout < 4)
              layout = layout + 5 - 2 * (layout % 2);
            else
              layout = layout - 6 + 4 * (1 - (layout - 4) / 2);
            break;
        }

      enum_class = g_type_class_ref (GTK_TYPE_NUMBER_UP_LAYOUT);
      enum_value = g_enum_get_value (enum_class, layout);
      gtk_print_settings_set (settings, "cups-number-up-layout", enum_value->value_nick);
      g_type_class_unref (enum_class);

      if (!(capabilities & GTK_PRINT_CAPABILITY_NUMBER_UP))
        {
          gtk_print_job_set_n_up (print_job, gtk_print_settings_get_number_up (settings));
          gtk_print_job_set_n_up_layout (print_job, gtk_print_settings_get_number_up_layout (settings));
        }
    }

  gtk_print_job_set_rotate (print_job, TRUE);
}

static GtkPageSetup *
create_page_setup (ppd_file_t *ppd_file,
		   ppd_size_t *size)
 {
   char *display_name;
   GtkPageSetup *page_setup;
   GtkPaperSize *paper_size;
   ppd_option_t *option;
   ppd_choice_t *choice;

  display_name = NULL;
  option = ppdFindOption (ppd_file, "PageSize");
  if (option)
    {
      choice = ppdFindChoice (option, size->name);
      if (choice)
	display_name = ppd_text_to_utf8 (ppd_file, choice->text);
    }

  if (display_name == NULL)
    display_name = g_strdup (size->name);

  page_setup = gtk_page_setup_new ();
  paper_size = gtk_paper_size_new_from_ppd (size->name,
					    display_name,
					    size->width,
					    size->length);
  gtk_page_setup_set_paper_size (page_setup, paper_size);
  gtk_paper_size_free (paper_size);

  gtk_page_setup_set_top_margin (page_setup, size->length - size->top, GTK_UNIT_POINTS);
  gtk_page_setup_set_bottom_margin (page_setup, size->bottom, GTK_UNIT_POINTS);
  gtk_page_setup_set_left_margin (page_setup, size->left, GTK_UNIT_POINTS);
  gtk_page_setup_set_right_margin (page_setup, size->width - size->right, GTK_UNIT_POINTS);

  g_free (display_name);

  return page_setup;
}

static GList *
cups_printer_list_papers (GtkPrinter *printer)
{
  ppd_file_t *ppd_file;
  ppd_size_t *size;
  GtkPageSetup *page_setup;
  GList *l;
  int i;

  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));
  if (ppd_file == NULL)
    return NULL;

  l = NULL;

  for (i = 0; i < ppd_file->num_sizes; i++)
    {
      size = &ppd_file->sizes[i];

      page_setup = create_page_setup (ppd_file, size);

      l = g_list_prepend (l, page_setup);
    }

  return g_list_reverse (l);
}

static GtkPageSetup *
cups_printer_get_default_page_size (GtkPrinter *printer)
{
  ppd_file_t *ppd_file;
  ppd_size_t *size;
  ppd_option_t *option;


  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));
  if (ppd_file == NULL)
    return NULL;

  option = ppdFindOption (ppd_file, "PageSize");
  if (option == NULL)
    return NULL;

  size = ppdPageSize (ppd_file, option->defchoice);
  if (size == NULL)
    return NULL;

  return create_page_setup (ppd_file, size);
}

static gboolean
cups_printer_get_hard_margins (GtkPrinter *printer,
			       gdouble    *top,
			       gdouble    *bottom,
			       gdouble    *left,
			       gdouble    *right)
{
  ppd_file_t *ppd_file;

  ppd_file = gtk_printer_cups_get_ppd (GTK_PRINTER_CUPS (printer));
  if (ppd_file == NULL)
    return FALSE;

  *left = ppd_file->custom_margins[0];
  *bottom = ppd_file->custom_margins[1];
  *right = ppd_file->custom_margins[2];
  *top = ppd_file->custom_margins[3];

  return TRUE;
}

static GtkPrintCapabilities
cups_printer_get_capabilities (GtkPrinter *printer)
{
  GtkPrintCapabilities  capabilities = 0;
  GtkPrinterCups       *cups_printer = GTK_PRINTER_CUPS (printer);

  if (gtk_printer_cups_get_ppd (cups_printer))
    {
      capabilities = GTK_PRINT_CAPABILITY_REVERSE;
    }

  if (cups_printer->supports_copies)
    {
      capabilities |= GTK_PRINT_CAPABILITY_COPIES;
    }

  if (cups_printer->supports_collate)
    {
      capabilities |= GTK_PRINT_CAPABILITY_COLLATE;
    }

  if (cups_printer->supports_number_up)
    {
      capabilities |= GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT |
                      GTK_PRINT_CAPABILITY_NUMBER_UP;
    }

  return capabilities;
}

static void
secrets_service_appeared_cb (GDBusConnection *connection,
                             const gchar     *name,
                             const gchar     *name_owner,
                             gpointer         user_data)
{
  GtkPrintBackendCups *backend = GTK_PRINT_BACKEND_CUPS (user_data);

  backend->secrets_service_available = TRUE;
}

static void
secrets_service_vanished_cb (GDBusConnection *connection,
                             const gchar     *name,
                             gpointer         user_data)
{
  GtkPrintBackendCups *backend = GTK_PRINT_BACKEND_CUPS (user_data);

  backend->secrets_service_available = FALSE;
}
