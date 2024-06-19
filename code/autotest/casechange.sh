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
01 We need a line with a character whose lower and uppercase
02 representation has a different number of utf8 bytes.
03 Kelvin sign (3 bytes -> lower k)
04    KKKKKKKKKKKKKKKKKKKK
05 Cap A with stroke (2 bytes -> 3)
06    ȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺȺ
07 Cap turned A (3 bytes -> 2)
08    ⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯⱯ
-- 123456789012345678901234567890123456789012345678901234567890123456789
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

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Case changing region"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "Shortening charlength"
run report-status

; lowercase a region contains the Kelvin char
;
4 goto-line
beginning-of-line
10 forward-character
set .ocol $curcol
drop-pin
previous-line
set-mark
2 next-line
case-region-lower
back-to-pin

  set %curtest "Pin check on double shortening"
  set %expline 4
  set %expcol .ocol
  set %expchar &asc k
run check-position

; lowercase a region with the turned As
;
end-of-file
set-mark
previous-line
15 forward-character
set .ocol $curcol
drop-pin
previous-line
case-region-lower
switch-with-pin

  set %curtest "Pin check on shortening"
  set %expline 8
  set %expcol .ocol
  set %expchar &asc ɐ
run check-position

; Now uppercase the same region (aftre the first switch-with-pin)
;
switch-with-pin
case-region-upper
switch-with-pin

  set %curtest "Pin check on re-casing"
  set %expline 8
  set %expcol .ocol
  set %expchar &asc Ɐ
run check-position

; lowercase a region with the A strokes
;
8 goto-line
end-of-line
set .ocol $curcol
set-mark
beginning-of-line
case-region-lower
exchange-point-and-mark

  set %curtest "Text check on shortening"
  set %expline 8
  set %expcol .ocol
  set %expchar 10
run check-position

; Check the line contents are correct.
;
set .expect "   ɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐɐ"
!if &seq $line .expect
  set %test-report &cat %curtest &cat " - line OK: " $line
  set %ok &add %ok 1
!else
  set %test-report &cat %curtest &cat " - WRONG line, got: " $line
  set %test-report &cat %test-report &cat " - expected: " .expect
  set %fail &add %fail 1
!endif

run report-status   


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Unmark (modified) autotest.tfile at end of tests...
;
unmark-buffer

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
