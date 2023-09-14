#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Test the translation function

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
1 select-buffer

; Run a test in %test and compare the result to %expect
;
store-procedure run-test
  set .res &ind %test
; Use string test, as some results returns strings (ZDIV, NAN, INF,
;  +INF or TOOBIG
;
  !if &seq .res %expect
    set %test-report &cat "OK: " %test
    set %ok &add %ok 1
  !else
    set %test-report &ptf "FAIL: %s - gave %s (exp: %s)" %test .res %expect
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; Open the current directory
;
set $discmd False               ; Stop sort method displaying
find-file "."
set $discmd True
set %test "&seq $cbufname ~"//directory~""
set %expect TRUE
execute-procedure run-test

; Find a file we know is there and open it (dive in)
;
search-forward main.c
simulate d
set %test "&seq $cbufname main.c"
set %expect TRUE
execute-procedure run-test

; Now open the parent (back to where we started)
;
set $discmd False
open-parent
set $discmd True
set %test "&seq $cbufname ~"//directory~""
set %expect TRUE
execute-procedure run-test

; Go up from this directory and "check" we went up
;
set %ocpath $cfname
set $discmd False
simulate U
set $discmd True
set %ncpath $cfname
set %test "&les &len %ncpath &len %ocpath"
set %expect TRUE
execute-procedure run-test

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
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
