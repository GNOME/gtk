/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2024 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>
#include "gtktextencodingprivate.h"

typedef struct {
  const char *charset;
  const char *label;
} GtkTextEncodingItem;

/* Note: the charsets here must be valid iconv charset names */
static GtkTextEncodingItem gtk_text_encodings[] = {
  { "automatic",    NC_("Encoding name", "Automatically Detected") },
  { "ASCII",        NC_("Encoding name", "ASCII") },

  { "ISO-8859-1",   NC_("Encoding name", "Western") },
  { "ISO-8859-2",   NC_("Encoding name", "Central European") },
  { "ISO-8859-3",   NC_("Encoding name", "South European") },
  { "ISO-8859-4",   NC_("Encoding name", "Baltic") },
  { "ISO-8859-5",   NC_("Encoding name", "Cyrillic") },
  { "ISO-8859-6",   NC_("Encoding name", "Arabic") },
  { "ISO-8859-7",   NC_("Encoding name", "Greek") },
  { "ISO-8859-8",   NC_("Encoding name", "Hebrew Visual") },
  { "ISO-8859-9",   NC_("Encoding name", "Turkish") },
  { "ISO-8859-10",  NC_("Encoding name", "Nordic") },
  { "ISO-8859-11",  NC_("Encoding name", "Thai") },
  { "ISO-8859-13",  NC_("Encoding name", "Baltic") },
  { "ISO-8859-14",  NC_("Encoding name", "Celtic") },
  { "ISO-8859-15",  NC_("Encoding name", "Western") },
  { "ISO-8859-16",  NC_("Encoding name", "Romanian") },

  { "UTF-8",        NC_("Encoding name", "Unicode") },
  { "UTF-7",        NC_("Encoding name", "Unicode") },
  { "UTF-16",       NC_("Encoding name", "Unicode") },
  { "UTF-16BE",     NC_("Encoding name", "Unicode") },
  { "UTF-16LE",     NC_("Encoding name", "Unicode") },
  { "UTF-32",       NC_("Encoding name", "Unicode") },
  { "UCS-2",        NC_("Encoding name", "Unicode") },
  { "UCS-4",        NC_("Encoding name", "Unicode") },

  { "ARMSCII-8",    NC_("Encoding name", "Armenian") },
  { "BIG5",         NC_("Encoding name", "Chinese Traditional") },
  { "BIG5-HKSCS",   NC_("Encoding name", "Chinese Traditional") },
  { "CP866",        NC_("Encoding name", "Cyrillic/Russian") },
  { "EUC-JP",       NC_("Encoding name", "Japanese") },
  { "EUC-JP-MS",    NC_("Encoding name", "Japanese") },
  { "CP932",        NC_("Encoding name", "Japanese") },
  { "EUC-KR",       NC_("Encoding name", "Korean") },
  { "EUC-TW",       NC_("Encoding name", "Chinese Traditional") },

  { "GB18030",      NC_("Encoding name", "Chinese Simplified") },
  { "GB2312",       NC_("Encoding name", "Chinese Simplified") },
  { "GBK",          NC_("Encoding name", "Chinese Simplified") },
  { "GEORGIAN-ACADEMY", NC_("Encoding name", "Georgian") },

  { "IBM850",       NC_("Encoding name", "Western") },
  { "IBM852",       NC_("Encoding name", "Central European") },
  { "IBM855",       NC_("Encoding name", "Cyrillic") },
  { "IBM857",       NC_("Encoding name", "Turkish") },
  { "IBM862",       NC_("Encoding name", "Hebrew") },
  { "IBM864",       NC_("Encoding name", "Arabic") },

  { "ISO-2022-JP",  NC_("Encoding name", "Japanese") },
  { "ISO-2022-KR",  NC_("Encoding name", "Korean") },
  { "ISO-IR-111",   NC_("Encoding name", "Cyrillic") },
  { "JOHAB",        NC_("Encoding name", "Korean") },
  { "KOI8-R",       NC_("Encoding name", "Cyrillic") },
  { "KOI8-U",       NC_("Encoding name", "Cyrillic/Ukrainian") },
  { "SHIFT-JIS",    NC_("Encoding name", "Japanese") },
  { "TCVN",         NC_("Encoding name", "Vietnamese") },
  { "TIS-620",      NC_("Encoding name", "Thai") },
  { "UHC",          NC_("Encoding name", "Korean") },
  { "VISCII",       NC_("Encoding name", "Vietnamese") },

  { "WINDOWS-1250", NC_("Encoding name", "Central European") },
  { "WINDOWS-1251", NC_("Encoding name", "Cyrillic") },
  { "WINDOWS-1252", NC_("Encoding name", "Western") },
  { "WINDOWS-1253", NC_("Encoding name", "Greek") },
  { "WINDOWS-1254", NC_("Encoding name", "Turkish") },
  { "WINDOWS-1255", NC_("Encoding name", "Hebrew") },
  { "WINDOWS-1256", NC_("Encoding name", "Arabic") },
  { "WINDOWS-1257", NC_("Encoding name", "Baltic") },
  { "WINDOWS-1258", NC_("Encoding name", "Vietnamese") },
};

char **
gtk_text_encoding_get_names (void)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  char **res;

  for (guint i = 0; i < G_N_ELEMENTS (gtk_text_encodings); i++)
    {
      g_strv_builder_add (builder, gtk_text_encodings[i].charset);
    }

  res = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return res;
}

char **
gtk_text_encoding_get_labels (void)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  char **res;

  g_strv_builder_add (builder,
                      g_dpgettext2 (NULL, "Encoding name", gtk_text_encodings[0].label));

  for (guint i = 1; i < G_N_ELEMENTS (gtk_text_encodings); i++)
    {
      char *text;

      text = g_strdup_printf ("%s (%s)",
                              g_dpgettext2 (NULL, "Encoding name", gtk_text_encodings[i].label),
                              gtk_text_encodings[i].charset);
      g_strv_builder_take (builder, text);
    }

  res = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return res;
}

const char *
gtk_text_encoding_from_name (const char *name)
{
  for (guint i = 0; i < G_N_ELEMENTS (gtk_text_encodings); i++)
    {
      if (strcmp (name, gtk_text_encodings[i].charset) == 0)
        return gtk_text_encodings[i].charset;
    }

  return NULL;
}

typedef struct {
  const char *line_ending;
  const char *name;
  const char *label;
} GtkLineEndingItem;

static GtkLineEndingItem gtk_line_endings[] = {
  { "",     "as-is",   NC_("Line ending name", "Unchanged") },
  { "\n",   "unix",    NC_("Line ending name", "Unix/Linux") },
  { "\r\n", "windows", NC_("Line ending name", "Windows") },
  { "\r",   "mac",     NC_("Line ending name", "Mac OS Classic") },
};

char **
gtk_line_ending_get_names (void)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  char **res;

  for (guint i = 0; i < G_N_ELEMENTS (gtk_line_endings); i++)
    {
      g_strv_builder_add (builder, gtk_line_endings[i].name);
    }

  res = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return res;
}

char **
gtk_line_ending_get_labels (void)
{
  GStrvBuilder *builder = g_strv_builder_new ();
  char **res;

  for (guint i = 0; i < G_N_ELEMENTS (gtk_line_endings); i++)
    {
      g_strv_builder_add (builder,
                          g_dpgettext2 (NULL, "Line ending name", gtk_line_endings[i].label));
    }

  res = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);
  return res;
}

const char *
gtk_line_ending_from_name (const char *name)
{
  for (guint i = 0; i < G_N_ELEMENTS (gtk_line_endings); i++)
    {
      if (strcmp (name, gtk_line_endings[i].name) == 0)
        return gtk_line_endings[i].line_ending;
    }

  return NULL;
}
