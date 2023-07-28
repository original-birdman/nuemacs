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
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Text for replacement text - query/interactive version
02 Text should contain a few instances of "ext" and 
03 we'll change most, but not all, of them to !!!!.
04 So we'll end up with 7!s here ext!!!.
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

set %test_name query-replace

select-buffer test-reports
insert-string &cat %test_name " started"
newline
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

set %test-report "START: Query replace testing"
execute-procedure report-status

beginning-of-file
add-mode Exact

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward query-replace (there *IS* no reverse one).
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; We need to set up individual checking functions for each test
; as we don't get to change expected values anywhere else;
;
store-procedure check1
  set %test-report "  replace 1"
  execute-procedure report-status
  set %curtest Replace-Yes
  set %expline 1
  set %expcol 2
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position
!endm
store-procedure check2
  set %test-report "  replace 2"
  execute-procedure report-status
  set %curtest Replace-No
  set %expline 1
  set %expcol 24
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position
!endm
store-procedure check3
  set %test-report "  replace 3"
  execute-procedure report-status
  set %curtest replace-Yes
  set %expline 2
  set %expcol 2
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position
!endm
store-procedure check4
  set %test-report "  replace 4"
  execute-procedure report-status
  set %curtest Replace-No
  set %expline 2
  set %expcol 42
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position
!endm
store-procedure check5
  set %test-report "  replace 5"
  execute-procedure report-status
  set %curtest Replace-ALL
  set %expline 4
  set %expcol 31
  set %expchar &asc "e"
  set %expmatchlen 3
  execute-procedure check-position
!endm
; After it completes we will be after the last replaced string
; but the match will be invalidated.
store-procedure check6
  set %test-report "  replace 6"
  execute-procedure report-status
  set %curtest Replace-ALL
  set %expline 4
  set %expcol 35
  set %expchar &asc "!"
  set %expmatchlen 0
  execute-procedure check-position
!endm

; Now set-up the control buffer
; 
select-buffer //incremental-debug
insert-string "Y check1"
next-line
insert-string "N check2"
next-line
insert-string "Y check3"
next-line
insert-string "N check4"
next-line
insert-string "! check5"
2 select-buffer

; Now run the search, which will produce the report.
; Not we replace with a longer string
;
query-replace-string "ext" "!!!!"
; Run the final test - query replace has exited....
execute-procedure check6
unmark-buffer
;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
unmark-buffer
-2 redraw-display
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
