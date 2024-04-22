#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Magic-mode Character Classes

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

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 match - ]xyzzy[
02 EOF
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

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
find-file autotest.tfile
add-mode Magic

set %test-report "START: Various Character Class tests"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ] in position 1 (start of range)"
run report-status
beginning-of-file
; ====
search-forward []-c]
  set %curtest Search1
  set %expline 1
  set %expcol 3
  set %expchar &asc t
  set %expmatch a
run check-position-match
search-forward []-c]+
  set %curtest Search2
  set %expline 1
  set %expcol 5
  set %expchar &asc h
  set %expmatch c
run check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ] in position 1 (literal)"
run report-status
beginning-of-file
; ====
search-forward []nq]
  set %curtest Search1
  set %expline 1
  set %expcol 10
  set %expchar &asc x
  set %expmatch ]
run check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - in position 1 (literal)"
run report-status
beginning-of-file
; ====
search-forward [-xyz]
  set %curtest Search1
  set %expline 1
  set %expcol 8
  set %expchar &asc " "
  set %expmatch -
run check-position-match
search-forward [-xyz]+
  set %curtest Search2
  set %expline 1
  set %expcol 15
  set %expchar &asc [
  set %expmatch xyzzy
run check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; This should do the same as the above - the "-" has moved, but the
; effect should be indentical.
set %test-report "   - in final position (literal)"
run report-status
beginning-of-file
; ====
search-forward [xyz-]
  set %curtest Search1
  set %expline 1
  set %expcol 8
  set %expchar &asc " "
  set %expmatch -
run check-position-match
search-forward [xyz-]+
  set %curtest Search2
  set %expline 1
  set %expcol 15
  set %expchar &asc [
  set %expmatch xyzzy
run check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - as range"
run report-status
beginning-of-file
; ====
search-forward [x-z]
  set %curtest Search1
  set %expline 1
  set %expcol 11
  set %expchar &asc y
  set %expmatch x
run check-position-match
search-forward [x-z]+
  set %curtest Search2
  set %expline 1
  set %expcol 15
  set %expchar &asc [
  set %expmatch yzzy
run check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - as range - inverted"
run report-status
beginning-of-file
; ====
search-forward [^x-z]
  set %curtest Search1
  set %expline 1
  set %expcol 2
  set %expchar &asc a
  set %expmatch m
run check-position-match
search-forward [^x-z]+
  set %curtest Search2
  set %expline 1
  set %expcol 10
  set %expchar &asc x
  set %expmatch "atch - ]"
run check-position-match

;
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
