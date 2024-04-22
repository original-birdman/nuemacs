#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Magic-mode groups

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
if type perl >/dev/null 2>&1; then
    prog='next if (/^--/); chomp; print substr($_, 3);'
    cmd="perl -lne"
else
    prog='$1 != "--" {print substr($0, 4);}'
    cmd=awk
fi

# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03   (but the ὐ has no upper-case equiv, so stays in lowercase!!!)
04
05 Now some Russian сейчас numbers 77712 (arabic ٧٧٧١٢) [Devenagari ७७७१२]
06 In UPPER СЕЙЧАС NUMBERS 77712 (ARABIC ٧٧٧١٢) [DEVENAGARI ७७७१२]
07
08 Can this be found case-sensitive and insensitive?
-- 123456789012345678901234567890123456789012345678901234567890123456789
09
10 οὐδένmañanaСЕЙЧАС
11 οὐδένmañanaсейчас
12 ΟὐΔΈΝMAÑANAСЕЙЧАС
13
14 And don't forget that I need to check that a match (ANY MATCH!) isn't
15 followed by a combining character (which would raise odd issues for
16 replace). Some more numbers + text 912 TEST.
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $match  are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

execute-file autotest/report-status.rc

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Load the check routine
;
execute-file autotest/check-position-match.rc

execute-file autotest/check-matchcount.rc

execute-file autotest/check-group.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Magic group tests"
run report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  (Upper) followed by (lower)"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %mcount 0
*again1
  !force search-forward "(\p{Lu}+)(\p{Ll}+)"
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search3
  set %expline 16
  set %expcol 15
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
run check-position-match
  set %expcount 11
run check-matchcount

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Reverse search and check match
  search-reverse "(\p{Lu}+)(\p{Ll}+)"
  set %curtest Search3-reversed
  set %expline 16
  set %expcol 11
  set %expchar &asc S
  set %expmatch "Some"
run check-position-match

  set %grpno 1
  set %expmatch S
run check-group
  set %grpno 2
  set %expmatch ome
run check-group

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Some CHOICEs-in-group checks"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
beginning-of-file
  search-forward ".(pp|s)\p{L}" ; \p{L} rather than . stops "as " match.
  set %curtest SearchChoice
  set %expline 3
  set %expcol 25
  set %expchar &asc r
  set %expmatch "uppe"
run check-position-match
  set %grpno 1
  set %expmatch pp
run check-group

  search-forward ".(pp|s)."
  set %curtest SearchChoiceRpt
  set %expline 3
  set %expcol 31
  set %expchar &asc " "
  set %expmatch "ase"
run check-position-match
  set %grpno 1
  set %expmatch s
run check-group

beginning-of-file
  search-forward ".(pp|s)\p{L}"
  reexecute             ; Should end up at same place a second search above
  set %curtest SearchChoice+reexecute
  set %expline 3
  set %expcol 31
  set %expchar &asc " "
  set %expmatch "ase"
run check-position-match

  set %grpno 1
  set %expmatch s
run check-group

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
EOD

# If running them all, leave - but first write out teh buffer if there
# were any failures.
#
if [ "$1" = FULL-RUN ]; then
    cat >>uetest.rc <<'EOD'
!if &not &equ %fail 0
    set $cfname &cat "FAIL-" %test_name
    save-file
!else
    unmark-buffer
!endif
exit-emacs
EOD
# Just leave display showing if being run singly.
else
    cat >>uetest.rc <<'EOD'
unmark-buffer
-2 redraw-display
EOD
fi

# Do it...set the default uemacs if caller hasn't set one.
[ -z "$UE2RUN" ] && UE2RUN="./uemacs -d etc"
$UE2RUN -x ./uetest.rc

if [ "$1" = FULL-RUN ]; then
    if [ -f FAIL-$TNAME ]; then
        echo "$TNAME FAILed"
    else
        echo "$TNAME passed"
    fi
fi
