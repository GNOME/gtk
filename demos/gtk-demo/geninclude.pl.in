#!/usr/bin/perl -w

print <<EOT;
typedef struct _Demo Demo;

struct _Demo 
{
  gchar *title;
  gchar *filename;
  void (*func) (void);
};

EOT

$array = "";
$first = 1;
for $file (@ARGV) {
    
    ($basename = $file) =~ s/\.c$//;

    if ($first) {
	$first = 0;
    } else {
	$array .= ",\n";
    }

    open INFO_FILE, $file or die "Cannot open '$file'\n";
    $title = <INFO_FILE>;
    $title =~ s@^\s*/\*\s*@@;
    $title =~ s@\s*$@@;

    close INFO_FILE;

    print "void do_$basename (void);\n";
    $array .= qq(  { "$title", "$file", do_$basename });
}

print "\nDemo testgtk_demos[] = {";
print $array;
print "\n};\n";
