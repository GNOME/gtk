#!/usr/bin/perl -w

#  GNOME entry finder utility.
#  (C) 2000 The Free Software Foundation
# 
#  Author(s): Kenneth Christiansen


$VERSION = "1.0.0 beta 5";
$LANG    = $ARGV[0];
$OPTION2 = $ARGV[1];
$SEARCH  = "Name";

if (! $LANG){
    print "desk.pl:  missing file arguments\n";
    print "Try `desk.pl --help' for more information.\n";
    exit;
}

if ($OPTION2){
    $SEARCH=$OPTION2;
}

if ($LANG){

if ($LANG=~/^-(.)*/){

    if ("$LANG" eq "--version" || "$LANG" eq "-V"){
        print "GNOME Entry finder $VERSION\n";
	print "Written by Kenneth Christiansen <kenneth\@gnome.org>, 2000.\n\n";
	print "Copyright (C) 2000 Free Software Foundation, Inc.\n";
	print "This is free software; see the source for copying conditions.  There is NO\n";
	print "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
        exit;
    }

    elsif ($LANG eq "--help" || "$LANG" eq "-H"){
	print "Usage: ./desk.pl [OPTIONS] ...LANGCODE ENTRY\n";
	print "Checks .desktop and alike files for missing translations.\n\n";
	print "  -V, --version                shows the version\n";
	print "  -H, --help                   shows this help page\n";
	print "\nReport bugs to <kenneth\@gnome.org>.\n";
        exit;
    }

    else{
    	print "desk.pl: invalid option -- $LANG\n";
    	print "Try `desk.pl --help' for more information.\n";
        exit;
    }
}

else{

    $a="find ../ -print | egrep '.*\\.(desktop|soundlist"
      ."|directory)' ";

    $b="find ../ -print | egrep '.*\\.(desktop|soundlist"
      ."|directory)' "; 

    print "Searching for missing $SEARCH\[$LANG\] entries...\n";

    open(BUF1, "$a|");
    open(BUF2, "$b|");

    @buf1 = <BUF1>;
    foreach my $file (@buf1){ 
        open FILE, "<$file";
        while (<FILE>) {
           if ($_=~/$SEARCH\[$LANG\]\=/o){ 
                $file = unpack("x2 A*",$file) . "\n";		
                push @buff1, $file;
                last;
           }
        }
    }

    @buf2 = <BUF2>;
    foreach my $file (@buf2){
        open FILE, "<$file";
        while (<FILE>) {
           if ($_=~/$SEARCH\=/o){
                $file = unpack("x2 A*",$file) . "\n";
                push @buff2, $file;
                last;
           }
        }
    }

    @bufff1 = sort (@buff1);
    @bufff2 = sort (@buff2);

    my %in2;
    foreach (@bufff1) {
        $in2{$_} = 1;
    }
 
   foreach (@bufff2){
        if (!exists($in2{$_})){
            push @result, $_ } 
        }
    }

    open(OUT1, ">MISSING.$LANG.$SEARCH");
       print OUT1 @result ;
    close OUT1;


    stat("MISSING.$LANG.$SEARCH");
        print "\nWell, you need to fix these:\n\n" if -s _;
        print @result if -s _;
        print "\nThe list is saved in MISSING.$LANG.$SEARCH\n" if -s _;
        print "\nWell, it's all perfect! Congratulation!\n" if -z _;
        unlink "MISSING.$LANG.$SEARCH" if -z _;
    exit;	
}


