#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Check that searching works if Magic modeis used, then it is switched
# off.
# At one point the step_scan was no reset, so the pattern structure for
# the Magic search was repeated.
# Short and simple
#

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
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03
04 Now some Russian сейчас
05 In UPPER СЕЙЧАС
06
07 We'll stop here...
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $match are what I expect them to be.
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
;
find-file autotest.tfile

set %test-report "START: Magic On then Off tests"
execute-procedure report-status

; ==== Run in Magic, non-Exact 
;
set %test-report "   сейчас searches"
execute-procedure report-status
beginning-of-file
delete-mode Exact
add-mode Magic

2 search-forward сейчас
; Where did we end up
  set %curtest Search-Magic-caseoff
  set %expline 5
  set %expcol 16
  set %expchar 10
  set %expmatch "СЕЙЧАС"
execute-procedure check-position-match

; Now switch Magic off and Exact on. Search for ASCII
;
set %test-report "   stop searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic
search-forward stop
; Where did we end up
  set %curtest Search- NoMagic-caseon
  set %expline 7
  set %expcol 11
  set %expchar &asc " "
  set %expmatch "stop"
execute-procedure check-position-match


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Show the report...
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
