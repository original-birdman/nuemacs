#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of *, + {m,n} and ? magic-mode operators.

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
01 Simple testing for */+/{m,n} handling.
02 acc, adc, a*c, a+c.
03
04 ac
05 abc
06 abbc
07 abbbc
08 abbbbc
09
10 Some whitespace tests too
11
12 a 	
13 c
14
15 EOF
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

set %test-report "START: Various magic repeating tests"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab*c searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again1
  !force search-forward ab*c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search1
  set %expline 10
  set %expcol 15
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 7
run check-matchcount

; Reverse search and check match
  search-reverse ab*c
  set %curtest Search1-reversed
  set %expline 10
  set %expcol 13
  set %expchar &asc a
  set %expmatch ac
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab*?c searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again2
  !force search-forward ab*?c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
; Should be the same as Search1
  set %curtest Search2
  set %expline 10
  set %expcol 15
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 7
run check-matchcount

; Reverse search and check match
  search-reverse ab*?c
  set %curtest Search2-reversed
  set %expline 10
  set %expcol 13
  set %expchar &asc a
  set %expmatch ac
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab{2,4}c searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again3
  !force search-forward ab{2,4}c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again3
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search3
  set %expline 8
  set %expcol 7
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 3
run check-matchcount

; Reverse search and check match
  search-reverse ab{2,4}c
  set %curtest Search3-reversed
  set %expline 8
  set %expcol 1
  set %expchar &asc a
  set %expmatch abbbbc
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab+ searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again4
  !force search-forward ab+
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again4
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search4
  set %expline 8
  set %expcol 6
  set %expchar &asc c
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 4
run check-matchcount

; Reverse search and check match
  search-reverse ab+
  set %curtest Search4-reversed
  set %expline 8
  set %expcol 1
  set %expchar &asc a
  set %expmatch abbbb
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab+? searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again5
  !force search-forward ab+?
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again5
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search5
  set %expline 8
  set %expcol 3
  set %expchar &asc b
  set %expmatch ""      ; We'll have failed, so no match
run check-position
  set %expcount 4
run check-matchcount

; Reverse search and check match
  search-reverse ab+?
  set %curtest Search4-reversed
  set %expline 8
  set %expcol 1
  set %expchar &asc a
  set %expmatch ab
run check-position

;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
EOD

# If running them all, leave - but first write out the buffer if there
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
