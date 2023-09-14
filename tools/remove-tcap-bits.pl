#!/usr/bin/perl
#
# Simple script to remove any GEGIN/END sections that mention tcapopen
# or tcapmove from valgrind logs, as we can't do anythin about them.
#
# Run this with pwrl's -i command-line arguemnt to do the edit in-place.
#
use strict;
use warnings;

my $in_block = 0;
my @this_block;
my $tcap_seen;

while(<>) {
    if (/^==\d+== BEGIN/) {
        $in_block = 1;
        @this_block = ($_);
        $tcap_seen = 0;
        next;
    }

# If we are not in a block, just print the line.
#
    if (not $in_block) {
        print;
        next;
    }

# Are we at the end of a block?
#
    if (/^==\d+== END/) {
        $in_block = 0;
        if (not $tcap_seen) {
            print @this_block;
            print;      # This line
        }
        @this_block = ();
        next;
    }

    $tcap_seen = 1 if (/by 0x[0-9A-F]+: tcap(?:open|move)/);
    push @this_block, $_;
}

# Dump anything left at the end too.
#
print @this_block if (@this_block);
