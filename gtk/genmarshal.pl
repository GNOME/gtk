#!/usr/bin/perl -w
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

open(IL, "<".$ENV{'srcdir'}."/gtkmarshal.list") || die("Open failed: $!");
open(OH, "|indent > gtkmarshal.h") || die("Open failed: $!");
open(OS, "|indent > gtkmarshal.c") || die("Open failed: $!");

print OH <<EOT;
#ifndef __GTKMARSHAL_H__
#define __GTKMARSHAL_H__ 1

#include "gtktypeutils.h"
#include "gtkobject.h"

EOT

print OS qq(#include "gtkmarshal.h"\n\n);

while(chomp($aline = <IL>)) {
  ($retval, $paramlist) = split(/:/, $aline, 2);
  @params = split(/\s*,\s*/, $paramlist);

  my $funcname = $retval."__".join("_",@params);
  
  next if (exists $defs{$funcname});

  $doequiv = 0;
  for (@params, $retval) { 
      if ($trans{$_} eq "gpointer") { 
	  $doequiv = 1;
	  last;
      } 
  }

  # Translate all function pointers to gpointer
  $defname = "";
  if($doequiv) {
      print OH "#define gtk_marshal_$funcname ";
      $defs{$defname} = 1;
      
      for (@params, $retval) {
	  if ($trans{$_} eq "gpointer") {
	      $_ = "POINTER";
	  }
      }

      $funcname = $retval."__".join("_",@params);

      print OH "gtk_marshal_$funcname\n";
      next if (exists $defs{$funcname});
  }
  $defs{$funcname} = 1;  

  print OH <<EOT;
void gtk_marshal_$funcname (GtkObject    *object, 
                            GtkSignalFunc func, 
                            gpointer      func_data, 
                            GtkArg       *args);

EOT
  
  print OS "typedef $trans{$retval} (*GtkSignal_$funcname) (GtkObject *object, \n";
  $argn = 1;
  for (@params) { 
      print OS "$trans{$_} arg".$argn++.",\n" unless $_ eq "NONE";
  }
  print OS "gpointer user_data);\n";

  print OS <<EOT;
void gtk_marshal_$funcname (GtkObject    *object, 
                            GtkSignalFunc func, 
                            gpointer      func_data, 
                            GtkArg       *args)
{
  GtkSignal_$funcname rfunc;
EOT

  if($retval ne "NONE") {
      print OS "  $trans{$retval}  *return_val;\n";
      print OS "  return_val = GTK_RETLOC_$retval (args[".(scalar @params)."]);\n";
  }
  print OS "  rfunc = (GtkSignal_$funcname) func;\n";
  print OS "  *return_val = " unless $retval eq "NONE";
  print OS                  " (* rfunc) (object,\n";

  for($i = 0; $i < (scalar @params); $i++) {
      if ($params[$i] ne "NONE") {
	  print OS "                      GTK_VALUE_$params[$i](args[$i]),\n";
      }
  }

  print OS  "                             func_data);\n}\n\n";
}
print OH "#endif\n";

close(IL); close(OH); close(OS);
