#!/bin/sh
#

# Simple testing repeating a zero-length match in Magic mode

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
awk "$prog" > autotest.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
01 Entered some silly text which allows us to
02 look for s then i then m meaning we need to jump
03 each time.
04 It's quite simple.
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $matchlen are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to whence we came
!endm

set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   Expects these to have been set, since it tests them all.
; %expline      the line for the match
; %expcol       the column for the match
; %expchar      the expected "char" at point (R/h side) at the match
; %expmatchlen  the text length of the match
;
  !if &equ $curline %expline
    set %test-report &cat %curtest &cat " - line OK: " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG line, got: " $curline
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curcol %expcol
    set %test-report &cat %curtest &cat " - column OK: " $curcol
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG column, got: " $curcol
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

; NOTE!!! We just check the length here as we expect multi-line matches!
;
  !if &equ &len $match %expmatchlen
    set %test-report &cat %curtest &cat " - matched OK: " %expmatchlen
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - match WRONG, got: " &len $match
    set %test-report &cat %test-report &cat " expected: " %expmatchlen
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Incremental search testing"
execute-procedure report-status

beginning-of-file
add-mode Exact

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward incremental search
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; We need to set up individual checking functions for each test
; as we don't get to change expected values anywhere else;
;
store-procedure check1
  set %test-report "  search for s"
  execute-procedure report-status
  set %curtest Search-s
  set %expline 1
  set %expcol 9
  set %expchar &asc "o"
  set %expmatchlen 1
  execute-procedure check-position
!endm
store-procedure check2
  set %test-report "  search for i"
  execute-procedure report-status
  set %curtest Search-i
  set %expline 1
  set %expcol 15
  set %expchar &asc "l"
  set %expmatchlen 2
  execute-procedure check-position
!endm
store-procedure check3
  set %test-report "  search for m"
  execute-procedure report-status
  set %curtest Search-m
  set %expline 4
  set %expcol 14
  set %expchar &asc "p"
  set %expmatchlen 3
  execute-procedure check-position
!endm

; Now set-up the control buffer
; 
select-buffer //incremental-debug
insert-string "s check1"
next-line
insert-string "i check2"
next-line
insert-string "m check3"
next-line
insert-tokens 0x03
2 select-buffer

; Now run the search, which will produce the report.
;
incremental-search

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Now we do the same search in reverse from the end of the file.
; We find the same match, but in a different manner.
; Note that the position doesn't change, just the matchlen
;
; Replace the forward test results
store-procedure check1
  set %test-report "  search for s"
  execute-procedure report-status
  set %curtest Search-s
  set %expline 4
  set %expcol 11
  set %expchar &asc "s"
  set %expmatchlen 1
  execute-procedure check-position
!endm
store-procedure check2
  set %test-report "  search for i"
  execute-procedure report-status
  set %curtest Search-i
  set %expline 4
  set %expcol 11
  set %expchar &asc "s"
  set %expmatchlen 2
  execute-procedure check-position
!endm
store-procedure check3
  set %test-report "  search for m"
  execute-procedure report-status
  set %curtest Search-m
  set %expline 4
  set %expcol 11
  set %expchar &asc "s"
  set %expmatchlen 3
  execute-procedure check-position
!endm

; The test buffer is the same as for the forward search.
;
select-buffer //incremental-debug
insert-string "s check1"
next-line
insert-string "i check2"
next-line
insert-string "m check3"
next-line
insert-tokens 0x03
2 select-buffer

end-of-file
reverse-incremental-search

;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc