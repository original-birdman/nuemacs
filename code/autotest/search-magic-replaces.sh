#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Magic-mode replacement groups/functions...

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

# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 ABC
02 XYZ
03 GML
04 7UP
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

; The check routine
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-line
;   Expects these to have been set, since this tests them all.
; %expline      the expected text of the current line
;
  !if &seq $line %expline
    set %test-report &cat %curtest &cat " - line OK: " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG line, got: " $line
    set %test-report &cat %test-report &cat " - expected: " %expline
    set %fail &add %fail 1
  !endif
  run report-status

!endm


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Magic replacment tests"
run report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Append counter"
run report-status

; Do the work
beginning-of-file
replace-string "^(...)" "${1}-${@}"

; Check what we have...
;
beginning-of-file
set %curtest "Counter replace"
set %expline "ABC-1"
run check-line
next-line
set %expline "XYZ-2"
run check-line
next-line
set %expline "GML-3"
run check-line
next-line
set %expline "7UP-4"
run check-line

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Reorder groups"
run report-status

; Do the work
;
unmark-buffer
read-file autotest.tfile

replace-string "^(.)(.)(.)" "BOL ${2}${3}${1} EOL"

; Check what we have...
;
beginning-of-file
set %curtest "Reorder match groups"
set %expline "BOL BCA EOL"
run check-line
next-line
set %expline "BOL YZX EOL"
run check-line
next-line
set %expline "BOL MLG EOL"
run check-line
next-line
set %expline "BOL UP7 EOL"
run check-line

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Function calls and groups"
run report-status

; Do the work
;
unmark-buffer
read-file autotest.tfile

; Only line 4 should not be 0 (7UP is 7 to uemacs macros)
;
replace-string "^(...)" "${&tim ${1} 6}"

; Check what we have...
;
beginning-of-file
set %curtest "Number multiplication"
set %expline "0"
run check-line
next-line
set %expline "0"
run check-line
next-line
set %expline "0"
run check-line
next-line
set %expline "42"
run check-line

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Function calls groups and counters"
run report-status

; Do the work
;
unmark-buffer
read-file autotest.tfile

; Multiple the ASCII value of the middle character by the counter
;
replace-string "^.(.)." "${&tim &asc ${1} ${@:start=2,incr=3}}"

; Check what we have...
;
beginning-of-file
set %curtest "Counter usage in multiplication"
set %expline "132"              ; 66 * 2
run check-line
next-line
set %expline "445"              ; 89 * 5
run check-line
next-line
set %expline "616"              ; 77 * 8
run check-line
next-line
set %expline "935"              ; 85 * 11
run check-line

; Use different  buffer for further tests.
; unmark current one first
;
unmark-buffer
select-buffer TEST
beginning-of-file
add-mode Exact
add-mode Magic

; A replacement with nested groups where one group is not matched (so is "")
; The matching and replacement text should be long enough to force an
; (r)mcpat reallocation
; It is:
;   Start group (0-3) ==    4
;   End group (0-3)   ==    4
;   Literal x9        ==    9
;   Any byte x2       ==    2
;   CHOICE x1         ==    1
; So 20 entries, and MAGIC_INCR is 16.
;
; The replacement has:
;   Literal x19        19
;   Add group x4        4
;   End group           1
; So 24 entries, also > 16
;
set %curtest "Unmatched group"
insert-string "abcdefghi"
beginning-of-file
replace-string "abc((..)|(def))ghi" "gp0:${0} gp1:${1} gp2:${2} gp3:${3}"
set %expline "gp0:abcdefghi gp1:def gp2: gp3:def"
run check-line

; A replacement where the groups reverse order
; First clear the buffer
;
beginning-of-file
set-mark
end-of-file
kill-region

set %curtest "Reverse groups in replacement"
insert-string "   ds_val(xyz)  ds_len(fgh)  "
beginning-of-file
replace-string "ds_(.*?)\((.*?)\)" "${2}->${1}"
set %expline "   xyz->val  fgh->len  "
run check-line

; A replacement with a lot of groups, to force realloc (so > 10)
; Include one "wrapper" group (3) that opens before and closes after the
; first realloc
; First clear the buffer
;
beginning-of-file
set-mark
end-of-file
kill-region

set %curtest "Lots of groups in replacement"
insert-string "0123456789abcdef"
beginning-of-file
set .match "(.)(.)((.)(.)(.)(.)(.)(.))(.)(.)(..)(..)(..)"
set .replace "${14}${13}${12}${11}${10}${9}${8}${7}${6}${5}${4}${3}${2}${1}${0}"
replace-string .match .replace
set %expline "efcdab98765432234567100123456789abcdef"
run check-line

; Multi-group, with second group matching its second choice.
;

beginning-of-file
set-mark
end-of-file
kill-region

set %curtest "Second group matches second choice"
insert-string "<p>abc</p>"
beginning-of-file
replace-string </(p|li|dd|dt|blockquote)(\s|>) XXX${2}
set %expline <p>abcXXX>
run check-line

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
