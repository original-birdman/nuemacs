#!/bin/sh
#

# Test for Unicode searching.

# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# Need the -b because of the non-Unicode (invalid) 0xf1 bytes.
#
awk -b "$prog" > test1.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03
04 Now some Russian сейчас
05 In UPPER СЕЙЧАС
06
07 Different sets that look the same
08 The first is ASCII n (0x6e) + a combining diacritic ~ (U+0303 = 0xcc 0x83)
09 The second is a utf-8 small n with tilde (U+00F1 = 0xc3 0xb1).
10 The third is Latin-1 (0xf1) (non-Unicode...)
-- 0123456789012345678901234567890123456789012345678901234567890123456789
11
12   mañana  mañana  mañana  mañana
13   mañana  mañana  mañana  mañana
14   ma�ana  ma�ana  ma�ana  ma�ana
15
16   déjà vu   déjà vu   déjà vu   déjà
17
18   Here's one at the end of a line - touché
19
20 Can this be found case-sensitive and insensitive?
-- 0123456789012345678901234567890123456789012345678901234567890123456789
21
22 οὐδένmañanaСЕЙЧАС
23 οὐδένmañanaсейчас
24 ΟὐΔΈΝMAÑANAСЕЙЧАС
25
26 And don't forget that I need to check that a match (ANY MATCH!) isn't
27 followed by a combining character (which would raise odd issues for
28 replace).
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

find-file test1.tfile

set %test-report "START: Various search tests"
execute-procedure report-status

set %test-report "   δένmañanaСЕ searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic

; ====
search-forward δένmañanaСЕ
  set %curtest Search1
  set %expline 22
  set %expcol 13
  set %expchar &asc Й
  set %expmatch δένmañanaСЕ
execute-procedure check-position
; ====
; Force this. It should fail, so not move anything...
!force search-forward δένmañanaСЕ
  set %curtest Search2
  set %expline 22
  set %expcol 13
  set %expchar &asc Й
  set %expmatch δένmañanaСЕ
execute-procedure check-position

; Now turn off Exact mode - should get two more search successes
delete-mode Exact

set %test-report "   δένmañanaСЕ searches - no Exact"
execute-procedure report-status
; ====
search-forward δένmañanaСЕ
  set %curtest Search2-e
  set %expline 23
  set %expcol 13
  set %expchar &asc й
  set %expmatch δένmañanaсе
execute-procedure check-position
; ====
search-forward δένmañanaСЕ
  set %curtest Search3-e
  set %expline 24
  set %expcol 13
  set %expchar &asc Й
  set %expmatch ΔΈΝMAÑANAСЕ
execute-procedure check-position
; ====
; Force this. It should fail, so not move anything...
!force search-forward δένmañanaСЕ
  set %curtest Search4-e
  set %expline 24
  set %expcol 13
  set %expchar &asc Й
  set %expmatch ΔΈΝMAÑANAСЕ
execute-procedure check-position
; ====
search-reverse δένmañanaСЕ
  set %curtest BackSearch4-e
  set %expline 24
  set %expcol 2
  set %expchar &asc Δ
  set %expmatch ΔΈΝMAÑANAСЕ
execute-procedure check-position
; ====
search-reverse δένmañanaСЕ
  set %curtest Search2-e
  set %expline 23
  set %expcol 2
  set %expchar &asc δ
  set %expmatch δένmañanaсе
execute-procedure check-position


select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
