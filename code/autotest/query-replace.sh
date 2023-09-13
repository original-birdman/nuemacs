#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing repeating a zero-length match in Magic mode

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='next if (/^--/); chomp; print substr($_, 3);'

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
perl -lne "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Text for replacement text - query/interactive version
02 Text should contain a few instances of "ext" and 
03 we'll change most, but not all, of them to !!!!.
04 So we'll end up with 7!s here ext!!!.
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $matchlen are what I expect them to be.
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
execute-file autotest/check-position-matchlen.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Query replace testing"
execute-procedure report-status

beginning-of-file
add-mode Exact

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward query-replace (there *IS* no reverse one).
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; We need to set up individual checking functions for each test
; as we don't get to change expected values anywhere else;
;
store-procedure check1
  set %test-report "  replace 1"
  execute-procedure report-status
  set %curtest Replace-Yes
  set %expline 1
  set %expcol 2
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position-matchlen
!endm
store-procedure check2
  set %test-report "  replace 2"
  execute-procedure report-status
  set %curtest Replace-No
  set %expline 1
  set %expcol 24
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position-matchlen
!endm
store-procedure check3
  set %test-report "  replace 3"
  execute-procedure report-status
  set %curtest replace-Yes
  set %expline 2
  set %expcol 2
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position-matchlen
!endm
store-procedure check4
  set %test-report "  replace 4"
  execute-procedure report-status
  set %curtest Replace-No
  set %expline 2
  set %expcol 42
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position-matchlen
!endm
store-procedure check5
  set %test-report "  replace 5"
  execute-procedure report-status
  set %curtest Replace-ALL
  set %expline 4
  set %expcol 31
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position-matchlen
!endm
; After it completes we will be after the last replaced string
; but the match will be invalidated.
store-procedure check6
  set %test-report "  replace 6"
  execute-procedure report-status
  set %curtest Replace-ALL
  set %expline 4
  set %expcol 35
  set %expchar &asc "!"
  set %expmatchlen 0
  execute-procedure check-position-matchlen
!endm

; Now set-up the control buffer
; 
select-buffer //incremental-debug
insert-string "Y check1"
next-line
insert-string "N check2"
next-line
insert-string "Y check3"
next-line
insert-string "N check4"
next-line
insert-string "! check5"
2 select-buffer

; Now run the search, which will produce the report.
; Not we replace with a longer string
;
query-replace-string "ext" "!!!!"
; Run the final test - query replace has exited....
execute-procedure check6
unmark-buffer
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
