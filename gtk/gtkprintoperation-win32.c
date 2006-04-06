/* EGG - The GIMP Toolkit
 * gtkprintoperation-win32.c: Print Operation Details for Win32
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

#ifndef _MSC_VER
#define _WIN32_WINNT 0x0500
#define WINVER _WIN32_WINNT
#endif

#include "config.h"
#include <stdlib.h>
#include <cairo-win32.h>
#include <glib.h>
#include "gtkprintoperation-private.h"
#include "gtkprint-win32.h"
#include "gtkintl.h"
#include "gtkinvisible.h"
#include "gtkalias.h"

#define MAX_PAGE_RANGES 20

typedef struct {
  HDC hdc;
  HGLOBAL devmode;
} GtkPrintOperationWin32;

static GtkPageOrientation
orientation_from_win32 (short orientation)
{
  if (orientation == DMORIENT_LANDSCAPE)
    return GTK_PAGE_ORIENTATION_LANDSCAPE;
  return GTK_PAGE_ORIENTATION_PORTRAIT;
}

static short
orientation_to_win32 (GtkPageOrientation orientation)
{
  if (orientation == GTK_PAGE_ORIENTATION_LANDSCAPE ||
      orientation == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE)
    return DMORIENT_LANDSCAPE;
  return DMORIENT_PORTRAIT;
}

static const char *
page_size_from_win32 (short size)
{
  switch (size)
    {
    case DMPAPER_LETTER_TRANSVERSE:
    case DMPAPER_LETTER:
    case DMPAPER_LETTERSMALL:
      return "na_letter";
    case DMPAPER_TABLOID:
    case DMPAPER_LEDGER:
      return "na_ledger";
    case DMPAPER_LEGAL:
      return "na_legal";
    case DMPAPER_STATEMENT:
      return "na_invoice";
    case DMPAPER_EXECUTIVE:
      return "na_executive";
    case DMPAPER_A3:
    case DMPAPER_A3_TRANSVERSE:
      return "iso_a3";
    case DMPAPER_A4:
    case DMPAPER_A4SMALL:
    case DMPAPER_A4_TRANSVERSE:
      return "iso_a4";
    case DMPAPER_A5:
    case DMPAPER_A5_TRANSVERSE:
      return "iso_a5";
    case DMPAPER_B4:
      return "iso_b4";
    case DMPAPER_B5:
    case DMPAPER_B5_TRANSVERSE:
      return "iso_b5";
    case DMPAPER_QUARTO:
      return "na_quarto";
    case DMPAPER_10X14:
      return "na_10x14";
    case DMPAPER_11X17:
      return "na_ledger";
    case DMPAPER_NOTE:
      return "na_letter";
    case DMPAPER_ENV_9:
      return "na_number-9";
    case DMPAPER_ENV_10:
      return "na_number-10";
    case DMPAPER_ENV_11:
      return "na_number-11";
    case DMPAPER_ENV_12:
      return "na_number-12";
    case DMPAPER_ENV_14:
      return "na_number-14";
    case DMPAPER_CSHEET:
      return "na_c";
    case DMPAPER_DSHEET:
      return "na_d";
    case DMPAPER_ESHEET:
      return "na_e";
    case DMPAPER_ENV_DL:
      return "iso_dl";
    case DMPAPER_ENV_C5:
      return "iso_c5";
    case DMPAPER_ENV_C3:
      return "iso_c3";
    case DMPAPER_ENV_C4:
      return "iso_c4";
    case DMPAPER_ENV_C6:
      return "iso_c6";
    case DMPAPER_ENV_C65:
      return "iso_c6c5";
    case DMPAPER_ENV_B4:
      return "iso_b4";
    case DMPAPER_ENV_B5:
      return "iso_b5";
    case DMPAPER_ENV_B6:
      return "iso_b6";
    case DMPAPER_ENV_ITALY:
      return "om_italian";
    case DMPAPER_ENV_MONARCH:
      return "na_monarch";
    case DMPAPER_ENV_PERSONAL:
      return "na_personal";
    case DMPAPER_FANFOLD_US:
      return "na_fanfold-us";
    case DMPAPER_FANFOLD_STD_GERMAN:
      return "na_fanfold-eur";
    case DMPAPER_FANFOLD_LGL_GERMAN:
      return "na_foolscap";
    case DMPAPER_ISO_B4:
      return "iso_b4";
    case DMPAPER_JAPANESE_POSTCARD:
      return "jpn_hagaki";
    case DMPAPER_9X11:
      return "na_9x11";
    case DMPAPER_10X11:
      return "na_10x11";
    case DMPAPER_ENV_INVITE:
      return "om_invite";
    case DMPAPER_LETTER_EXTRA:
    case DMPAPER_LETTER_EXTRA_TRANSVERSE:
      return "na_letter-extra";
    case DMPAPER_LEGAL_EXTRA:
      return "na_legal-extra";
    case DMPAPER_TABLOID_EXTRA:
      return "na_arch";
    case DMPAPER_A4_EXTRA:
      return "iso_a4-extra";
    case DMPAPER_B_PLUS:
      return "na_b-plus";
    case DMPAPER_LETTER_PLUS:
      return "na_letter-plus";
    case DMPAPER_A3_EXTRA:
    case DMPAPER_A3_EXTRA_TRANSVERSE:
      return "iso_a3-extra";
    case DMPAPER_A5_EXTRA:
      return "iso_a5-extra";
    case DMPAPER_B5_EXTRA:
      return "iso_b5-extra";
    case DMPAPER_A2:
      return "iso_a2";
      
    default:
      /* Dunno what these are: */
    case DMPAPER_A4_PLUS:
    case DMPAPER_A_PLUS:
    case DMPAPER_FOLIO:
    case DMPAPER_15X11:
      return "iso_a4";
    }
}

static short
paper_size_to_win32 (GtkPaperSize *paper_size)
{
  const char *format;

  if (gtk_paper_size_is_custom (paper_size))
    return 0;
  
  format = gtk_paper_size_get_name (paper_size);

  if (strcmp (format, "na_letter") == 0)
    return DMPAPER_LETTER;
  if (strcmp (format, "na_ledger") == 0)
    return DMPAPER_LEDGER;
  if (strcmp (format, "na_legal") == 0)
    return DMPAPER_LEGAL;
  if (strcmp (format, "na_invoice") == 0)
    return DMPAPER_STATEMENT;
  if (strcmp (format, "na_executive") == 0)
    return DMPAPER_EXECUTIVE;
  if (strcmp (format, "iso_a2") == 0)
    return DMPAPER_A2;
  if (strcmp (format, "iso_a3") == 0)
    return DMPAPER_A3;
  if (strcmp (format, "iso_a4") == 0)
    return DMPAPER_A4;
  if (strcmp (format, "iso_a5") == 0)
    return DMPAPER_A5;
  if (strcmp (format, "iso_b4") == 0)
    return DMPAPER_B4;
  if (strcmp (format, "iso_b5") == 0)
    return DMPAPER_B5;
  if (strcmp (format, "na_quarto") == 0)
    return DMPAPER_QUARTO;
  if (strcmp (format, "na_10x14") == 0)
    return DMPAPER_10X14;
  if (strcmp (format, "na_number-9") == 0)
    return DMPAPER_ENV_9;
  if (strcmp (format, "na_number-10") == 0)
    return DMPAPER_ENV_10;
  if (strcmp (format, "na_number-11") == 0)
    return DMPAPER_ENV_11;
  if (strcmp (format, "na_number-12") == 0)
    return DMPAPER_ENV_12;
  if (strcmp (format, "na_number-14") == 0)
    return DMPAPER_ENV_14;
  if (strcmp (format, "na_c") == 0)
    return DMPAPER_CSHEET;
  if (strcmp (format, "na_d") == 0)
    return DMPAPER_DSHEET;
  if (strcmp (format, "na_e") == 0)
    return DMPAPER_ESHEET;
  if (strcmp (format, "iso_dl") == 0)
    return DMPAPER_ENV_DL;
  if (strcmp (format, "iso_c3") == 0)
    return DMPAPER_ENV_C3;
  if (strcmp (format, "iso_c4") == 0)
    return DMPAPER_ENV_C4;
  if (strcmp (format, "iso_c5") == 0)
    return DMPAPER_ENV_C5;
  if (strcmp (format, "iso_c6") == 0)
    return DMPAPER_ENV_C6;
  if (strcmp (format, "iso_c5c6") == 0)
    return DMPAPER_ENV_C65;
  if (strcmp (format, "iso_b6") == 0)
    return DMPAPER_ENV_B6;
  if (strcmp (format, "om_italian") == 0)
    return DMPAPER_ENV_ITALY;
  if (strcmp (format, "na_monarch") == 0)
    return DMPAPER_ENV_MONARCH;
  if (strcmp (format, "na_personal") == 0)
    return DMPAPER_ENV_PERSONAL;
  if (strcmp (format, "na_fanfold-us") == 0)
    return DMPAPER_FANFOLD_US;
  if (strcmp (format, "na_fanfold-eur") == 0)
    return DMPAPER_FANFOLD_STD_GERMAN;
  if (strcmp (format, "na_foolscap") == 0)
    return DMPAPER_FANFOLD_LGL_GERMAN;
  if (strcmp (format, "jpn_hagaki") == 0)
    return DMPAPER_JAPANESE_POSTCARD;
  if (strcmp (format, "na_9x11") == 0)
    return DMPAPER_9X11;
  if (strcmp (format, "na_10x11") == 0)
    return DMPAPER_10X11;
  if (strcmp (format, "om_invite") == 0)
    return DMPAPER_ENV_INVITE;
  if (strcmp (format, "na_letter-extra") == 0)
    return DMPAPER_LETTER_EXTRA;
  if (strcmp (format, "na_legal-extra") == 0)
    return DMPAPER_LEGAL_EXTRA;
  if (strcmp (format, "na_arch") == 0)
    return DMPAPER_TABLOID_EXTRA;
  if (strcmp (format, "iso_a3-extra") == 0)
    return DMPAPER_A3_EXTRA;
  if (strcmp (format, "iso_a4-extra") == 0)
    return DMPAPER_A4_EXTRA;
  if (strcmp (format, "iso_a5-extra") == 0)
    return DMPAPER_A5_EXTRA;
  if (strcmp (format, "iso_b5-extra") == 0)
    return DMPAPER_B5_EXTRA;
  if (strcmp (format, "na_b-plus") == 0)
    return DMPAPER_B_PLUS;
  if (strcmp (format, "na_letter-plus") == 0)
    return DMPAPER_LETTER_PLUS;

  return 0;
}

void
win32_start_page (GtkPrintOperation *op,
		  GtkPrintContext *print_context,
		  GtkPageSetup *page_setup)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  LPDEVMODEW devmode;
  GtkPaperSize *paper_size;
  
  devmode = GlobalLock (op_win32->devmode);
  
  devmode->dmFields |= DM_ORIENTATION;
  devmode->dmOrientation =
    orientation_to_win32 (gtk_page_setup_get_orientation (page_setup));
  
  paper_size = gtk_page_setup_get_paper_size (page_setup);
  devmode->dmFields |= DM_PAPERSIZE;
  devmode->dmFields &= ~(DM_PAPERWIDTH | DM_PAPERLENGTH);
  devmode->dmPaperSize = paper_size_to_win32 (paper_size);
  if (devmode->dmPaperSize == 0)
    {
      devmode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
      devmode->dmPaperWidth = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM) * 10.0;
      devmode->dmPaperLength = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM) * 10.0;
    }
  
  ResetDCW (op_win32->hdc, devmode);
  
  GlobalUnlock (op_win32->devmode);
  
  StartPage (op_win32->hdc);
}

static void
win32_end_page (GtkPrintOperation *op,
		GtkPrintContext *print_context)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;
  EndPage (op_win32->hdc);
}

static void
win32_end_run (GtkPrintOperation *op)
{
  GtkPrintOperationWin32 *op_win32 = op->priv->platform_data;

  EndDoc (op_win32->hdc);
  GlobalFree(op_win32->devmode);

  cairo_surface_destroy (op->priv->surface);
  op->priv->surface = NULL;

  DeleteDC(op_win32->hdc);
  
  g_free (op_win32);
}

static HWND
get_parent_hwnd (GtkWidget *widget)
{
  gtk_widget_realize (widget);
  return gdk_win32_drawable_get_handle (widget->window);
}

static void
dialog_to_print_settings (GtkPrintOperation *op,
			  LPPRINTDLGEXW printdlgex)
{
  int i;
  LPDEVMODEW devmode;
  GtkPrintSettings *settings;

  settings = gtk_print_settings_new ();

  gtk_print_settings_set_print_pages (settings,
				      GTK_PRINT_PAGES_ALL);
  if (printdlgex->Flags & PD_CURRENTPAGE)
    gtk_print_settings_set_print_pages (settings,
					GTK_PRINT_PAGES_CURRENT);
  else if (printdlgex->Flags & PD_PAGENUMS)
    gtk_print_settings_set_print_pages (settings,
					GTK_PRINT_PAGES_RANGES);

  if (printdlgex->nPageRanges > 0)
    {
      GtkPageRange *ranges;
      ranges = g_new (GtkPageRange, printdlgex->nPageRanges);

      for (i = 0; i < printdlgex->nPageRanges; i++)
	{
	  ranges[i].start = printdlgex->lpPageRanges[i].nFromPage - 1;
	  ranges[i].end = printdlgex->lpPageRanges[i].nToPage - 1;
	}

      gtk_print_settings_set_page_ranges (settings, ranges,
					  printdlgex->nPageRanges);
      g_free (ranges);
    }
  
  if (printdlgex->hDevNames != NULL) 
    {
      GtkPrintWin32Devnames *devnames = gtk_print_win32_devnames_from_win32 (printdlgex->hDevNames);
      gtk_print_settings_set_printer (settings,
				      devnames->device);
      gtk_print_win32_devnames_free (devnames);
    }

  if (printdlgex->hDevMode != NULL) 
    {
      devmode = GlobalLock (printdlgex->hDevMode);

      gtk_print_settings_set_int (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION,
				  devmode->dmDriverVersion);
      if (devmode->dmDriverExtra != 0)
	{
	  char *extra = g_base64_encode (((char *)devmode) + sizeof (DEVMODEW),
					 devmode->dmDriverExtra);
	  gtk_print_settings_set (settings,
				  GTK_PRINT_SETTINGS_WIN32_DRIVER_EXTRA,
				  extra);
	  g_free (extra);
	}
      
      if (devmode->dmFields & DM_ORIENTATION)
	gtk_print_settings_set_orientation (settings,
					    orientation_from_win32 (devmode->dmOrientation));


      if (devmode->dmFields & DM_PAPERSIZE)
	{
	  if (devmode->dmPaperSize != 0)
	    {
	      GtkPaperSize *paper_size = gtk_paper_size_new (page_size_from_win32 (devmode->dmPaperSize));
	      gtk_print_settings_set_paper_size (settings, paper_size);
	      gtk_paper_size_free (paper_size);
	    }
	  else
	    {
	      GtkPaperSize *paper_size = gtk_paper_size_new_custom ("dialog", _("Custom paper"),
								    devmode->dmPaperWidth * 10.0,
								    devmode->dmPaperLength * 10.0,
								    GTK_UNIT_MM);
	      gtk_print_settings_set_paper_size (settings, paper_size);
	      gtk_paper_size_free (paper_size);
	    }
	}

      if (devmode->dmFields & DM_SCALE)
	gtk_print_settings_set_scale (settings,
				      devmode->dmScale / 100.0);

      if (devmode->dmFields & DM_COPIES)
	gtk_print_settings_set_num_copies (settings,
					   devmode->dmCopies);
      
      if (devmode->dmFields & DM_DEFAULTSOURCE)
	{
	  char *source;
	  switch (devmode->dmDefaultSource)
	    {
	    default:
	    case DMBIN_AUTO:
	      source = "auto";
	      break;
	    case DMBIN_CASSETTE:
	      source = "cassette";
	      break;
	    case DMBIN_ENVELOPE:
	      source = "envelope";
	      break;
	    case DMBIN_ENVMANUAL:
	      source = "envelope-manual";
	      break;
	    case DMBIN_LOWER:
	      source = "lower";
	      break;
	    case DMBIN_MANUAL:
	      source = "manual";
	      break;
	    case DMBIN_MIDDLE:
	      source = "middle";
	      break;
	    case DMBIN_ONLYONE:
	      source = "only-one";
	      break;
	    case DMBIN_FORMSOURCE:
	      source = "form-source";
	      break;
	    case DMBIN_LARGECAPACITY:
	      source = "large-capacity";
	      break;
	    case DMBIN_LARGEFMT:
	      source = "large-format";
	      break;
	    case DMBIN_TRACTOR:
	      source = "tractor";
	      break;
	    case DMBIN_SMALLFMT:
	      source = "small-format";
	      break;
	    }
	  gtk_print_settings_set_default_source (settings, source);
	}

      if (devmode->dmFields & DM_PRINTQUALITY)
	{
	  GtkPrintQuality quality;
	  switch (devmode->dmPrintQuality)
	    {
	    case DMRES_LOW:
	      quality = GTK_PRINT_QUALITY_LOW;
	      break;
	    case DMRES_MEDIUM:
	      quality = GTK_PRINT_QUALITY_NORMAL;
	      break;
	    default:
	    case DMRES_HIGH:
	      quality = GTK_PRINT_QUALITY_HIGH;
	      break;
	    case DMRES_DRAFT:
	      quality = GTK_PRINT_QUALITY_DRAFT;
	      break;
	    }
	  gtk_print_settings_set_quality (settings, quality);
	}
      
      if (devmode->dmFields & DM_COLOR)
	gtk_print_settings_set_use_color (settings, devmode->dmFields == DMCOLOR_COLOR);

      if (devmode->dmFields & DM_DUPLEX)
	{
	  GtkPrintDuplex duplex;
	  switch (duplex)
	    {
	    default:
	    case DMDUP_SIMPLEX:
	      duplex = GTK_PRINT_DUPLEX_SIMPLEX;
	      break;
	    case DMDUP_HORIZONTAL:
	      duplex = GTK_PRINT_DUPLEX_HORIZONTAL;
	      break;
	    case DMDUP_VERTICAL:
	      duplex = GTK_PRINT_DUPLEX_VERTICAL;
	      break;
	    }
	  
	  gtk_print_settings_set_duplex (settings, duplex);
	}
      
      if (devmode->dmFields & DM_COLLATE)
	gtk_print_settings_set_collate (settings,
					devmode->dmCollate == DMCOLLATE_TRUE);

      if (devmode->dmFields & DM_MEDIATYPE)
	{
	  char *media_type;
	  switch (devmode->dmMediaType)
	    {
	    default:
	    case DMMEDIA_STANDARD:
	      media_type = "stationery";
	      break;
	    case DMMEDIA_TRANSPARENCY:
	      media_type = "transparency";
	      break;
	    case DMMEDIA_GLOSSY:
	      media_type = "photographic-glossy";
	      break;
	    }
	  gtk_print_settings_set_media_type (settings, media_type);
	}

      if (devmode->dmFields & DM_DITHERTYPE)
	{
	  char *dither;
	  switch (devmode->dmDitherType)
	    {
	    default:
	    case DMDITHER_FINE:
	      dither = "fine";
	      break;
	    case DMDITHER_NONE:
	      dither = "none";
	      break;
	    case DMDITHER_COARSE:
	      dither = "coarse";
	      break;
	    case DMDITHER_LINEART:
	      dither = "lineart";
	      break;
	    case DMDITHER_GRAYSCALE:
	      dither = "grayscale";
	      break;
	    case DMDITHER_ERRORDIFFUSION:
	      dither = "error-diffusion";
	      break;
	    }
	  gtk_print_settings_set_dither (settings, dither);
	}
      	  
      GlobalUnlock (printdlgex->hDevMode);
    }
  
  gtk_print_operation_set_print_settings (op, settings);
}

static void
dialog_from_print_settings (GtkPrintOperation *op,
			    LPPRINTDLGEXW printdlgex)
{
  GtkPrintSettings *settings = op->priv->print_settings;
  const char *printer;
  const char *extras_base64;
  const char *val;
  GtkPaperSize *paper_size;
  char *extras;
  int extras_len;
  
  LPDEVMODEW devmode;

  if (settings == NULL)
    return;

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_PRINT_PAGES))
    {
      GtkPrintPages print_pages = gtk_print_settings_get_print_pages (settings);

      switch (print_pages)
	{
	default:
	case GTK_PRINT_PAGES_ALL:
	  printdlgex->Flags |= PD_ALLPAGES;
	  break;
	case GTK_PRINT_PAGES_CURRENT:
	  printdlgex->Flags |= PD_CURRENTPAGE;
	  break;
	case GTK_PRINT_PAGES_RANGES:
	  printdlgex->Flags |= PD_PAGENUMS;
	  break;
	}
    }
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_PAGE_RANGES))
    {
      GtkPageRange *ranges;
      int num_ranges, i;

      ranges = gtk_print_settings_get_page_ranges (settings, &num_ranges);

      if (num_ranges > MAX_PAGE_RANGES)
	num_ranges = MAX_PAGE_RANGES;

      printdlgex->nPageRanges = num_ranges;
      for (i = 0; i < num_ranges; i++)
	{
	  printdlgex->lpPageRanges[i].nFromPage = ranges[i].start + 1;
	  printdlgex->lpPageRanges[i].nToPage = ranges[i].end + 1;
	}
    }
  
  printer = gtk_print_settings_get_printer (settings);
  if (printer)
    printdlgex->hDevNames = gtk_print_win32_devnames_from_printer_name (printer);

  extras = NULL;
  extras_len = 0;
  extras_base64 = gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_EXTRA);
  if (extras_base64)
    extras = g_base64_decode (extras_base64, &extras_len);

  printdlgex->hDevMode = GlobalAlloc (GMEM_MOVEABLE, 
				      sizeof (DEVMODEW) + extras_len);

  devmode = GlobalLock (printdlgex->hDevMode);

  memset (devmode, 0, sizeof (DEVMODEW));
  
  devmode->dmSpecVersion = DM_SPECVERSION;
  devmode->dmSize = sizeof (DEVMODEW);
  
  devmode->dmDriverExtra = 0;
  if (extras && extras_len > 0)
    {
      devmode->dmDriverExtra = extras_len;
      memcpy (((char *)devmode) + sizeof (DEVMODEW), extras, extras_len);
      g_free (extras);
    }
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION))
    devmode->dmDriverVersion = gtk_print_settings_get_int (settings, GTK_PRINT_SETTINGS_WIN32_DRIVER_VERSION);

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_ORIENTATION))
    {
      devmode->dmFields |= DM_ORIENTATION;
      devmode->dmOrientation =
	orientation_to_win32 (gtk_print_settings_get_orientation (settings));
    }

  paper_size = gtk_print_settings_get_paper_size (settings);
  if (paper_size)
    {
      devmode->dmFields |= DM_PAPERSIZE;
      devmode->dmPaperSize = paper_size_to_win32 (paper_size);
      if (devmode->dmPaperSize == 0)
	{
	  devmode->dmFields |= DM_PAPERWIDTH | DM_PAPERLENGTH;
	  devmode->dmPaperWidth = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM) * 10.0;
	  devmode->dmPaperLength = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM) * 10.0;
	}
      gtk_paper_size_free (paper_size);
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_SCALE))
    {
      devmode->dmFields |= DM_SCALE;
      devmode->dmScale = gtk_print_settings_get_scale (settings) * 100;
    }
  
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_NUM_COPIES))
    {
      devmode->dmFields |= DM_COPIES;
      devmode->dmCopies = gtk_print_settings_get_num_copies (settings);
    }
  
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DEFAULT_SOURCE))
    {
      devmode->dmFields |= DM_DEFAULTSOURCE;
      devmode->dmDefaultSource = DMBIN_AUTO;

      val = gtk_print_settings_get_default_source (settings);
      if (strcmp (val, "auto") == 0)
	devmode->dmDefaultSource = DMBIN_AUTO;
      if (strcmp (val, "cassette") == 0)
	devmode->dmDefaultSource = DMBIN_CASSETTE;
      if (strcmp (val, "envelope") == 0)
	devmode->dmDefaultSource = DMBIN_ENVELOPE;
      if (strcmp (val, "envelope-manual") == 0)
	devmode->dmDefaultSource = DMBIN_ENVMANUAL;
      if (strcmp (val, "lower") == 0)
	devmode->dmDefaultSource = DMBIN_LOWER;
      if (strcmp (val, "manual") == 0)
	devmode->dmDefaultSource = DMBIN_MANUAL;
      if (strcmp (val, "middle") == 0)
	devmode->dmDefaultSource = DMBIN_MIDDLE;
      if (strcmp (val, "only-one") == 0)
	devmode->dmDefaultSource = DMBIN_ONLYONE;
      if (strcmp (val, "form-source") == 0)
	devmode->dmDefaultSource = DMBIN_FORMSOURCE;
      if (strcmp (val, "large-capacity") == 0)
	devmode->dmDefaultSource = DMBIN_LARGECAPACITY;
      if (strcmp (val, "large-format") == 0)
	devmode->dmDefaultSource = DMBIN_LARGEFMT;
      if (strcmp (val, "tractor") == 0)
	devmode->dmDefaultSource = DMBIN_TRACTOR;
      if (strcmp (val, "small-format") == 0)
	devmode->dmDefaultSource = DMBIN_SMALLFMT;
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_RESOLUTION))
    {
      devmode->dmFields |= DM_PRINTQUALITY;
      devmode->dmPrintQuality = gtk_print_settings_get_resolution (settings);
    } 
  else if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_QUALITY))
    {
      devmode->dmFields |= DM_PRINTQUALITY;
      switch (gtk_print_settings_get_quality (settings))
	{
	case GTK_PRINT_QUALITY_LOW:
	  devmode->dmPrintQuality = DMRES_LOW;
	  break;
	case GTK_PRINT_QUALITY_DRAFT:
	  devmode->dmPrintQuality = DMRES_DRAFT;
	  break;
	default:
	case GTK_PRINT_QUALITY_NORMAL:
	  devmode->dmPrintQuality = DMRES_MEDIUM;
	  break;
	case GTK_PRINT_QUALITY_HIGH:
	  devmode->dmPrintQuality = DMRES_HIGH;
	  break;
	}
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_USE_COLOR))
    {
      devmode->dmFields |= DM_COLOR;
      if (gtk_print_settings_get_use_color (settings))
	devmode->dmColor = DMCOLOR_COLOR;
      else
	devmode->dmColor = DMCOLOR_MONOCHROME;
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DUPLEX))
    {
      devmode->dmFields |= DM_DUPLEX;
      switch (gtk_print_settings_get_duplex (settings))
	{
	default:
	case GTK_PRINT_DUPLEX_SIMPLEX:
	  devmode->dmDuplex = DMDUP_SIMPLEX;
	  break;
	case GTK_PRINT_DUPLEX_HORIZONTAL:
	  devmode->dmDuplex = DMDUP_HORIZONTAL;
	  break;
	case GTK_PRINT_DUPLEX_VERTICAL:
	  devmode->dmDuplex = DMDUP_VERTICAL;
	  break;
	}
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_COLLATE))
    {
      devmode->dmFields |= DM_COLLATE;
      if (gtk_print_settings_get_collate (settings))
	devmode->dmCollate = DMCOLLATE_TRUE;
      else
	devmode->dmCollate = DMCOLLATE_FALSE;
    }

  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_MEDIA_TYPE))
    {
      devmode->dmFields |= DM_MEDIATYPE;
      devmode->dmMediaType = DMMEDIA_STANDARD;
      
      val = gtk_print_settings_get_media_type (settings);
      if (strcmp (val, "transparency") == 0)
	devmode->dmMediaType = DMMEDIA_TRANSPARENCY;
      if (strcmp (val, "photographic-glossy") == 0)
	devmode->dmMediaType = DMMEDIA_GLOSSY;
    }
 
  if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_DITHER))
    {
      devmode->dmFields |= DM_DITHERTYPE;
      devmode->dmDitherType = DMDITHER_FINE;
      
      val = gtk_print_settings_get_dither (settings);
      if (strcmp (val, "none") == 0)
	devmode->dmDitherType = DMDITHER_NONE;
      if (strcmp (val, "coarse") == 0)
	devmode->dmDitherType = DMDITHER_COARSE;
      if (strcmp (val, "fine") == 0)
	devmode->dmDitherType = DMDITHER_FINE;
      if (strcmp (val, "lineart") == 0)
	devmode->dmDitherType = DMDITHER_LINEART;
      if (strcmp (val, "grayscale") == 0)
	devmode->dmDitherType = DMDITHER_GRAYSCALE;
      if (strcmp (val, "error-diffusion") == 0)
	devmode->dmDitherType = DMDITHER_ERRORDIFFUSION;
    }
  
  GlobalUnlock (printdlgex->hDevMode);

}

GtkPrintOperationResult
_gtk_print_operation_platform_backend_run_dialog (GtkPrintOperation *op,
						  GtkWindow *parent,
						  gboolean *do_print,
						  GError **error)
{
  HRESULT hResult;
  LPPRINTDLGEXW printdlgex = NULL;
  LPPRINTPAGERANGE page_ranges = NULL;
  HWND parentHWnd;
  GtkWidget *invisible = NULL;
  GtkPrintOperationResult result;
  GtkPrintOperationWin32 *op_win32;
  
  *do_print = FALSE;

  if (parent == NULL)
    {
      invisible = gtk_invisible_new ();
      parentHWnd = get_parent_hwnd (invisible);
    }
  else 
    parentHWnd = get_parent_hwnd (GTK_WIDGET (parent));

  printdlgex = (LPPRINTDLGEXW)GlobalAlloc (GPTR, sizeof (PRINTDLGEXW));
  if (!printdlgex)
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error (error,
		   GTK_PRINT_ERROR,
		   GTK_PRINT_ERROR_NOMEM,
		   _("Not enough free memory"));
      goto out;
    }      

  page_ranges = (LPPRINTPAGERANGE) GlobalAlloc (GPTR, 
						MAX_PAGE_RANGES * sizeof (PRINTPAGERANGE));
  if (!page_ranges) 
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      g_set_error (error,
		   GTK_PRINT_ERROR,
		   GTK_PRINT_ERROR_NOMEM,
		   _("Not enough free memory"));
      goto out;
    }

  printdlgex->lStructSize = sizeof(PRINTDLGEX);
  printdlgex->hwndOwner = parentHWnd;
  printdlgex->hDevMode = NULL;
  printdlgex->hDevNames = NULL;
  printdlgex->hDC = NULL;
  printdlgex->Flags = PD_RETURNDC | PD_NOSELECTION;
  if (op->priv->current_page == -1)
    printdlgex->Flags |= PD_NOCURRENTPAGE;
  printdlgex->Flags2 = 0;
  printdlgex->ExclusionFlags = 0;
  printdlgex->nPageRanges = 0;
  printdlgex->nMaxPageRanges = MAX_PAGE_RANGES;
  printdlgex->lpPageRanges = page_ranges;
  printdlgex->nMinPage = 1;
  if (op->priv->nr_of_pages != -1)
    printdlgex->nMaxPage = op->priv->nr_of_pages;
  else
    printdlgex->nMaxPage = 10000;
  printdlgex->nCopies = 1;
  printdlgex->hInstance = 0;
  printdlgex->lpPrintTemplateName = NULL;
  printdlgex->lpCallback = NULL;
  printdlgex->nPropertyPages = 0;
  printdlgex->lphPropertyPages = NULL;
  printdlgex->nStartPage = START_PAGE_GENERAL;
  printdlgex->dwResultAction = 0;

  dialog_from_print_settings (op, printdlgex);

  /* TODO: We should do this in a thread to avoid blocking the mainloop */
  hResult = PrintDlgExW(printdlgex);

  if (hResult != S_OK) 
    {
      result = GTK_PRINT_OPERATION_RESULT_ERROR;
      if (hResult == E_OUTOFMEMORY)
	g_set_error (error,
		     GTK_PRINT_ERROR,
		     GTK_PRINT_ERROR_NOMEM,
		     _("Not enough free memory"));
      else if (hResult == E_INVALIDARG)
	g_set_error (error,
		     GTK_PRINT_ERROR,
		     GTK_PRINT_ERROR_INTERNAL_ERROR,
		     _("Invalid argument to PrintDlgEx"));
      else if (hResult == E_POINTER)
	g_set_error (error,
		     GTK_PRINT_ERROR,
		     GTK_PRINT_ERROR_INTERNAL_ERROR,
		     _("Invalid pointer to PrintDlgEx"));
      else if (hResult == E_HANDLE)
	g_set_error (error,
		     GTK_PRINT_ERROR,
		     GTK_PRINT_ERROR_INTERNAL_ERROR,
		     _("Invalid handle to PrintDlgEx"));
      else /* E_FAIL */
	g_set_error (error,
		     GTK_PRINT_ERROR,
		     GTK_PRINT_ERROR_GENERAL,
		     _("Unspecified error"));
      goto out;
    }

  if (printdlgex->dwResultAction == PD_RESULT_PRINT ||
      printdlgex->dwResultAction == PD_RESULT_APPLY)
    {
      result = GTK_PRINT_OPERATION_RESULT_APPLY;
      dialog_to_print_settings (op, printdlgex);
    }
  else
    result = GTK_PRINT_OPERATION_RESULT_CANCEL;
  
  if (printdlgex->dwResultAction == PD_RESULT_PRINT)
    {
      DOCINFOW docinfo;
      int job_id;

      *do_print = TRUE;

      op->priv->surface = cairo_win32_surface_create (printdlgex->hDC);
      op->priv->dpi_x = (double)GetDeviceCaps (printdlgex->hDC, LOGPIXELSX);
      op->priv->dpi_y = (double)GetDeviceCaps (printdlgex->hDC, LOGPIXELSY);

      memset( &docinfo, 0, sizeof (DOCINFOW));
      docinfo.cbSize = sizeof (DOCINFOW); 
      docinfo.lpszDocName = g_utf8_to_utf16 (op->priv->job_name, -1, NULL, NULL, NULL); 
      docinfo.lpszOutput = (LPCWSTR) NULL; 
      docinfo.lpszDatatype = (LPCWSTR) NULL; 
      docinfo.fwType = 0; 

      job_id = StartDocW(printdlgex->hDC, &docinfo); 
      g_free ((void *)docinfo.lpszDocName);
      if (job_id <= 0) 
	{ 
	  result = GTK_PRINT_OPERATION_RESULT_ERROR;
	  g_set_error (error,
		       GTK_PRINT_ERROR,
		       GTK_PRINT_ERROR_GENERAL,
		     _("Error from StartDoc"));
	  *do_print = FALSE;
	  cairo_surface_destroy (op->priv->surface);
	  op->priv->surface = NULL;
	  goto out; 
	} 

      op_win32 = g_new (GtkPrintOperationWin32, 1);
      op->priv->platform_data = op_win32;
      op_win32->hdc = printdlgex->hDC;
      op_win32->devmode = printdlgex->hDevMode;

      op->priv->print_pages = gtk_print_settings_get_print_pages (op->priv->print_settings);
      op->priv->num_page_ranges = 0;
      if (op->priv->print_pages == GTK_PRINT_PAGES_RANGES)
	op->priv->page_ranges = gtk_print_settings_get_page_ranges (op->priv->print_settings,
								    &op->priv->num_page_ranges);
      op->priv->manual_num_copies = printdlgex->nCopies;
      op->priv->manual_collation = (printdlgex->Flags & PD_COLLATE) != 0;
      op->priv->manual_reverse = FALSE;
      op->priv->manual_orientation = FALSE;
      op->priv->manual_scale = 1.0;
      op->priv->manual_page_set = GTK_PAGE_SET_ALL;
    }

  op->priv->start_page = win32_start_page;
  op->priv->end_page = win32_end_page;
  op->priv->end_run = win32_end_run;
  
  out:
  if (!result && printdlgex && printdlgex->hDevMode != NULL) 
    GlobalFree(printdlgex->hDevMode); 

  if (printdlgex && printdlgex->hDevNames != NULL) 
    GlobalFree(printdlgex->hDevNames); 

  if (page_ranges)
    GlobalFree (page_ranges);

  if (printdlgex)
    GlobalFree (printdlgex);

  if (invisible)
    gtk_widget_destroy (invisible);

  return result;
}

GtkPageSetup *
gtk_print_run_page_setup_dialog (GtkWindow        *parent,
				 GtkPageSetup     *page_setup,
				 GtkPrintSettings *settings)
{
  return NULL;
}
