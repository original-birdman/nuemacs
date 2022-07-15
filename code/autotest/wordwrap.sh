#!/bin/sh
#

# Simple testing of wrapping in Wrap mode

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# Need the -b because of the non-Unicode (invalid) 0xf1 bytes.
# But Entware systems don't understand it, and work without it.
#
if awk -b '{}' /dev/null >/dev/null 2>&1; then
    AWKARG=-b
else
    AWKARG=
fi

# Write out the test input file
# It's written here with row and column markers.
#
awk $AWKARG "$prog" > autotest.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
01 We need some text to test wrapping.
02 We'll set the fill-column to 60 before running things.
03
04 For the next two we'll test at the end of each line.
05 They should behave differently in/not in FullWrap.
06
07 This is a normal line, where the text goes beyond the fill column.
08 And another, where spaces follow the text beyond the fill    column.
09
-- 0123456789012345678901234567890123456789012345678901234567890123456789
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
store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to buffer from whence we came
!endm

set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   Expects these to have been set, since this tests them all.
; %expline      the line for the match
; %expcol       the column for the match
; %expchar      the expected "char" at point (R/h side) at the match
; %expmatch     the text of the match
;
  !if &equ $curline %expline
    set %test-report &cat %curtest &cat " - line OK: " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG line, got: " $curline
    set %test-report &cat %test-report &cat " - expected: " %expline
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curcol %expcol
    set %test-report &cat %curtest &cat " - column OK: " $curcol
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG column, got: " $curcol
    set %test-report &cat %test-report &cat " - expected: " %expcol
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curchar 10
    set %pchar "\n"
  !else
    set %pchar &chr $curchar
  !endif
  !if &equ $curchar %expchar
    set %test-report &cat %curtest &cat " - at OK: " %pchar
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - at WRONG char, got: " %pchar
    set %test-report &cat %test-report &cat " expected: " %expchar
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

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
    set %test-report "The zero-width char is missing. Cannot continue"
    execute-procedure report-status
    select-buffer test-reports
    unmark-buffer
    !finish
!endif
delete-mode Magic

; Set mode we are testing
;
add-mode Wrap

60 set-fill-column

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Line7 tests"
execute-procedure report-status

; Insert space at end of line 7
;
; We have to fudge running Wrap, as insert-space etc. don't trigger it
; "2 wrap-word" is FullWrap mode and "wrap-word" is original mode.
;
7 goto-line
end-of-line
2 wrap-word
insert-string " "
  set %curtest Line7-FullWrap
  set %expline 8
  set %expcol 8
  set %expchar 10
execute-procedure check-position

beginning-of-line
  set %curtest Line7-Fullwrap-bol
  set %expline 8
  set %expcol 0
  set %expchar &asc c
execute-procedure check-position

; Re-read file...
unmark-buffer
read-file autotest.tfile

7 goto-line
end-of-line
wrap-word
insert-string " "
  set %curtest Line7-OrigWrap
  set %expline 8
  set %expcol 8
  set %expchar 10
execute-procedure check-position

beginning-of-line
  set %curtest Line7-OrigWrap-bol
  set %expline 8
  set %expcol 0
  set %expchar &asc c
execute-procedure check-position
unmark-buffer

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Insert space at end of line 8
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
end-of-line
2 wrap-word
insert-string " "
  set %curtest Line8-FullWrap
  set %expline 9
  set %expcol 8
  set %expchar 10
execute-procedure check-position

previous-line
end-of-line
backward-character
; Expect to be at end of fill - spaces removed
  set %curtest Line8-Fullwrap-endprev
  set %expline 8
  set %expcol 56
  set %expchar &asc l
execute-procedure check-position

; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
end-of-line
wrap-word
insert-string " "
  set %curtest Line8-OrigWrap
  set %expline 9
  set %expcol 8
  set %expchar 10
execute-procedure check-position

; Expect to be beyond fill - in the kept spaces
previous-line
end-of-line
backward-character
  set %curtest Line8-OrigWrap-endprev
  set %expline 8
  set %expcol 59
  set %expchar &asc " "
execute-procedure check-position
unmark-buffer

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Insert space in the space-gap on line 8
; Note that these behave quite differently.
; FullWrap leaves you at the end of the line where you were typing.
; OrigWrap puts you on the next line, after a space in the first column.
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
beginning-of-line
61 forward-character
2 wrap-word
insert-string " "
  set %curtest Line8-FullWrap-MidSpace
  set %expline 8
  set %expcol 58
  set %expchar 10
execute-procedure check-position

next-line
beginning-of-line
; Expect to be at start of the wrapped "column."
  set %curtest Line8-Fullwrap-MSnextstart
  set %expline 9
  set %expcol 0
  set %expchar &asc c
execute-procedure check-position

; Re-read file...
unmark-buffer
read-file autotest.tfile

8 goto-line
beginning-of-line
61 forward-character
wrap-word
insert-string " "
  set %curtest Line8-OrigWrap-MidSpace
  set %expline 9
  set %expcol 1
  set %expchar &asc c
execute-procedure check-position

previous-line
end-of-line
backward-character
; Expect to be at the end of the spaces
  set %curtest Line8-Origwrap-MSprevend
  set %expline 8
  set %expcol 59
  set %expchar &asc " "
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Wrap on a zero-width break
;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
2 wrap-word
insert-string " "
  set %curtest Line12-FullWrap-zwb
  set %expline 13
  set %expcol 8
  set %expchar 10
execute-procedure check-position

beginning-of-line
; Expect to be at start of the wrapped text - looking at the zwb.
  set %curtest Line12-Fullwrap-bol
  set %expline 13
  set %expcol 0
; Convenient way to get a hex value in!
  set %expchar &band 0x200b 0xffff
execute-procedure check-position

;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
wrap-word
insert-string " "
  set %curtest Line12-OrigWrap-zwb
  set %expline 13
  set %expcol 8
  set %expchar 10
execute-procedure check-position

beginning-of-line
; Expect to be at start of the wrapped text - looking at the zwb.
  set %curtest Line12-Origwrap-bol
  set %expline 13
  set %expcol 0
  set %expchar &band 0x200b 0xffff
execute-procedure check-position

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
2 wrap-word
insert-string " "
  set %curtest Line12+-FullWrap
  set %expline 13
  set %expcol 14
  set %expchar 10
execute-procedure check-position

beginning-of-line
; Expect to be at start of the wrapped text - looking at the zwb.
  set %curtest Line12+-Fullwrap-bol
  set %expline 13
  set %expcol 0
; Convenient way to get a hex value in!
  set %expchar &band 0x200b 0xffff
execute-procedure check-position

;
; Re-read file...
unmark-buffer
read-file autotest.tfile

12 goto-line
end-of-line
insert-string " xyzzy"
wrap-word
insert-string " "
  set %curtest Line12-OrigWrap-zwb
  set %expline 13
  set %expcol 6
  set %expchar 10
execute-procedure check-position

beginning-of-line
; Expect to be at start of the wrapped xyzzy
  set %curtest Line12-Origwrap-bol
  set %expline 13
  set %expcol 0
  set %expchar &asc x
execute-procedure check-position

unmark-buffer

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
-2 redraw-display
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc
