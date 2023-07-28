#!/bin/sh
#

# Test for Unicode searching.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# Need the -b because of the non-Unicode (invalid) 0xf1 bytes.
#

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
gawk -b $AWKARG "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
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
-- 123456789012345678901234567890123456789012345678901234567890123456789
11
12   mañana  mañana  mañana  mañana
13   mañana  mañana  mañana  mañana
14   ma�ana  ma�ana  ma�ana  ma�ana
15
16   déjà vu   déjà vu   déjà vu   déjà
17
18   Here's one at the end of a line - touché
19
20 Can these be found case-sensitive and insensitive?
-- 123456789012345678901234567890123456789012345678901234567890123456789
21
22 οὐδένmañanaСЕЙЧАС
23 οὐδένmañanaсейчас
24 ΟὐΔΈΝMAÑANAСЕЙЧАС
25
26 And here's a list of active Magic characters, which should all be
27 taken literally.
28 
29 (a|b)*\s\p{L}[]??END
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $match are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to whence we came
!endm

set %test_name search-no_magic

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   These expected values must be set, since this tests them all.
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

set %test-report "START: Various search tests"
execute-procedure report-status

; ==== Run in EXACT mode
;
set %test-report "   δένmañanaСЕ searches - EXACT"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic

set %mcount 0
*again1
  !force search-forward δένmañanaСЕ
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search1
  set %expline 22
  set %expcol 14
  set %expchar &asc Й
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 1
execute-procedure check-matchcount
; Reverse search and check match
  search-reverse δένmañanaСЕ
  set %curtest Search1-reversed
  set %expline 22
  set %expcol 3
  set %expchar &asc δ
  set %expmatch δένmañanaСЕ
execute-procedure check-position

; ==== Re-run in non-EXACT mode
;
set %test-report "   δένmañanaСЕ searches - no EXACT"
execute-procedure report-status
beginning-of-file
delete-mode Exact
delete-mode Magic

set %mcount 0
*again2
  !force search-forward δένmañanaСЕ
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search2
  set %expline 24
  set %expcol 14
  set %expchar &asc Й
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 3
execute-procedure check-matchcount
; Reverse search and check match
  search-reverse δένmañanaСЕ
  set %curtest Search2-reversed
  set %expline 24
  set %expcol 3
  set %expchar &asc Δ
  set %expmatch ΔΈΝMAÑANAСЕ
execute-procedure check-position

; ==== Check that Magic active characters are taken literally
;
set %test-report "   Magic active characters are taken literally"
execute-procedure report-status
beginning-of-file
search-forward (a|b)*\s\p{L}[]??E
; Where did we end up, and wat did we match?
  set %curtest Search3
  set %expline 29
  set %expcol 19
  set %expchar &asc N
  set %expmatch (a|b)*\s\p{L}[]??E
execute-procedure check-position

; Show the report...
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
