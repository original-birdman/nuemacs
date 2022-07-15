#!/bin/sh
#

# Simple testing of *, + {m,n} and ? magic-mode operators.

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
01 Simple testing for */+/{m,n} handling.
02 acc, adc, a*c, a+c.
03 
04 ac
05 abc
06 abbc
07 abbbc
08 abbbbc
09 
10 Some whitespace tests too
11 
12 a 	
13 c
14 
15 EOF
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
  1 select-buffer     ; Back to whence we came
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
store-procedure check-matchcount
;   This expected value must be set, since this tests it.
; %expcount         number of successful matches
  !if &equ %mcount %expcount
    set %test-report &cat %curtest &cat " match count OK: " %mcount
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " match count WRONG, got: " %mcount
    set %test-report &cat %test-report &cat " - expected: " %expcount
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various magic repeating tests"
execute-procedure report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab*c searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again1
  !force search-forward ab*c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search1
  set %expline 10
  set %expcol 14
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 7
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse ab*c
  set %curtest Search1-reversed
  set %expline 10
  set %expcol 12
  set %expchar &asc a
  set %expmatch ac
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab*?c searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again2
  !force search-forward ab*?c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
; Should be the same as Search1
  set %curtest Search2
  set %expline 10
  set %expcol 14
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 7
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse ab*?c
  set %curtest Search2-reversed
  set %expline 10
  set %expcol 12
  set %expchar &asc a
  set %expmatch ac
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab{2,4}c searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again3
  !force search-forward ab{2,4}c
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again3
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search3
  set %expline 8
  set %expcol 6
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 3
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse ab{2,4}c
  set %curtest Search3-reversed
  set %expline 8
  set %expcol 0
  set %expchar &asc a
  set %expmatch abbbbc
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab+ searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again4
  !force search-forward ab+
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again4
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search4
  set %expline 8
  set %expcol 5
  set %expchar &asc c
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 4
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse ab+
  set %curtest Search4-reversed
  set %expline 8
  set %expcol 0
  set %expchar &asc a
  set %expmatch abbbb
execute-procedure check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   ab+? searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
add-mode Magic

set %mcount 0
*again5
  !force search-forward ab+?
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again5
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search5
  set %expline 8
  set %expcol 2
  set %expchar &asc b
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 4
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse ab+?
  set %curtest Search4-reversed
  set %expline 8
  set %expcol 0
  set %expchar &asc a
  set %expmatch ab
execute-procedure check-position

;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
