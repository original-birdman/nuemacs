#!/usr/bin/perl -CO
#

# Simple script to parse a text file into Unicode characters to
# aid with debugging.
# The -CO above is to indicate we'll have Unicode in the output.
#

use strict;
use warnings;
#

# We'll read the input file entirely into memory in one go.
# This is just intended for small, test files.
#
$/ = undef;
my $data = <>;
my $limit = length($data);

my ($bstr, $res, $togo);

# Step through the byte stream one byte at a time....
#
my $i = 0;
my $first = 1;
while ($i < $limit) {
    my $byte = ord(substr($data, $i++, 1));
    if ($first) {
        $bstr = sprintf("%02x", $byte);
        if ($byte < 0xc0) {
            printf "U+%04x [%c] (%s)\n", $byte, $byte, $bstr;
            next;
        }
        elsif (($byte & 0xe0) == 0xc0) {
            $res = $byte & 0x1f;
            $togo = 1;
        }
        elsif (($byte & 0xf0) == 0xe0) {
            $res = $byte & 0x0f;
            $togo = 2;
        }
        elsif (($byte & 0xf8) == 0xf0) {
            $res = $byte & 0x07;
            $togo = 3;
        }
        else {
            die "Invalid start $byte\n";
        }
# Check the next byte *is* a continuation byte - to handle
# the invalid unicode (iso8859-1?) chars that uemacs actually handles...
# This only applies on a first-byte.
#
        if ((ord(substr($data, $i, 1)) & 0xc0) != 0x80) {
            printf "U+%04x [%c!] (%s)\n", $byte, $byte, $bstr;
            next;
        }
        $first = 0;
        next;
    }
    die sprintf("Invalid continuation %02x\n", $byte)
        if (($byte & 0xc0) != 0x80);
    $res <<= 6;
    my $add = ($byte & 0x3f);
    $res |= $add;
    $bstr = sprintf("%s %02x", $bstr, $byte);
    if (--$togo == 0) {
        printf "U+%04x [%c] (%s)\n", $res, $res, $bstr;
        $first = 1;
    }
}
exit;
