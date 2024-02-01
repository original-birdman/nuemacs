#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of word movement

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

# Write out the test input file.
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 –—‘“”„†•…‰™œŠŸž€ ΑΒΓΔΩαβγδω АБВГДабвгд
02 –—‘“”„†•…‰™œŠŸž€ ΑΒΓΔΩαβγδω АБВГДабвгд
03
04 This is just some 2- and 3-byte unicode chars on lines 1 and 2
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
execute-file autotest/check-position.rc

store-procedure check-kill
  !if &seq $kill %expkill
    set %test-report &cat %curtest &cat " - kill text OK: " %expkill
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG kill text, got: " $kill
    set %test-report &cat %test-report &cat " - expected: " %expkill
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Character deletion tests"
execute-procedure report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "Backwards tests"
execute-procedure report-status

; We should always be on line 1
;
set %expline 1
1 goto-line
set $curcol 3
3 delete-next-character
  set %curtest "Delete forwards 1"
  set %expcol 3
  set %expchar &asc „
execute-procedure check-position
  set %expkill "‘“”"
execute-procedure check-kill
unmark-buffer

; Read the file anew each time
;
read-file autotest.tfile
1 goto-line
set $curcol 24
11 delete-next-character
  set %curtest "Delete forwards 2"
  set %expcol 24
  set %expchar &asc б
execute-procedure check-position
  set %expkill "βγδω АБВГДа"
execute-procedure check-kill
unmark-buffer

; Read the file anew each time
; This should delete over a newline
;
read-file autotest.tfile
1 goto-line
set $curcol 38
4 delete-next-character
  set %curtest "Delete forwards 3"
  set %expcol 38
  set %expchar &asc ‘
execute-procedure check-position
  set %expkill &ptf "%s%s%s" "д" ~n "–—"
execute-procedure check-kill
unmark-buffer

; Read the file anew each time
;
read-file autotest.tfile
1 goto-line
set $curcol 16
6 delete-previous-character
  set %curtest "Delete backwards 1"
  set %expcol 10
  set %expchar &asc €
execute-procedure check-position
  set %expkill "‰™œŠŸž"
execute-procedure check-kill
unmark-buffer

; Read the file anew each time
;
read-file autotest.tfile
1 goto-line
set $curcol 39
10 delete-previous-character
  set %curtest "Delete backwards 2"
  set %expcol 29
  set %expchar &asc ~n
execute-procedure check-position
  set %expkill "АБВГДабвгд"
execute-procedure check-kill
unmark-buffer

; Read the file anew each time
; This should delete over a newline (so starts on line 2...)
;
read-file autotest.tfile
2 goto-line
set $curcol 16
22 delete-previous-character
  set %curtest "Delete backwards 3"
  set %expcol 33
  set %expchar &asc €
execute-procedure check-position
  set %expkill &ptf "%s%s%s" "Дабвгд" ~n "–—‘“”„†•…‰™œŠŸž"
execute-procedure check-kill
unmark-buffer

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
