#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Test for searching with hits at beginning/end of line/buffer.

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
01 Able was I ere I was Elba
02 Just some text to be searched in each direction
03 that will produce hist at the start and end of liens and buffer.
04 Able was I ere I was Elba
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

execute-file autotest/check-matchcount.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various boundary searches"
execute-procedure report-status

; ==== Run in EXACT mode
;
set %test-report " forward Elba search"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic

set %mcount 0
*again1
  !force search-forward Elba
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Forward Elba search"
  set %expline 4
  set %expcol 26
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 2
execute-procedure check-matchcount

; Now run a reverse search for Able
set %mcount 0
*again2
  !force search-reverse Able
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Reverse Able search"
  set %expline 1
  set %expcol 1
  set %expchar &asc A
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 2
execute-procedure check-matchcount

; Now repeat the above in Magic mode with a magic string that
; will match the same things.
;
add-mode Magic
set %mcount 0
*again3
  !force search-forward "Elb."
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again3
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Forward Elb. magic search"
  set %expline 4
  set %expcol 26
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 2
execute-procedure check-matchcount

; Now run a reverse search for Able
set %mcount 0
*again4
  !force search-reverse ".ble"
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again4
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Reverse .ble magic search"
  set %expline 1
  set %expcol 1
  set %expchar &asc A
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 2
execute-procedure check-matchcount


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
 
./uemacs -c etc/uemacs.rc -x ./uetest.rc
    
if [ "$1" = FULL-RUN ]; then
    if [ -f FAIL-$TNAME ]; then
        echo "$TNAME FAILed"
    else
        echo "$TNAME passed"
    fi
fi
