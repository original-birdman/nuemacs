#!/bin/bash
#

lc=1000
rm -f read-speed.tfile
while [ $lc -gt 0 ]; do
    cat ../CHANGES search.c >> read-speed.tfile
    let "lc=$lc - 1"
done

lines=`wc -l < read-speed.tfile`

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
# This contains all of the tests
#
cat >uetest.rc <<'EOD'

find-file read-speed.tfile

exit-emacs
EOD

[ -z "$UE2RUN" ] && UE2RUN=./uemacs
$UE2RUN -v
etime=`/usr/bin/time -f "%E" $UE2RUN -P -x  ./uetest.rc 2>&1`

rm -f read-speed.tfile

(echo $etime; echo $lines) | perl -e '
    chomp (my $etime = <>);
    my @tbits = reverse(split ":", $etime);
    my $time = $tbits[0];
    shift @tbits;
    if (@tbits) {
        $time = $time + 60*$tbits[0];
        shift @tbits;
    }
    if (@tbits) {
        $time = $time + 60*60*$tbits[0];
        shift @tbits;
    }
    chomp(my $lc = <>);
    my $lrate = $lc/$time;
    printf "Rate is %.2f lines/sec\n", $lrate;
'
echo "(Read $lines lines in $etime)"
