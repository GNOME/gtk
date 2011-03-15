#!/usr/bin/perl -w

my $FILENAME = $ARGV[0];

open FILE, $FILENAME or die "Cannot open $FILENAME";

my $ARRAYNAME = $ARGV[1];
print "static const char $ARRAYNAME\[\] =";
while (<FILE>) {
    s@\\@\\\\@g;
    s@"@\\"@g;
    chomp ($_);
    print "\n  \"$_\\n\"";
}
print ";\n";
