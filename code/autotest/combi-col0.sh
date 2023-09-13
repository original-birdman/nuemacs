#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing for combining diacriticals starting a line
# i.e. having nothing to combine with
# (In any other position they will combine with the previous char
#  Trying to add a combining diacritical to a newline will result in it
#  starting the next line, as the line ends as soon as the NL is seen.).

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#

# Write out the test input file
# It's NOT written here with row and column markers, as then
# we can't get the ~ to be teh first char on the line.
cat - > autotest.tfile <<EOD
This is a very simple test.
These 2 lines start with a tilde accent "on its own" at the
start of the line.
The code "pretends" there is a space for it to combine with
(for display only - it does not exist in the file data)
̃a
̃b
end of the file...
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

set %test-report "START: Perverse combining char at start-of-line"
execute-procedure report-status

beginning-of-file
add-mode Exact

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  search"
execute-procedure report-status

6 goto-line
  set %curtest Pos1
  set %expline 6
  set %expcol 1
  set %expchar &blit 0x0303
execute-procedure check-position

; Forward one char should take us to "a" in the next column
forward-character
  set %curtest Pos2
  set %expline 6
  set %expcol 2
  set %expchar &asc a
execute-procedure check-position

; Forward 2 words takes as past "a", then "b"
; then backward-character twice should be at the ~ at start if line
;
2 next-word
2 backward-character
  set %curtest Pos3
  set %expline 7
  set %expcol 1
  set %expchar &blit 0x0303
execute-procedure check-position

; Forward one char should take us to "b" in the next column
forward-character
  set %curtest Pos4
  set %expline 7
  set %expcol 2
  set %expchar &asc b
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
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
