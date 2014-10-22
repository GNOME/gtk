#!/usr/bin/perl -w

my $ARRAYNAME = $ARGV[0];
my $first = 0;
print "static const char $ARRAYNAME\[\] = {";

for ($i = 1; $i <= $#ARGV; $i = $i + 1) {
    my $FILENAME = $ARGV[$i];
    open FILE, $FILENAME or die "Cannot open $FILENAME";
    while (my $line = <FILE>) {
        foreach my $c (split //, $line) {
            if ($first == 1) {
                printf (",\n");
            }
            printf ("0x%02x", ord ($c));
            $first = 1;
        }
    }
}

print "};\n";

