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

; Create a check routine
;
store-procedure run-xlat

  set .result &xla %input %from %to
  !if &seq %expect .result
    set %test-report &cat %test " OK"
    set %ok &add %ok 1
  !else
    set %test-report &ptf "%s FAILED. %s not %s" %test .result %expect
    set %fail &add %fail 1
  !endif
  run report-status

!endm

set %test "Simple translate"
set %input  abcde
set %from   ae
set %to     xz
set %expect xbcdz
run run-xlat

set %test "Simple truncate"
set %input  abcde
set %from   ae
set %to     x
set %expect xbcd
run run-xlat

set %test "Simple omit"
set %input  abcde
set %from   cdeab
set %to     x
set %expect x
run run-xlat

set %test "Add accents"
set %input  abcde
set %from   ce
set %to     çé
set %expect abçdé
run run-xlat

set %test "Remove accents"
set %input  abçdé
set %from   çé
set %to     ce
set %expect abcde
run run-xlat

set %test "Add extended chars..."
set %input  abcde
set %from   ce
; The first char is 3 unicode chars
;   U+0063  c
;   U+0327  Combining Cedilla (so NOT U+00e7  c-cedilla)
;   U+20eb  Combining Long Double Solidus Overlay
set %to     ç⃫é
set %expect abç⃫dé
run run-xlat

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
