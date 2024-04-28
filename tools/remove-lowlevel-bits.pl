#!/usr/bin/perl
#
# Simple script to remove any BEGIN/END sections that mention tcapopen
# or tcapmove from valgrind logs, as we can't do anything about them.
# Also now clears valgridn bits (for FreeBSD).
#
# Run this with perl's -i command-line arguemnt to do the edit in-place.
#
use strict;
use warnings;

my $in_block = 0;
my @this_block;
my $lowlevel;

while(<>) {
    if (/^==\d+== BEGIN/) {
        $in_block = 1;
        @this_block = ($_);
        $lowlevel = 0;
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
        if (not $lowlevel) {
            print @this_block;
            print;      # This line
        }
        @this_block = ();
        next;
    }

# Remove tcap and valgrind bits that are nothing to do with uemacs
#
    $lowlevel = 1 if (/by 0x[0-9A-F]+: tcap(?:open|move)/);
    $lowlevel = 1 if (/at 0x[0-9A-F]+: (?:malloc|realloc) \(vg_replace_malloc/);
    push @this_block, $_;
}

# Dump anything left at the end too.
#
print @this_block if (@this_block);
