#!/usr/bin/perl -w
#
# by Elliot Lee <sopwith@redhat.com>

%trans = ( "NONE"=>"void", "CHAR"=>"char",
	   "BOOL"=>"gboolean", "INT"=>"gint",
	   "UINT"=>"guint", "LONG"=>"glong",
	   "ULONG"=>"gulong", "FLOAT"=>"gfloat",
	   "DOUBLE"=>"gdouble", "STRING"=>"gpointer",
	   "ENUM"=>"gint", "FLAGS"=>"gint",
	   "BOXED"=>"gpointer",
	   "POINTER"=>"gpointer",
	   "OBJECT"=>"gpointer",

# complex types. These need special handling.
		"FOREIGN"=>"FOREIGN",
		"C_CALLBACK"=>"C_CALLBACK",
		"SIGNAL"=>"SIGNAL",
		"ARGS"=>"ARGS",
		"CALLBACK"=>"CALLBACK"
		);

if ($#ARGV != 2) {
	die ("Wrong number of arguments given, need <source> <target.h> <target.c>");
}

open(IL, "<" . $ARGV[0]) || die ("Open failed: $!");
open(OH, ">" . $ARGV[1]) || die ("Open failed: $!");
open(OS, ">" . $ARGV[2]) || die ("Open failed: $!");

print OH <<EOT;
#ifndef __GTK_MARSHAL_H__
#define __GTK_MARSHAL_H__

#include <gtk/gtktypeutils.h>
#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define gtk_signal_default_marshaller gtk_marshal_NONE__NONE

EOT

print OS qq(#include "gtkmarshal.h"\n\n);

while (defined($aline = <IL>)) {
  chomp $aline;
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
	if($_ eq "C_CALLBACK") {
		print OS "GtkFunction arg".$argn."a,\n";
		print OS "gpointer    arg".$argn."b,\n";
		$argn++;
	} elsif($_ eq "SIGNAL") {
		print OS "gpointer arg".$argn."a,\n";
		print OS "gpointer arg".$argn."b,\n";
		$argn++;
	} elsif($_ eq "ARGS") {
		print OS "gpointer arg".$argn."a,\n";
		print OS "gpointer arg".$argn."b,\n";
		$argn++;
	} elsif($_ eq "CALLBACK") {
		print OS "gpointer arg".$argn."a,\n";
		print OS "gpointer arg".$argn."b,\n";
		print OS "gpointer arg".$argn."c,\n";
		$argn++;
	} elsif($_ eq "FOREIGN") {
		print OS "gpointer arg".$argn."a,\n";
		print OS "gpointer arg".$argn."b,\n";
		$argn++;
	} else {
		print OS "$trans{$_} arg".$argn++.",\n" unless $_ eq "NONE";
	}
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
      $retn = 0;
      $retn = scalar @params unless $params[0] eq "NONE";
      print OS "  return_val = GTK_RETLOC_$retval (args[$retn]);\n";
  }
  print OS "  rfunc = (GtkSignal_$funcname) func;\n";
  print OS "  *return_val = " unless $retval eq "NONE";
  print OS                  " (* rfunc) (object,\n";

  for($i = 0; $i < (scalar @params); $i++) {
      if($params[$i] eq "C_CALLBACK") {
	print OS <<EOT;
GTK_VALUE_C_CALLBACK(args[$i]).func,
GTK_VALUE_C_CALLBACK(args[$i]).func_data,
EOT
      } elsif ($params[$i] eq "SIGNAL") {
	print OS <<EOT;
GTK_VALUE_SIGNAL(args[$i]).f,
GTK_VALUE_SIGNAL(args[$i]).d,
EOT
      } elsif ($params[$i] eq "CALLBACK") {
	print OS <<EOT;
GTK_VALUE_CALLBACK(args[$i]).marshal,
GTK_VALUE_CALLBACK(args[$i]).data,
GTK_VALUE_CALLBACK(args[$i]).notify,
EOT
      } elsif ($params[$i] eq "FOREIGN") {
	print OS <<EOT;
GTK_VALUE_FOREIGN(args[$i]).data,
GTK_VALUE_FOREIGN(args[$i]).notify,
EOT
      } elsif ($params[$i] eq "NONE") {
      } else {
	print OS "                      GTK_VALUE_$params[$i](args[$i]),\n";
      }
  }

  print OS  "                             func_data);\n}\n\n";
}
print OH <<EOT;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_MARSHAL_H__ */
EOT

close(IL); close(OH); close(OS);
