#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of wrapping in Wrap mode

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
#
# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 We need some text to test wrapping.
02 We'll set the fill-column to 60 before running things.
03
04 For the next two we'll test at the end of each line.
05 They should behave differently in/not in FullWrap.
06
07 This is a normal line, where the text goes beyond the fill column.
08 And another, where spaces follow the text beyond the fill    column.
09
-- 123456789012345678901234567890123456789012345678901234567890123456789
10 These lines are to test zero-width breaks (U+200b).
11
12 This is a line containing a zero-width break after Sh. Sh​oulder.
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
delete-mode wrap
1 select-buffer

; Load the check routine
;
execute-file autotest/check-position-match.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-full-line
;   Expects these to have been set, since this tests them all.
; %expltext     the expected text of the line
;
  !if &seq $line %expltext
    set %test-report &cat %curtest &cat " OK text for: line " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " WRONG text for: line " $curline
    set %test-report &cat %test-report &cat " expected: " &chr 10
    set %test-report &cat %test-report %expltext
    set %test-report &cat %test-report &cat &chr 10 &cat " got: " &chr 10
    set %test-report &cat %test-report $line
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Functions to set the required $ggr_opts wrap mode on or off
; before the simulate call.
;
store-procedure set-wrap-mode
    set $ggr_opts &bor $ggr_opts 0x04
!endm
store-procedure unset-wrap-mode
    set $ggr_opts &ban $ggr_opts &bno 0x04
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Wrap mode tests"
execute-procedure report-status

; First check the zero-width char is there. In case it was
; accidentally removed in an edit....
;
add-mode Magic
!force search-forward \u{200b}
!if &equ 0 &len &grp 0
    set %test-report "The zero-width char in Shoulder is missing. Cannot continue"
    execute-procedure report-status
    select-buffer test-reports
    unmark-buffer
    !finish
!endif
delete-mode Magic

; Set mode we are testing
;
add-mode Wrap
execute-procedure set-wrap-mode
60 set-fill-column

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Line7 tests"
execute-procedure report-status

; Insert space at end of line 7
;
7 goto-line
end-of-line
simulate " "
  set %curtest Line7-FullWrap
  set %expline 8
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

  set %expltext "column. "
execute-procedure check-full-line

; Re-read file...
unmark-buffer
read-file autotest.tfile

7 goto-line
end-of-line
execute-procedure unset-wrap-mode
simulate " "
  set %curtest Line7-OrigWrap
  set %expline 8
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

  set %expltext "column. "
execute-procedure check-full-line

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Insert space at end of line 8
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
end-of-line
execute-procedure set-wrap-mode
simulate " "
  set %curtest Line8-FullWrap
  set %expline 9
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

previous-line
; Expect line to have spaces removed
  set %expltext "And another, where spaces follow the text beyond the fill"
execute-procedure check-full-line

; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
end-of-line
execute-procedure unset-wrap-mode
simulate " "
  set %curtest Line8-OrigWrap
  set %expline 9
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

previous-line
; Expect line to have trailing spaces
  set %expltext "And another, where spaces follow the text beyond the fill    "
execute-procedure check-full-line


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Insert space in the space-gap on line 8
; Note that these behave differently.
; They both leave you at the start of the next line
; but FullWrap has removed all spaces from the end of the preceding line.
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
beginning-of-line
61 forward-character
execute-procedure set-wrap-mode
simulate " "
  set %curtest Line8-FullWrap-MidSpace
  set %expline 9
  set %expcol 1
  set %expchar &asc c
execute-procedure check-position-match

; We expect this line to be the wrapped "column."
  set %expltext "column."
execute-procedure check-full-line

previous-line
; We expect this line to have no trailing spaces
  set %curtest Line8-FullWrap-MSprevline
  set %expltext "And another, where spaces follow the text beyond the fill"
execute-procedure check-full-line

; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
beginning-of-line
61 forward-character
execute-procedure unset-wrap-mode
simulate " "
; We expect this to have wrapped
  set %curtest Line8-OrigWrap-MidSpace
  set %expline 9
  set %expcol 1
  set %expchar &asc c
execute-procedure check-position-match

; We expect this line to be the wrapped "column."
  set %expltext "column."
execute-procedure check-full-line

previous-line
; We expect this line to have the trailing spaces left in place
  set %curtest Line8-OrigWrap-MSprevline
  set %expltext "And another, where spaces follow the text beyond the fill    "
execute-procedure check-full-line

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Wrap on a zero-width break
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
execute-procedure set-wrap-mode
simulate " "
; Expect to be at end of the wrapped text - looking at the zwb.
  set %curtest Line12-FullWrap-zwb
  set %expline 13
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

; The line is expected to contain oulder
  set %expltext "oulder. "
execute-procedure check-full-line

; The previous line is expected to end with the zero-width break.
previous-line
end-of-line
backward-character
  set %curtest Line12-FullWrap-zwb-eol
  set %expline 12
  set %expcol 58
  set %expchar &blit 0x200b
execute-procedure check-position-match

;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
; Same results as with mode set expected
execute-procedure unset-wrap-mode
simulate " "
; Expect to be at end of the wrapped text - looking at the zwb.
  set %curtest Line12-OrigWrap-zwb
  set %expline 13
  set %expcol 9
  set %expchar 10
execute-procedure check-position-match

; The line is expected to contain oulder
  set %expltext "oulder. "
execute-procedure check-full-line

; The previous line is expected to end with the zero-width break.
previous-line
end-of-line
backward-character
  set %curtest Line12-OrigWrap-zwb-eol
  set %expline 12
  set %expcol 58
  set %expchar &blit 0x200b
execute-procedure check-position-match

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Repeat with extra txt at the end.
; FullWrap will wrap back to fillcol.
; OrigWrap just wraps last word.
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
insert-string " xyzzy"
execute-procedure set-wrap-mode
simulate " "
  set %curtest Line12+-FullWrap+xyzzy
  set %expline 13
  set %expcol 15
  set %expchar 10
execute-procedure check-position-match
  set %expltext "oulder. xyzzy "
execute-procedure check-full-line

;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
insert-string " xyzzy"
execute-procedure unset-wrap-mode
simulate " "
  set %curtest Line12-OrigWrap+xyzzy
  set %expline 13
  set %expcol 7
  set %expchar 10
execute-procedure check-position-match
  set %expltext "xyzzy "
execute-procedure check-full-line

unmark-buffer

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
