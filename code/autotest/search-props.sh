#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Magic-mode properties

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
execute-file autotest/check-position.rc

execute-file autotest/check-matchcount.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Magic property tests"
run report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Upper, punctuation - \p{Lu}{3}\p{P}"
run report-status
; ====
search-forward \p{Lu}{3}\p{P}   ; should match ΔΈΝ. ...
  set %curtest Search1
  set %expline 2
  set %expcol 16
  set %expchar 10
  set %expmatch ΔΈΝ.
run check-position

; ====
search-forward \p{Lu}{3}\p{P}   ; ...now TCH!
  set %curtest Search2
  set %expline 14
  set %expcol 63
  set %expchar &asc )
  set %expmatch TCH!
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  !upper, space, 3 numbers - \P{Lu} \p{N}{3}"
run report-status
beginning-of-file

set %mcount 0
*again1
  !force search-forward "\P{Lu} \p{N}{3}"   ; NOT upper ltr, space, 3 nmbrs...
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search3
  set %expline 16
  set %expcol 39
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 4
run check-matchcount

; Reverse search and check match
  search-reverse "\P{Lu} \p{N}{3}"
  set %curtest Search3-reversed
  set %expline 16
  set %expcol 34
  set %expchar &asc t
  set %expmatch "t 912"
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  lower, anything, Upper - \p{Ll}\X\p{LU}
run report-status
beginning-of-file

set %mcount 0
*again2
  !force search-forward "\p{Ll}\X\p{LU}"
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search4
  set %expline 14
  set %expcol 24
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 10      ; 10 since GGR4.162 - with 1 overlapping
run check-matchcount

; Reverse search (x2) and check match
2 search-reverse "\p{Ll}\X\p{LU}"
  set %curtest Search4-reversed-twice
  set %expline 12
  set %expcol 2
  set %expchar &asc ὐ
  set %expmatch ὐΔΈ
run check-position

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
