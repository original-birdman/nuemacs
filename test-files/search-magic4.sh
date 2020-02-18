#!/bin/sh
#

# Simple testing of Character Classes within Closures

# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

awk "$prog" > magic4.tfile <<EOD
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

cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $match  are what I expect them to be.
;
store-procedure report-status
  select-buffer test-reports
  insert-raw-string %test-report
  newline
  1 select-buffer     ; Back to whence we came
!endm

set %fail 0
set %ok 0

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
    set %fail &add %fail 1
  !endif
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

find-file magic4.tfile
add-mode Magic

set %test-report "START: Various Character Class/Closure tests"
execute-procedure report-status

beginning-of-file
set %test-report "   [\p{Nd}\p{Po}]* searches"
execute-procedure report-status
; ====
search-forward [\p{Nd}\p{Po}]*\X\n
  set %curtest [\p{Nd}\p{Po}]*\X\n
  set %expline 2
  set %expcol 0
  set %expchar &asc o
  set %expmatch s~n
execute-procedure check-position
; ====
search-forward [\p{Nd}\p{Po}]+\X\n      ; Don't test this one...
search-forward [\p{Nd}\p{Po}]+\X\n
  set %curtest [\p{Nd}\p{Po}]+\X\n
  set %expline 6
  set %expcol 0
  set %expchar &asc N
  set %expmatch 0,1.2;3:4'5!6*7@8?9~n
execute-procedure check-position
; ====
search-reverse \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest "Search back for NPNPNPNP"
  set %expline 5
  set %expcol 15
  set %expchar &asc 5
  set %expmatch 5!6*7@8?
execute-procedure check-position

;
select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
