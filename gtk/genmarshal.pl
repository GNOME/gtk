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
	   "C_CALLBACK"=>"gpointer", "OBJECT"=>"gpointer",
	   "STYLE"=>"gpointer", "GDK_EVENT"=>"gpointer");

$srcdir = $ENV{'srcdir'} || '.';

open(IL, "<$srcdir/gtkmarshal.list") || die("Open failed: $!");
open(OH, "|indent > $srcdir/gtkmarshal.h") || die("Open failed: $!");
open(OS, "|indent > $srcdir/gtkmarshal.c") || die("Open failed: $!");

print OH <<EOT;
#ifndef __GTKMARSHAL_H__
#define __GTKMARSHAL_H__ 1

#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define gtk_signal_default_marshaller gtk_marshal_NONE__NONE

EOT

print OS qq(#include "gtkmarshal.h"\n\n);

while(chomp($aline = <IL>)) {
  ($retval, $paramlist) = split(/:/, $aline, 2);
  @params = split(/\s*,\s*/, $paramlist);

  my $funcname = $retval."__".join("_",@params);
  my $defname;
  
  next if (exists $defs{$funcname});

  $doequiv = 0;
  for (@params, $retval) { 
      if ($trans{$_} eq "gpointer" && $_ ne "POINTER") {
	  $doequiv = 1;
	  last;
      }
      if($_ eq "ENUM" || $_ eq "UINT" || $_ eq "ULONG") {
	$doequiv = 1;
	last;
      }
  }

  # Translate all function pointers to gpointer
  $defname = $funcname;
  if($doequiv) {
      print OH "#define gtk_marshal_$funcname ";
      $defs{$defname} = 1;
      
      for (@params, $retval) {
	if ($trans{$_} eq "gpointer") {
	  $_ = "POINTER";
	}
	if($_ eq "ENUM") {
	  $_ = "UINT";
	}
	if($_ eq "UINT") {
	  $_ = "INT"; # Unvalidated assumption - please check
	}
	if($_ eq "ULONG") {
	  $_ = "LONG";
	}
      }

      $funcname = $retval."__".join("_",@params);

      print OH "gtk_marshal_$funcname\n\n";
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
print OH <<EOT;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTKMARSHAL_H__ */
EOT

close(IL); close(OH); close(OS);
