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
01 #if it_works
02     we should get here
03     and here
04 #endif
05     and stop after this...
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

set %test-report "START: Zero length magic repeat matches"
execute-procedure report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a zero-length match at the start of line 1
;
set %test-report "  ^[^#]* - search 1"
execute-procedure report-status

search-forward ^[^#]*
  set %curtest Search1
  set %expline 1
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 0
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 2-line + newline at the start of line 2
;
set %test-report "  ^[^#]* - re-search 2"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search2
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 36
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 1-line + newline at the start of line 3
;
set %test-report "  ^[^#]* - re-search 3"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search3
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 13
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a zero-length match at the start of line 4
;
set %test-report "  ^[^#]* - re-search 4"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search4
  set %expline 4
  set %expcol 1
  set %expchar &asc "#"
  set %expmatchlen 0
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should get a 1-line + newline match at the start of line 5
;
set %test-report "  ^[^#]* - re-search 5"
execute-procedure report-status

set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
hunt-forward
  set %curtest Search5
  set %expline 6
  set %expcol 1
  set %expchar 10
  set %expmatchlen 27
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  ^[^#]* - re-search 5"
execute-procedure report-status

; This should not pass!!!
set $srch_can_hunt 1    ; OK, even though we have switched buffer and back
!force hunt-forward
!if &seq $force_status PASSED
    set %fail &add %fail 1
    set .res "PASSED!"
!else
    set %ok &add %ok 1
    set .res "failed, as required"
!endif

set %test-report &cat "Expected FAILing search " .res
execute-procedure report-status

;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
-2 redraw-display
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
