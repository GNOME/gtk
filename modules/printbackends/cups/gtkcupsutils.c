/* GTK - The GIMP Toolkit
 * gtkcupsutils.h: Statemachine implementation of POST and GET 
 * cup calls which can be used to create a non-blocking cups API
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "gtkcupsutils.h"
#include "config.h"
#include "gtkdebug.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>

#if CUPS_VERSION_MAJOR > 1 || (CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR > 1) || (CUPS_VERSION_MAJOR == 1 && CUPS_VERSION_MINOR == 1 && CUPS_VERSION_PATCH >= 20)
#define HAVE_HTTP_AUTHSTRING 1
#endif

typedef void (*GtkCupsRequestStateFunc) (GtkCupsRequest *request);

static void _connect            (GtkCupsRequest *request);
static void _post_send          (GtkCupsRequest *request);
static void _post_write_request (GtkCupsRequest *request);
static void _post_write_data    (GtkCupsRequest *request);
static void _post_check         (GtkCupsRequest *request);
static void _post_read_response (GtkCupsRequest *request);

static void _get_send           (GtkCupsRequest *request);
static void _get_check          (GtkCupsRequest *request);
static void _get_read_data      (GtkCupsRequest *request);

struct _GtkCupsResult
{
  gchar *error_msg;
  ipp_t *ipp_response;
  GtkCupsErrorType error_type;

  /* some error types like HTTP_ERROR have a status and a code */
  int error_status;            
  int error_code;

  guint is_error : 1;
  guint is_ipp_response : 1;
};


#define _GTK_CUPS_MAX_ATTEMPTS 10 
#define _GTK_CUPS_MAX_CHUNK_SIZE 8192

GtkCupsRequestStateFunc post_states[] = {_connect,
                                         _post_send,
                                         _post_write_request,
                                         _post_write_data,
                                         _post_check,
                                         _post_read_response};

GtkCupsRequestStateFunc get_states[] = {_connect,
                                        _get_send,
                                        _get_check,
                                        _get_read_data};

static void
gtk_cups_result_set_error (GtkCupsResult    *result,
                           GtkCupsErrorType  error_type,
                           int               error_status,
                           int               error_code, 
                           const char       *error_msg,
			   ...)
{
  va_list args;

  result->is_ipp_response = FALSE;
  result->is_error = TRUE;
  result->error_type = error_type;
  result->error_status = error_status;
  result->error_code = error_code;

  va_start (args, error_msg);
  result->error_msg = g_strdup_vprintf (error_msg, args);
  va_end (args);
}

GtkCupsRequest *
gtk_cups_request_new (http_t *connection,
                      GtkCupsRequestType req_type, 
                      gint operation_id,
                      GIOChannel *data_io,
                      const char *server,
                      const char *resource)
{
  GtkCupsRequest *request;
  cups_lang_t *language;
  
  request = g_new0 (GtkCupsRequest, 1);
  request->result = g_new0 (GtkCupsResult, 1);

  request->result->error_msg = NULL;
  request->result->ipp_response = NULL;

  request->result->is_error = FALSE;
  request->result->is_ipp_response = FALSE;

  request->type = req_type;
  request->state = GTK_CUPS_REQUEST_START;

   if (server)
    request->server = g_strdup (server);
  else
    request->server = g_strdup (cupsServer());


  if (resource)
    request->resource = g_strdup (resource);
  else
    request->resource = g_strdup ("/");
 
  if (connection != NULL)
    {
      request->http = connection;
      request->own_http = FALSE;
    }
  else
    {
      request->http = NULL;
      request->http = httpConnectEncrypt (request->server, ippPort(), cupsEncryption());

      if (request->http)
        httpBlocking (request->http, 0);
        
      request->own_http = TRUE;
    }

  request->last_status = HTTP_CONTINUE;

  request->attempts = 0;
  request->data_io = data_io;

  request->ipp_request = ippNew();
  request->ipp_request->request.op.operation_id = operation_id;
  request->ipp_request->request.op.request_id = 1;

  language = cupsLangDefault ();

  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_CHARSET,
                                   "attributes-charset", 
                                   NULL, "utf-8");
	
  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_LANGUAGE,
                                   "attributes-natural-language", 
                                   NULL, language->language);

  gtk_cups_request_ipp_add_string (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                                   "requesting-user-name",
                                   NULL, cupsUser());
  
  cupsLangFree (language);

  return request;
}

static void
gtk_cups_result_free (GtkCupsResult *result)
{
  g_free (result->error_msg);

  if (result->ipp_response)
    ippDelete (result->ipp_response);

  g_free (result);
}

void
gtk_cups_request_free (GtkCupsRequest *request)
{
  if (request->own_http)
    if (request->http)
      httpClose (request->http);
  
  if (request->ipp_request)
    ippDelete (request->ipp_request);

  g_free (request->server);
  g_free (request->resource);

  gtk_cups_result_free (request->result);

  g_free (request);
}

gboolean 
gtk_cups_request_read_write (GtkCupsRequest *request)
{
  if (request->type == GTK_CUPS_POST)
    post_states[request->state](request);
  else if (request->type == GTK_CUPS_GET)
    get_states[request->state](request);

  if (request->attempts > _GTK_CUPS_MAX_ATTEMPTS && 
      request->state != GTK_CUPS_REQUEST_DONE)
    {
      /* TODO: should add a status or error code for too many failed attempts */
      gtk_cups_result_set_error (request->result, 
                                 GTK_CUPS_ERROR_GENERAL,
                                 0,
                                 0, 
                                 "Too many failed attempts");

      request->state = GTK_CUPS_REQUEST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    }
    
  if (request->state == GTK_CUPS_REQUEST_DONE)
    {
      request->poll_state = GTK_CUPS_HTTP_IDLE;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

GtkCupsPollState 
gtk_cups_request_get_poll_state (GtkCupsRequest *request)
{
  return request->poll_state;
}



GtkCupsResult *
gtk_cups_request_get_result (GtkCupsRequest *request)
{
  return request->result;
}

void            
gtk_cups_request_ipp_add_string (GtkCupsRequest *request,
                                 ipp_tag_t group,
                                 ipp_tag_t tag,
                                 const char *name,
                                 const char *charset,
                                 const char *value)
{
  ippAddString (request->ipp_request,
                group,
                tag,
                name,
                charset,
                value);
}

void            
gtk_cups_request_ipp_add_strings (GtkCupsRequest *request,
				  ipp_tag_t group,
				  ipp_tag_t tag,
				  const char *name,
				  int num_values,
				  const char *charset,
				  const char * const *values)
{
  ippAddStrings (request->ipp_request,
		 group,
		 tag,
		 name,
		 num_values,
		 charset,
		 values);
}



typedef struct
{
  const char	*name;
  ipp_tag_t	value_tag;
} ipp_option_t;

static const ipp_option_t ipp_options[] =
			{
			  { "blackplot",		IPP_TAG_BOOLEAN },
			  { "brightness",		IPP_TAG_INTEGER },
			  { "columns",			IPP_TAG_INTEGER },
			  { "copies",			IPP_TAG_INTEGER },
			  { "finishings",		IPP_TAG_ENUM },
			  { "fitplot",			IPP_TAG_BOOLEAN },
			  { "gamma",			IPP_TAG_INTEGER },
			  { "hue",			IPP_TAG_INTEGER },
			  { "job-k-limit",		IPP_TAG_INTEGER },
			  { "job-page-limit",		IPP_TAG_INTEGER },
			  { "job-priority",		IPP_TAG_INTEGER },
			  { "job-quota-period",		IPP_TAG_INTEGER },
			  { "landscape",		IPP_TAG_BOOLEAN },
			  { "media",			IPP_TAG_KEYWORD },
			  { "mirror",			IPP_TAG_BOOLEAN },
			  { "natural-scaling",		IPP_TAG_INTEGER },
			  { "number-up",		IPP_TAG_INTEGER },
			  { "orientation-requested",	IPP_TAG_ENUM },
			  { "page-bottom",		IPP_TAG_INTEGER },
			  { "page-left",		IPP_TAG_INTEGER },
			  { "page-ranges",		IPP_TAG_RANGE },
			  { "page-right",		IPP_TAG_INTEGER },
			  { "page-top",			IPP_TAG_INTEGER },
			  { "penwidth",			IPP_TAG_INTEGER },
			  { "ppi",			IPP_TAG_INTEGER },
			  { "prettyprint",		IPP_TAG_BOOLEAN },
			  { "printer-resolution",	IPP_TAG_RESOLUTION },
			  { "print-quality",		IPP_TAG_ENUM },
			  { "saturation",		IPP_TAG_INTEGER },
			  { "scaling",			IPP_TAG_INTEGER },
			  { "sides",			IPP_TAG_KEYWORD },
			  { "wrap",			IPP_TAG_BOOLEAN }
			};


static ipp_tag_t
_find_option_tag (const gchar *option)
{
  int lower_bound, upper_bound, num_options;
  int current_option;
  ipp_tag_t result;

  result = IPP_TAG_ZERO;

  lower_bound = 0;
  upper_bound = num_options = (int)(sizeof(ipp_options) / sizeof(ipp_options[0])) - 1;
  
  while (1)
    {
      int match;
      current_option = (int) (((upper_bound - lower_bound) / 2) + lower_bound);

      match = strcasecmp(option, ipp_options[current_option].name);
      if (match == 0)
        {
	  result = ipp_options[current_option].value_tag;
	  return result;
	}
      else if (match < 0)
        {
          upper_bound = current_option - 1;
	}
      else
        {
          lower_bound = current_option + 1;
	}

      if (upper_bound == lower_bound && upper_bound == current_option)
        return result;

      if (upper_bound < 0)
        return result;

      if (lower_bound > num_options)
        return result;

      if (upper_bound < lower_bound)
        return result;
    }
}

/*
 * Note that this function uses IPP_TAG_JOB, so it is
 * only suitable for IPP Group 2 attributes.
 * See RFC 2911.
 */
void
gtk_cups_request_encode_option (GtkCupsRequest *request,
                                const gchar    *option,
			        const gchar    *value)
{
  ipp_tag_t option_tag;

  g_assert (option != NULL);
  g_assert (value != NULL);

  option_tag = _find_option_tag (option);

  if (option_tag == IPP_TAG_ZERO)
    {
      option_tag = IPP_TAG_NAME;
      if (strcasecmp (value, "true") == 0 ||
          strcasecmp (value, "false") == 0)
        {
          option_tag = IPP_TAG_BOOLEAN;
        }
    }
        
  switch (option_tag)
    {
      case IPP_TAG_INTEGER:
      case IPP_TAG_ENUM:
        ippAddInteger (request->ipp_request,
                       IPP_TAG_JOB,
                       option_tag,
                       option,
                       strtol (value, NULL, 0));
        break;

      case IPP_TAG_BOOLEAN:
        {
          char b;
          b = 0;
          if (!strcasecmp(value, "true") ||
	      !strcasecmp(value, "on") ||
	      !strcasecmp(value, "yes")) 
	    b = 1;
	  
          ippAddBoolean(request->ipp_request,
                        IPP_TAG_JOB,
                        option,
                        b);
        
          break;
        }
        
      case IPP_TAG_RANGE:
        {
          char	*s;
          int lower;
          int upper;
          
          if (*value == '-')
	    {
	      lower = 1;
	      s = (char *)value;
	    }
	  else
	    lower = strtol(value, &s, 0);

	  if (*s == '-')
	    {
	      if (s[1])
		upper = strtol(s + 1, NULL, 0);
	      else
		upper = 2147483647;
            }
	  else
	    upper = lower;
         
          ippAddRange (request->ipp_request,
                       IPP_TAG_JOB,
                       option,
                       lower,
                       upper);

          break;
        }

      case IPP_TAG_RESOLUTION:
        {
          char *s;
          int xres;
          int yres;
          ipp_res_t units;
          
          xres = strtol(value, &s, 0);

	  if (*s == 'x')
	    yres = strtol(s + 1, &s, 0);
	  else
	    yres = xres;

	  if (strcasecmp(s, "dpc") == 0)
            units = IPP_RES_PER_CM;
          else
            units = IPP_RES_PER_INCH;
          
          ippAddResolution (request->ipp_request,
                            IPP_TAG_JOB,
                            option,
                            units,
                            xres,
                            yres);

          break;
        }

      default:
        {
          char *values;
          char *s;
          int in_quotes;
          char *next;
          GPtrArray *strings;
          
          values = g_strdup (value);
          strings = NULL;
          in_quotes = 0;
          
          for (s = values, next = s; *s != '\0'; s++)
            {
              if (in_quotes != 2 && *s == '\'')
                {
                  /* skip quoted value */
                  if (in_quotes == 0)
                    in_quotes = 1;
                  else
                    in_quotes = 0;
                }
              else if (in_quotes != 1 && *s == '\"')
                {
                  /* skip quoted value */
                  if (in_quotes == 0)
                    in_quotes = 2;
                  else
                    in_quotes = 0;
                }
              else if (in_quotes == 0 && *s == ',')
                {
                  /* found delimiter, add to value array */
                  *s = '\0';
                  if (strings == NULL)
                    strings = g_ptr_array_new ();
                  g_ptr_array_add (strings, next);
                  next = s + 1;
                }
              else if (in_quotes == 0 && *s == '\\' && s[1] != '\0')
                {
                  /* skip escaped character */
                  s++;
                }
            }
          
          if (strings == NULL)
            {
              /* single value */
              ippAddString (request->ipp_request,
                            IPP_TAG_JOB,
                            option_tag,
                            option,
                            NULL,
                            value);
            }
          else
            {
              /* multiple values */
              
              /* add last value */
              g_ptr_array_add (strings, next);
              
              ippAddStrings (request->ipp_request,
                             IPP_TAG_JOB,
                             option_tag,
                             option,
                             strings->len,
                             NULL,
                             (const char **) strings->pdata);
              g_ptr_array_free (strings, TRUE);
            }

          g_free (values);
        }

        break;
    }
}
				

static void
_connect (GtkCupsRequest *request)
{
  request->poll_state = GTK_CUPS_HTTP_IDLE;

  if (request->http == NULL)
    {
      request->http = httpConnectEncrypt (request->server, ippPort(), cupsEncryption());

      if (request->http == NULL)
        request->attempts++;

      if (request->http)
        httpBlocking (request->http, 0);
        
      request->own_http = TRUE;
    }
  else
    {
      request->attempts = 0;
      request->state++;

      /* we always write to the socket after we get
         the connection */
      request->poll_state = GTK_CUPS_HTTP_WRITE;
    }
}

static void 
_post_send (GtkCupsRequest *request)
{
  gchar length[255];
  struct stat data_info;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  request->poll_state = GTK_CUPS_HTTP_WRITE;

  if (request->data_io != NULL)
    {
      fstat (g_io_channel_unix_get_fd (request->data_io), &data_info);
      sprintf (length, "%lu", (unsigned long)ippLength(request->ipp_request) + data_info.st_size);
    }
  else
    {
      sprintf (length, "%lu", (unsigned long)ippLength(request->ipp_request));
    }
	
  httpClearFields(request->http);
  httpSetField(request->http, HTTP_FIELD_CONTENT_LENGTH, length);
  httpSetField(request->http, HTTP_FIELD_CONTENT_TYPE, "application/ipp");
#ifdef HAVE_HTTP_AUTHSTRING
  httpSetField(request->http, HTTP_FIELD_AUTHORIZATION, request->http->authstring);
#endif

  if (httpPost(request->http, request->resource))
    {
      if (httpReconnect(request->http))
        {
          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;

          /* TODO: should add a status or error code for failed post */
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_GENERAL,
                                     0,
                                     0,
                                     "Failed Post");
        }

      request->attempts++;
      return;    
    }
        
    request->attempts = 0;

    request->state = GTK_CUPS_POST_WRITE_REQUEST;
    request->ipp_request->state = IPP_IDLE;
}

static void 
_post_write_request (GtkCupsRequest *request)
{
  ipp_state_t ipp_status;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  request->poll_state = GTK_CUPS_HTTP_WRITE;
  
  ipp_status = ippWrite(request->http, request->ipp_request);

  if (ipp_status == IPP_ERROR)
    {
      int cups_error = cupsLastError ();
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
 
      gtk_cups_result_set_error (request->result, 
                                 GTK_CUPS_ERROR_IPP,
                                 ipp_status,
                                 cups_error,
                                 "%s", 
                                 ippErrorString (cups_error));
      return;
    }

  if (ipp_status == IPP_DATA)
    {
      if (request->data_io != NULL)
        request->state = GTK_CUPS_POST_WRITE_DATA;
      else
        {
          request->state = GTK_CUPS_POST_CHECK;
          request->poll_state = GTK_CUPS_HTTP_READ;
	}
    }
}

static void 
_post_write_data (GtkCupsRequest *request)
{
  gsize bytes;
  char buffer[_GTK_CUPS_MAX_CHUNK_SIZE];
  http_status_t http_status;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  request->poll_state = GTK_CUPS_HTTP_WRITE;
  
  if (httpCheck (request->http))
    http_status = httpUpdate(request->http);
  else
    http_status = request->last_status;

  request->last_status = http_status;


  if (http_status == HTTP_CONTINUE || http_status == HTTP_OK)
    {
      GIOStatus io_status;
      GError *error;

      error = NULL;

      /* send data */
      io_status =
        g_io_channel_read_chars (request->data_io, 
	                         buffer, 
				 _GTK_CUPS_MAX_CHUNK_SIZE,
				 &bytes,
				 &error);

      if (io_status == G_IO_STATUS_ERROR)
        {
          request->state = GTK_CUPS_POST_DONE;
	  request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_IO,
                                     io_status,
                                     error->code, 
                                     "Error reading from cache file: %s",
                                     error->message);

	  g_error_free (error);
          return;
	}
      else if (bytes == 0 && io_status == G_IO_STATUS_EOF)
        {
          request->state = GTK_CUPS_POST_CHECK;
	  request->poll_state = GTK_CUPS_HTTP_READ;

          request->attempts = 0;
          return;
        }


#if HAVE_CUPS_API_1_2
      if (httpWrite2(request->http, buffer, bytes) < bytes)
#else
      if (httpWrite(request->http, buffer, (int) bytes) < bytes)
#endif /* HAVE_CUPS_API_1_2 */
        {
          int http_errno;

          http_errno = httpError (request->http);

          request->state = GTK_CUPS_POST_DONE;
	  request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "Error writing to socket in Post %s", 
                                     g_strerror (http_errno));
          return;
        }
    }
   else
    {
      request->attempts++;
    }
}

static void 
_post_check (GtkCupsRequest *request)
{
  http_status_t http_status;

  http_status = request->last_status;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s - status %i\n", G_STRFUNC, http_status));

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (http_status == HTTP_CONTINUE)
    {
      goto again; 
    }
  else if (http_status == HTTP_UNAUTHORIZED)
    {
      /* TODO: callout for auth */
      g_warning ("NOT IMPLEMENTED: We need to prompt for authorization");
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
      
      /* TODO: create a not implemented error code */
      gtk_cups_result_set_error (request->result, 
                                 GTK_CUPS_ERROR_GENERAL,
                                 0,
                                 0,
                                 "Can't prompt for authorization");
      return;
    }
  else if (http_status == HTTP_ERROR)
    {
      int error = httpError (request->http);
#ifdef G_OS_WIN32
      if (error != WSAENETDOWN && error != WSAENETUNREACH)
#else
      if (error != ENETDOWN && error != ENETUNREACH)	  
#endif /* G_OS_WIN32 */
        {
          request->attempts++;
          goto again;
        }
      else
        {
          request->state = GTK_CUPS_POST_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
     
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     error, 
                                     "Unknown HTTP error");

          return;
        }
    }
  else if (http_status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush (request->http);

      cupsSetEncryption (HTTP_ENCRYPT_REQUIRED);
      request->state = GTK_CUPS_POST_CONNECT;

      /* Reconnect... */
      httpReconnect (request->http);

      /* Upgrade with encryption... */
      httpEncryption(request->http, HTTP_ENCRYPT_REQUIRED);
 
      request->attempts++;
      goto again;
    }
  else if (http_status != HTTP_OK)
    {
      int http_errno;

      http_errno = httpError (request->http);

      if (http_errno == EPIPE)
        request->state = GTK_CUPS_POST_CONNECT;
      else
        {
          request->state = GTK_CUPS_POST_DONE;
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "HTTP Error in POST %s", 
                                     g_strerror (http_errno));
         request->poll_state = GTK_CUPS_HTTP_IDLE;
 
          httpFlush(request->http); 
          return;
        }

      request->poll_state = GTK_CUPS_HTTP_IDLE;
       
      httpFlush(request->http); 
      
      request->last_status = HTTP_CONTINUE;
      httpClose (request->http);
      request->http = NULL;
      return;  
    }
  else
    {
      request->state = GTK_CUPS_POST_READ_RESPONSE;
      return;
    }

 again:
  http_status = HTTP_CONTINUE;

  if (httpCheck (request->http))
    http_status = httpUpdate (request->http);

  request->last_status = http_status;
}

static void 
_post_read_response (GtkCupsRequest *request)
{
  ipp_state_t ipp_status;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (request->result->ipp_response == NULL)
    request->result->ipp_response = ippNew();

  ipp_status = ippRead (request->http, 
                        request->result->ipp_response);

  if (ipp_status == IPP_ERROR)
    {
      int ipp_error = cupsLastError ();
      gtk_cups_result_set_error (request->result,  
                                 GTK_CUPS_ERROR_IPP,
                                 ipp_status,
                                 ipp_error,
                                 "%s",
                                 ippErrorString (ipp_error));
      
      ippDelete (request->result->ipp_response);
      request->result->ipp_response = NULL;

      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    }
  else if (ipp_status == IPP_DATA)
    {
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    }
}

static void 
_get_send (GtkCupsRequest *request)
{
  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  request->poll_state = GTK_CUPS_HTTP_WRITE;

  if (request->data_io == NULL)
    {
      gtk_cups_result_set_error (request->result,
                                 GTK_CUPS_ERROR_IO,
                                 G_IO_STATUS_ERROR,
                                 G_IO_CHANNEL_ERROR_FAILED, 
                                 "Get requires an open io channel");

      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      return;
    }

  httpClearFields(request->http);
#ifdef HAVE_HTTP_AUTHSTRING
  httpSetField(request->http, HTTP_FIELD_AUTHORIZATION, request->http->authstring);
#endif

  if (httpGet(request->http, request->resource))
    {
      if (httpReconnect(request->http))
        {
          request->state = GTK_CUPS_GET_DONE;
          request->poll_state = GTK_CUPS_HTTP_IDLE;
	 
          /* TODO: should add a status or error code for failed GET */ 
          gtk_cups_result_set_error (request->result, 
                                     GTK_CUPS_ERROR_GENERAL,
                                     0,
                                     0,
                                     "Failed Get");
        }

      request->attempts++;
      return;    
    }
        
  request->attempts = 0;

  request->state = GTK_CUPS_GET_CHECK;
  request->poll_state = GTK_CUPS_HTTP_READ;
  
  request->ipp_request->state = IPP_IDLE;
}

static void 
_get_check (GtkCupsRequest *request)
{
  http_status_t http_status;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  http_status = request->last_status;

  request->poll_state = GTK_CUPS_HTTP_READ;

  if (http_status == HTTP_CONTINUE)
    {
      goto again; 
    }
  else if (http_status == HTTP_UNAUTHORIZED)
    {
      /* TODO: callout for auth */
      g_warning ("NOT IMPLEMENTED: We need to prompt for authorization in a non blocking manner");
      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      /* TODO: should add a status or error code for not implemented */ 
      gtk_cups_result_set_error (request->result, 
                                 GTK_CUPS_ERROR_GENERAL,
                                 0,
                                 0,
                                 "Can't prompt for authorization");
      return;
    }
  else if (http_status == HTTP_UPGRADE_REQUIRED)
    {
      /* Flush any error message... */
      httpFlush (request->http);

      cupsSetEncryption (HTTP_ENCRYPT_REQUIRED);
      request->state = GTK_CUPS_POST_CONNECT;

      /* Reconnect... */
      httpReconnect (request->http);

      /* Upgrade with encryption... */
      httpEncryption(request->http, HTTP_ENCRYPT_REQUIRED);
 
      request->attempts++;
      goto again;
    }
  else if (http_status != HTTP_OK)
    {
      int http_errno;

      http_errno = httpError (request->http);

      if (http_errno == EPIPE)
        request->state = GTK_CUPS_GET_CONNECT;
      else
        {
          request->state = GTK_CUPS_GET_DONE;
          gtk_cups_result_set_error (request->result,
                                     GTK_CUPS_ERROR_HTTP,
                                     http_status,
                                     http_errno, 
                                     "HTTP Error in GET %s", 
                                     g_strerror (http_errno));
          request->poll_state = GTK_CUPS_HTTP_IDLE;
          httpFlush(request->http);

          return;
        }

      request->poll_state = GTK_CUPS_HTTP_IDLE;
      httpFlush (request->http);
      httpClose (request->http);
      request->last_status = HTTP_CONTINUE;
      request->http = NULL;
      return;

    }
  else
    {
      request->state = GTK_CUPS_GET_READ_DATA;
      return;
    }

 again:
  http_status = HTTP_CONTINUE;

  if (httpCheck (request->http))
    http_status = httpUpdate (request->http);

  request->last_status = http_status;

}

static void 
_get_read_data (GtkCupsRequest *request)
{
  char buffer[_GTK_CUPS_MAX_CHUNK_SIZE];
  gsize bytes;
  gsize bytes_written;
  GIOStatus io_status;
  GError *error;

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %s\n", G_STRFUNC));

  error = NULL;

  request->poll_state = GTK_CUPS_HTTP_READ;

#if HAVE_CUPS_API_1_2
  bytes = httpRead2(request->http, buffer, sizeof(buffer));
#else
  bytes = httpRead(request->http, buffer, sizeof(buffer));
#endif /* HAVE_CUPS_API_1_2 */

  GTK_NOTE (PRINTING,
            g_print ("CUPS Backend: %i bytes read\n", bytes));
  
  if (bytes == 0)
    {
      request->state = GTK_CUPS_GET_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;

      return;
    }
  
  io_status =
    g_io_channel_write_chars (request->data_io, 
                              buffer, 
			      bytes, 
			      &bytes_written,
			      &error);

  if (io_status == G_IO_STATUS_ERROR)
    {
      request->state = GTK_CUPS_POST_DONE;
      request->poll_state = GTK_CUPS_HTTP_IDLE;
    
      gtk_cups_result_set_error (request->result,
                                 GTK_CUPS_ERROR_IO,
                                 io_status,
                                 error->code, 
                                 error->message);
      g_error_free (error);
    }
}

gboolean
gtk_cups_request_is_done (GtkCupsRequest *request)
{
  return (request->state == GTK_CUPS_REQUEST_DONE);
}

gboolean
gtk_cups_result_is_error (GtkCupsResult *result)
{
  return result->is_error;
}

ipp_t *
gtk_cups_result_get_response (GtkCupsResult *result)
{
  return result->ipp_response;
}

GtkCupsErrorType
gtk_cups_result_get_error_type (GtkCupsResult *result)
{
  return result->error_type;
}

int
gtk_cups_result_get_error_status (GtkCupsResult *result)
{
  return result->error_status;
}

int
gtk_cups_result_get_error_code (GtkCupsResult *result)
{
  return result->error_code;
}

const char *
gtk_cups_result_get_error_string (GtkCupsResult *result)
{
  return result->error_msg; 
}

