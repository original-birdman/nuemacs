#!/bin/sh
#

# Simple testing of Magic-mode properties (part2)

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
01 Not sure whether the best test of CCLs is to have lots
02 of alternating classes or not?
03 Here are some numbers 0123456789
04 Here, numbers interspersed with differing punctuation (all Po):
05      0,1.2;3:4'5!6*7@8?9
06 Now some other P classes (PePs)*
07      ][}{)(
08 Some symbols (all Sm)
09  +<=>|~¬±𝝯𝞉
10 And some Letter Modifiers...
11      ˆˈˊˌˎː
12 So now a sequence of Lm Sm Ps Pe Nd Po Ll Lu, then reversed
13      ˊ±{]3@zQ
14      Td:8)[|ˆ
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
; START running the code!
find-file autotest.tfile
add-mode Magic

set %test-report "START: Various property tests (part 2)"
execute-procedure report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   \p{Nd}\p{Po} searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest Search1
  set %expline 5
  set %expcol 11
  set %expchar &asc 3
  set %expmatch 0,1.2;
execute-procedure check-position
search-forward \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest Search2
  set %expline 5
  set %expcol 19
  set %expchar &asc 7
  set %expmatch 3:4'5!6*
execute-procedure check-position

set %test-report "  \P searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward 789\n
  set %curtest "Placing search"
  set %expline 4
  set %expcol 0
  set %expchar &asc H
  set %expmatch 789~n
execute-procedure check-position
; ====
search-forward \P{L}
  set %curtest Search not Letter 
  set %expline 4
  set %expcol 5
  set %expchar &asc " "
  set %expmatch ,
execute-procedure check-position

set %test-report "  \p{SM}{5} search"
execute-procedure report-status
beginning-of-file
; ====
search-forward \p{SM}{5}
  set %curtest Search1
  set %expline 9
  set %expcol 6
  set %expchar &asc ~~
  set %expmatch +<=>|
execute-procedure check-position

set %test-report "  6-class search"
execute-procedure report-status
beginning-of-file
; ====
search-forward \p{Lm}\p{Sm}\p{Ps}\p{Pe}\p{Nd}\p{Po}
  set %curtest Search1
  set %expline 13
  set %expcol 11
  set %expchar &asc z
  set %expmatch ˊ±{]3@
execute-procedure check-position
end-of-file
; ====
search-reverse \p{Lu}\p{ll}\p{Po}\p{Nd}\p{Pe}\p{Ps}
  set %curtest "Reverse search"
  set %expline 14
  set %expcol 5
  set %expchar &asc T
  set %expmatch Td:8)[
execute-procedure check-position

set %test-report "  negative reverse search"
execute-procedure report-status
; ====
!force search-reverse \p{M}
  set %curtest "Reverse search for Mark (none there)"
  set %expline 14
  set %expcol 5
  set %expchar &asc T
  set %expmatch ""
execute-procedure check-position

;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
