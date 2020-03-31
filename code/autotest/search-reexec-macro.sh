#!/bin/sh
#

# Simple test that using search with a reexeced macro searches
# for the given string, not a rexec of the previous search.
#

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
01 match - ]xyzzy[
02 EOF
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
  insert-raw-string %test-report
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
; %expmatch     the text of the match
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
  set  %test-report &cat %test-report &cat " (match: " &cat $match ")"
  execute-procedure report-status

  !if &seq $match %expmatch
    set %test-report &cat %curtest &cat " - matched OK: " %expmatch
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - match WRONG, got: " $match
    set %test-report &cat %test-report &cat " expected: " %expmatch
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-group
;   This expected value must be set, since this tests it.
; %grpno            group to test
; %expmatch         group text
  !if &seq %expmatch &grp %grpno
    set %test-report &cat %curtest &cat " group " &cat %grpno " OK"
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " group " &cat %grpno " WRONG! got: " &cat &grp %grpno &cat " expected: " %expmatch
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-count
;   This expected value must be set, since this tests it.
; %expcalls         call count we should have reached
  !if &equ %expcalls %calls
    set %test-report &cat %curtest &cat " calls " &cat %calls " OK"
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " calls WRONG! got: " &cat %calls &cat " expected: " %expcalls
    set %fail &add %fail 1 
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
find-file autotest.tfile
add-mode Exact

set %test-report "START: Various Character Class tests"
execute-procedure report-status

set %calls 0
store-procedure tester
    set %test-report "Entering tester"
    execute-procedure report-status
    search-forward xyzzy
    search-reverse atc
    set %calls &add %calls 1
    set %test-report "Leaving tester"
    execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   search in reexec macro"
execute-procedure report-status
beginning-of-file

execute-procedure tester
  set %curtest "Run tester"
  set %expline 1
  set %expcol 1
  set %expchar &asc a
  set %expmatch atc
execute-procedure check-position
  set %expcalls 1
execute-procedure check-count

execute-procedure tester
  set %curtest "Run tester again"
  set %expline 1
  set %expcol 1
  set %expchar &asc a
  set %expmatch atc
execute-procedure check-position
  set %expcalls 2
execute-procedure check-count

execute-procedure tester    ; Must run it again to be able rexecute it!
2 reexecute
  set %curtest "Run tester again"
  set %expline 1
  set %expcol 1
  set %expchar &asc a
  set %expmatch atc
execute-procedure check-position
  set %expcalls 5
execute-procedure check-count


select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
