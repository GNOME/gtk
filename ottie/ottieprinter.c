#include "ottieprinterprivate.h"

void
ottie_printer_init (OttiePrinter *printer)
{
  printer->str = g_string_new ("");
  printer->indent_level = 0;
  printer->has_member = FALSE;
}

#define INDENT 2

void
ottie_printer_indent (OttiePrinter *printer)
{
  if (printer->indent_level > 0)
    g_string_append_printf (printer->str, "%*s", printer->indent_level * INDENT, " ");
}

void
ottie_printer_start_object (OttiePrinter *printer,
                            const char   *name)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  if (name)
    g_string_append_printf (printer->str, "\"%s\" : ", name);
  g_string_append (printer->str, "{\n");
  printer->indent_level++;
  printer->has_member = FALSE;
}

void
ottie_printer_end_object (OttiePrinter *printer)
{
  printer->indent_level--;
  if (printer->has_member)
    g_string_append (printer->str, "\n");
  ottie_printer_indent (printer);
  g_string_append (printer->str, "}");
  printer->has_member = TRUE;
}

void
ottie_printer_start_array (OttiePrinter *printer,
                           const char   *name)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  if (name)
    g_string_append_printf (printer->str, "\"%s\" : ", name);
  g_string_append (printer->str, "[\n");
  printer->indent_level++;
  printer->has_member = FALSE;
}

void
ottie_printer_end_array (OttiePrinter *printer)
{
  printer->indent_level--;
  if (printer->has_member)
    g_string_append (printer->str, "\n");
  ottie_printer_indent (printer);
  g_string_append (printer->str, "]");
  printer->has_member = TRUE;
}

void
ottie_printer_add_double (OttiePrinter *printer,
                          const char   *name,
                          double        value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : %g", name, value);
  printer->has_member = TRUE;
}

void
ottie_printer_add_int (OttiePrinter *printer,
                       const char   *name,
                       int           value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : %d", name, value);
  printer->has_member = TRUE;
}

void
ottie_printer_add_boolean (OttiePrinter *printer,
                           const char   *name,
                           gboolean      value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : %s", name, value ? "true" : "false");
  printer->has_member = TRUE;
}

void
ottie_printer_add_string (OttiePrinter *printer,
                          const char   *name,
                          const char   *value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : \"%s\"", name, value);
  printer->has_member = TRUE;
}

void
ottie_printer_add_color (OttiePrinter  *printer,
                         const char    *name,
                         const GdkRGBA *value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : [ %g, %g, %g ]",
                          name, value->red, value->green, value->blue);
  printer->has_member = TRUE;
}

void
ottie_printer_add_point (OttiePrinter           *printer,
                         const char             *name,
                         const graphene_point_t *value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : [ %g, %g ]",
                          name, value->x, value->y);
  printer->has_member = TRUE;
}

void
ottie_printer_add_point3d (OttiePrinter             *printer,
                           const char               *name,
                           const graphene_point3d_t *value)
{
  if (printer->has_member)
    g_string_append (printer->str, ",\n");
  ottie_printer_indent (printer);
  g_string_append_printf (printer->str, "\"%s\" : [ %g, %g, %g ]",
                          name, value->x, value->y, value->z);
  printer->has_member = TRUE;
}
