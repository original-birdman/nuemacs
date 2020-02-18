#!/bin/sh
#

# Test for Unicode searching.

# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

awk "$prog" > magic2.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03   (but the ὐ has no upper-case equiv, so stays in lowercase!!!)
04
05 Now some Russian сейчас numbers 77712 (arabic ٧٧٧١٢) [Devenagari ७७७१२]
06 In UPPER СЕЙЧАС NUMBERS 77712 (ARABIC ٧٧٧١٢) [DEVENAGARI ७७७१२]
07
08 Can this be found case-sensitive and insensitive?
-- 0123456789012345678901234567890123456789012345678901234567890123456789
09
10 οὐδένmañanaСЕЙЧАС
11 οὐδένmañanaсейчас
12 ΟὐΔΈΝMAÑANAСЕЙЧАС
13
14 And don't forget that I need to check that a match (ANY MATCH!) isn't
15 followed by a combining character (which would raise odd issues for
16 replace). Some more numbers + text 912 TEST.
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
  1 select-buffer     ; Back to buffer from whence we came
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

; Open the test file
;
find-file magic2.tfile

set %test-report "START: Various Magic property tests"
execute-procedure report-status

beginning-of-file
add-mode Exact
add-mode Magic

set %test-report "  Upper, punctuation - \p{Lu}{3}\p{P}"
execute-procedure report-status
; ====
search-forward \p{Lu}{3}\p{P}   ; should match ΔΈΝ. ...
  set %curtest Search1
  set %expline 2
  set %expcol 15
  set %expchar 10
  set %expmatch ΔΈΝ.
execute-procedure check-position
; ====
search-forward \p{Lu}{3}\p{P}   ; ...now TCH!
  set %curtest Search2
  set %expline 14
  set %expcol 62
  set %expchar &asc )
  set %expmatch TCH!

execute-procedure check-position

set %test-report "  !upper, space, 3 numbers - \P{Lu} \p{N}{3}"
execute-procedure report-status
beginning-of-file
; ====
search-forward "\P{Lu} \p{N}{3}"   ; NOT upper letter, space, 3 numbers...
  set %curtest Search1
  set %expline 5
  set %expcol 35
  set %expchar &asc 1
  set %expmatch "s 777"
execute-procedure check-position
; ====
search-forward "\P{Lu} \p{N}{3}"
  set %curtest Search2
  set %expline 5
  set %expcol 49
  set %expchar &asc ١
  set %expmatch "c ٧٧٧"
execute-procedure check-position
; ====
search-forward "\P{Lu} \p{N}{3}"
  set %curtest Search3
  set %expline 5
  set %expcol 68
  set %expchar &asc १
  set %expmatch "i ७७७"
execute-procedure check-position

set %test-report "  lower, anything, Upper - \p{Ll}\X\p{LU}
execute-procedure report-status
beginning-of-file
; ====
search-forward "\p{Ll}\X\p{LU}" ;
  set %curtest Search1
  set %expline 2
  set %expcol 1
  set %expchar &asc n
  set %expmatch ν~nI
execute-procedure check-position
search-forward "\p{Ll}\X\p{LU}"
  set %curtest Search2
  set %expline 2
  set %expcol 4
  set %expchar &asc P
  set %expmatch "n U"
execute-procedure check-position
search-forward "\p{Ll}\X\p{LU}"
  set %curtest Search3          ; As ὐ doesn't Uppercase...
  set %expline 2
  set %expcol 13
  set %expchar &asc Ν           ; Capital Greek Nu
  set %expmatch ὐΔΈ
execute-procedure check-position

set %test-report "  \u{03b4}"
execute-procedure report-status
; Continue from current position
search-forward "\u{03b4}"
  set %curtest Search1
  set %expline 10
  set %expcol 3
  set %expchar &asc έ
  set %expmatch δ
execute-procedure check-position
search-forward "\u{03b4}"
  set %curtest Search2
  set %expline 11
  set %expcol 3
  set %expchar &asc έ
  set %expmatch δ
execute-procedure check-position
!force search-forward "\u{03b4}"    ; Check it fails
  set %curtest Search3
  set %expline 11
  set %expcol 3
  set %expchar &asc έ
  set %expmatch "ΔΈΝ."
  set %expmatch δ
execute-procedure check-position

set %test-report "  \u{03b4}, not Exact"
execute-procedure report-status
; Continue from current position
delete-mode Exact                   ; For one more success
search-forward "\u{03b4}"
  set %curtest Search4-e
  set %expline 12
  set %expcol 3
  set %expchar &asc Έ
  set %expmatch Δ
execute-procedure check-position
set %test-report "  \u{03b4}, not Exact, reversed"
execute-procedure report-status
; Continue from current position
search-reverse "\u{03b4}"           ; Check a back search...
  set %curtest BackSearch4-e
  set %expline 12
  set %expcol 2
  set %expchar &asc Δy
  set %expmatch Δ
execute-procedure check-position

select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc
