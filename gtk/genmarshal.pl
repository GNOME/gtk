#!/usr/bin/perl
#
# by Elliot Lee <sopwith@redhat.com>

%trans = ( "NONE"=>"void", "CHAR"=>"char",
	   "BOOL"=>"gboolean", "INT"=>"gint",
	   "UINT"=>"guint", "LONG"=>"glong",
	   "ULONG"=>"gulong", "FLOAT"=>"gfloat",
	   "DOUBLE"=>"gdouble", "STRING"=>"gpointer",
	   "ENUM"=>"gint", "FLAGS"=>"gint",
	   "BOXED"=>"gpointer", "FOREIGN"=>"gpointer",
	   "CALLBACK"=>"gpointer", "POINTER"=>"gpointer",
	   "ARGS"=>"gpointer", "SIGNAL"=>"gpointer",
	   "C_CALLBACK"=>"gpointer");

open(IL, "<gtkmarshal.list") || die("Open failed: $!");
open(OH, ">gtkmarshal.h") || die("Open failed: $!");
open(OS, ">gtkmarshal.c") || die("Open failed: $!");

print OH "#ifndef __GTKMARSHAL_H__\n#define __GTKMARSHAL_H__ 1\n\n";
print OH "#include \"gtktypeutils.h\"\n#include \"gtkobject.h\"\n";

print OS "#include \"gtkmarshal.h\"\n";

while(chomp($aline = <IL>)) {
  ($retval, $paramlist) = split(/:/, $aline, 2);
  @params = split(/\s*,\s*/, $paramlist);

  if($defs{$retval."__".join("_",@params)} == 1) { next; }

  $doequiv = 0;
  foreach(@params) { if($trans{$_} eq "gpointer") { $doequiv = 1; } }

  $defname = "";
  if($doequiv) {
    $defname = $retval."__".join("_",@params);
    print OH "#define gtk_marshal_".$retval."__".join("_",@params)." ";

    for($i = 0; $i < scalar @params; $i++)
      { if($trans{$params[$i]} eq "gpointer") { $params[$i] = "POINTER"; } }
    if($trans{$retval} eq "gpointer") { $retval = "POINTER"; }
    print OH "gtk_marshal_".$retval."__".join("_",@params)."\n";

    $regname = $retval."__".join("_",@params);
    if($defs{$regname} == 1) { next; }
    $defs{$defname} = 1;
  }

  $defs{$retval."__".join("_",@params)} = 1;  

  print OH "void gtk_marshal_".$retval."__".join("_",@params)."(GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args);\n";

  print OS "typedef ".$trans{$retval}. " (*GtkSignal_"
    .$retval."__".join("_",@params).")(GtkObject *object, ";

  $argn = 1;
  foreach $it(@params) { print OS $trans{$it}." arg".$argn++.",\n"; }
  print OS "gpointer user_data);\n";

  print OS "void gtk_marshal_".$retval."__".join("_",@params)."(GtkObject *object, GtkSignalFunc func, gpointer func_data, GtkArg *args)\n";

  print OS "{\nGtkSignal_".$retval."__".join("_",@params)." rfunc;\n";
  if($retval ne "NONE") {
    print OS $trans{$retval}." *return_val;\n";

    print OS "return_val = GTK_RETLOC_".$retval."(args[".(scalar @params)."]);\n";
  }
  print OS "rfunc = (GtkSignal_".$retval."__".join("_",@params).") func;\n";

  if($retval ne "NONE") { print OS "*return_val = "; }

  print OS "(* rfunc)(object, ";
  for($i = 0; $i < (scalar @params); $i++) {
    print OS "GTK_VALUE_".$params[$i]."(args[$i]), ";
  }

  print OS "func_data);\n}\n\n";
}
print OH "#endif\n";
close(IL); close(OH); close(OS);
