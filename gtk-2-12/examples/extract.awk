# extract - extract C source files from GTK Tutorial
# Copyright (C) Tony Gale 1998
# Contact: gale@gtk.org
#
# Command Switches:
# -c : Just do checking rather than output files
# -f <filename> : Extract a specific file
# -d : Extract files to current directory
# -v : Verbose
 
BEGIN {in_example=0; check=0; spec_example=""; do_output=0; flatten=0; verbose=0;
       for (i=0 ; i < ARGC ; i++) {
	if ( ARGV[i] == "-c" ) {
	  check = 1;
	  ARGV[i]="";
	} else if ( ARGV[i] == "-f" ) {
	  spec_example=ARGV[i+1];
	  ARGV[i]="";
	  ARGV[i+1]="";
	  if ( length(spec_example) == 0 ) {
	    print "usage: -f <filename>";
	    exit;
	  }
	} else if ( ARGV[i] == "-d" ) {
	  flatten = 1;
	  ARGV[i]="";
	} else if ( ARGV[i] == "-v" ) {
	  verbose = 1;
	  ARGV[i] = "";
	}
       }
}

$2 == "example-end" && in_example == 0 	  { printf("\nERROR: multiple ends at line %d\n", NR) > "/dev/stderr";
					    exit}
$2 == "example-end" 			  { in_example=0; do_output=0 }

in_example==1 && check==0 && do_output==1 { gsub(/&amp;/, "\\&", $0);
					    gsub(/&lt;/, "<", $0);
					    gsub(/&gt;/, ">", $0);
					    print $0 >file_name } 

$2 == "example-start" && in_example == 1 { printf("\nERROR: nested example at line %d\n", NR) > "/dev/stderr";
					   exit}

$2 == "example-start"			 { in_example=1 }

$2 == "example-start" && check == 0 \
  { if ( (spec_example == "") || (spec_example == $4) ) {
    if ( flatten == 0 ) {
      if (file_name != "")
          close(file_name);
      file_name = sprintf("%s/%s",$3, $4);
      command = sprintf("mkdir -p %s", $3);
      system(command);
    } else {
      file_name = $4;
    }
    if (verbose == 1)
      printf("%s\n", file_name);
    do_output=1;
  }
  }


END {}



