#!/bin/sh
#

# Simple testing for combining diacriticals starting a line
# i.e. having nothing to combine with
# (In any other position they will combine with the previous char
#  Trying to add a combining diacritical to a newline will result in it
#  starting the next line, as the line ends as soon as the NL is seen.).

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

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
    set %test-report &cat %test-report &cat " expected: " %expchar
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - at WRONG char, got: " %pchar
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

!endm

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
unmark-buffer
-2 redraw-display
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc
