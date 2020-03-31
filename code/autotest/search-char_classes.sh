#!/bin/sh
#

# Simple testing of Magic-mode Character Classes

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
; START running the code!
find-file autotest.tfile
add-mode Magic

set %test-report "START: Various Character Class tests"
execute-procedure report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ] in position 1 (start of range)"
execute-procedure report-status
beginning-of-file
; ====
search-forward []-c]
  set %curtest Search1
  set %expline 1
  set %expcol 2
  set %expchar &asc t
  set %expmatch a
execute-procedure check-position
search-forward []-c]+
  set %curtest Search2
  set %expline 1
  set %expcol 4
  set %expchar &asc h
  set %expmatch c
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ] in position 1 (literal)"
execute-procedure report-status
beginning-of-file
; ====
search-forward []nq]
  set %curtest Search1
  set %expline 1
  set %expcol 9
  set %expchar &asc x
  set %expmatch ]
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - in position 1 (literal)"
execute-procedure report-status
beginning-of-file
; ====
search-forward [-xyz]
  set %curtest Search1
  set %expline 1
  set %expcol 7
  set %expchar &asc " "
  set %expmatch -
execute-procedure check-position
search-forward [-xyz]+
  set %curtest Search2
  set %expline 1
  set %expcol 14
  set %expchar &asc [
  set %expmatch xyzzy
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; This should do the same as the above - the "-" has moved, but the
; effect should be indentical.
set %test-report "   - in final position (literal)"
execute-procedure report-status
beginning-of-file
; ====
search-forward [xyz-]
  set %curtest Search1
  set %expline 1
  set %expcol 7
  set %expchar &asc " "
  set %expmatch -
execute-procedure check-position
search-forward [xyz-]+
  set %curtest Search2
  set %expline 1
  set %expcol 14
  set %expchar &asc [
  set %expmatch xyzzy
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - as range"
execute-procedure report-status
beginning-of-file
; ====
search-forward [x-z]
  set %curtest Search1
  set %expline 1
  set %expcol 10
  set %expchar &asc y
  set %expmatch x
execute-procedure check-position
search-forward [x-z]+
  set %curtest Search2
  set %expline 1
  set %expcol 14
  set %expchar &asc [
  set %expmatch yzzy
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   - as range - inverted"
execute-procedure report-status
beginning-of-file
; ====
search-forward [^x-z]
  set %curtest Search1
  set %expline 1
  set %expcol 1
  set %expchar &asc a
  set %expmatch m
execute-procedure check-position
search-forward [^x-z]+
  set %curtest Search2
  set %expline 1
  set %expcol 9
  set %expchar &asc x
  set %expmatch "atch - ]"
execute-procedure check-position


;
select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
