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
01 #if it_works
02     we should get here
03     and here
04 #endif
05     and stop after this...
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

set %test-report "START: Zero length magic repeat matches"
execute-procedure report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a zero-length match at the start of line 1
;
set %test-report "  ^[^#]* - search 1"
execute-procedure report-status

search-forward ^[^#]*
  set %curtest Search1
  set %expline 1
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 0
execute-procedure check-position-matchlen

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 2-line + newline at the start of line 2
;
set %test-report "  ^[^#]* - re-search 2"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search2
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 36
execute-procedure check-position-matchlen

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 1-line + newline at the start of line 3
;
set %test-report "  ^[^#]* - re-search 3"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search3
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 13
execute-procedure check-position-matchlen

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a zero-length match at the start of line 4
;
set %test-report "  ^[^#]* - re-search 4"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search4
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 0
execute-procedure check-position-matchlen

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 1-line + newline match at the start of line 5
;
set %test-report "  ^[^#]* - re-search 5"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search5
  set %expline 6
  set %expcol 1
  set %expchar 10
  set %expmatchlen 27
execute-procedure check-position-matchlen

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  ^[^#]* - re-search 5"
execute-procedure report-status

; This should not pass!!!
set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
!force hunt-forward
!if &seq $force_status PASSED
    set %fail &add %fail 1
    set .res "PASSED!"
!else
    set %ok &add %ok 1
    set .res "failed, as required"
!endif

set %test-report &cat "Expected FAILing search " .res
execute-procedure report-status

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
