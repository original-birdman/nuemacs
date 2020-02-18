#!/bin/sh
#

# Simple testing of *, + {m,n} and ? magic-mode operators.

# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

awk "$prog" > magic1.tfile <<EOD
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

find-file magic1.tfile
add-mode Magic

set %test-report "START: Various magic tests"
execute-procedure report-status

set %test-report "   ab*c searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward ab*c
  set %curtest Search1
  set %expline 2
  set %expcol 2
  set %expchar &asc c
  set %expmatch ac
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search2
  set %expline 4
  set %expcol 2
  set %expchar 10
  set %expmatch ac
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search3
  set %expline 5
  set %expcol 3
  set %expchar 10
  set %expmatch abc
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search4
  set %expline 6
  set %expcol 4
  set %expchar 10
  set %expmatch abbc
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search5
  set %expline 7
  set %expcol 5
  set %expchar 10
  set %expmatch abbbc
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search6
  set %expline 8
  set %expcol 6
  set %expchar 10
  set %expmatch abbbbc
execute-procedure check-position
; ====
search-forward ab*c
  set %curtest Search7
  set %expline 10
  set %expcol 14
  set %expchar &asc e
  set %expmatch ac
execute-procedure check-position

set %test-report "   ab*?c searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward ab*?c
  set %curtest Search1
  set %expline 2
  set %expcol 2
  set %expchar &asc c
  set %expmatch ac
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search2
  set %expline 4
  set %expcol 2
  set %expchar 10
  set %expmatch ac
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search3
  set %expline 5
  set %expcol 3
  set %expchar 10
  set %expmatch abc
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search4
  set %expline 6
  set %expcol 4
  set %expchar 10
  set %expmatch abbc
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search5
  set %expline 7
  set %expcol 5
  set %expchar 10
  set %expmatch abbbc
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search6
  set %expline 8
  set %expcol 6
  set %expchar 10
  set %expmatch abbbbc
execute-procedure check-position
; ====
search-forward ab*?c
  set %curtest Search7
  set %expline 10
  set %expcol 14
  set %expchar &asc e
  set %expmatch ac
execute-procedure check-position

set %test-report "   ab{2,4}c searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward ab{2,4}c
  set %curtest Search1
  set %expline 6
  set %expcol 4
  set %expchar 10
  set %expmatch abbc
execute-procedure check-position
; ====
search-forward ab{2,4}c
  set %curtest Search2
  set %expline 7
  set %expcol 5
  set %expchar 10
  set %expmatch abbbc
execute-procedure check-position
; ====
search-forward ab{2,4}c
  set %curtest Search3
  set %expline 8
  set %expcol 6
  set %expchar 10
  set %expmatch abbbbc
execute-procedure check-position

set %test-report "   ab+ searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward ab+
  set %curtest Search1
  set %expline 5
  set %expcol 2
  set %expchar &asc c
  set %expmatch ab
execute-procedure check-position
; ====
search-forward ab+
  set %curtest Search2
  set %expline 6
  set %expcol 3
  set %expchar &asc c
  set %expmatch abb
execute-procedure check-position
; ====
search-forward ab+
  set %curtest Search3
  set %expline 7
  set %expcol 4
  set %expchar &asc c
  set %expmatch abbb
execute-procedure check-position
; ====
search-forward ab+
  set %curtest Search4
  set %expline 8
  set %expcol 5
  set %expchar &asc c
  set %expmatch abbbb
execute-procedure check-position

set %test-report "   ab+? searches"
execute-procedure report-status
beginning-of-file
; ====
search-forward ab+?
  set %curtest Search1
  set %expline 5
  set %expcol 2
  set %expchar &asc c
  set %expmatch ab
execute-procedure check-position
; ====
search-forward ab+?
  set %curtest Search2
  set %expline 6
  set %expcol 2
  set %expchar &asc b
  set %expmatch ab
execute-procedure check-position
; ====
search-forward ab+?
  set %curtest Search3
  set %expline 7
  set %expcol 2
  set %expchar &asc b
  set %expmatch ab
execute-procedure check-position
; ====
search-forward ab+?
  set %curtest Search4
  set %expline 8
  set %expcol 2
  set %expchar &asc b
  set %expmatch ab
execute-procedure check-position

set %test-report "   Reverse ab+? searches"
execute-procedure report-status
end-of-file
; ====
search-reverse ab+?
  set %curtest Search1
  set %expline 8
  set %expcol 0
  set %expchar &asc a
  set %expmatch ab
execute-procedure check-position
; ====
search-reverse ab+?
  set %curtest Search2
  set %expline 7
  set %expcol 0
  set %expchar &asc a
  set %expmatch ab
execute-procedure check-position
; ====
search-reverse ab+?
  set %curtest Search3
  set %expline 6
  set %expcol 0
  set %expchar &asc a
  set %expmatch ab
execute-procedure check-position
; ====
search-reverse ab+?
  set %curtest Search4
  set %expline 5
  set %expcol 0
  set %expchar &asc a
  set %expmatch ab
execute-procedure check-position

;
select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
