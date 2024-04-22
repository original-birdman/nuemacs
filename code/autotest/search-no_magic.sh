#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Test for Unicode searching.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
if type perl >/dev/null 2>&1; then
    prog='next if (/^--/); chomp; print substr($_, 3);'
    cmd="perl -lne"
else
    prog='$1 != "--" {print substr($0, 4);}'
    cmd=awk
fi

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
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

execute-file autotest/report-status.rc

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Load the check routine
;
execute-file autotest/check-position-match.rc

execute-file autotest/check-matchcount.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various search tests"
run report-status

; ==== Run in EXACT mode
;
set %test-report "   δένmañanaСЕ searches - EXACT"
run report-status
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
run check-position-match
  set %expcount 1
run check-matchcount
; Reverse search and check match
  search-reverse δένmañanaСЕ
  set %curtest Search1-reversed
  set %expline 22
  set %expcol 3
  set %expchar &asc δ
  set %expmatch δένmañanaСЕ
run check-position-match

; ==== Re-run in non-EXACT mode
;
set %test-report "   δένmañanaСЕ searches - no EXACT"
run report-status
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
run check-position-match
  set %expcount 3
run check-matchcount
; Reverse search and check match
  search-reverse δένmañanaСЕ
  set %curtest Search2-reversed
  set %expline 24
  set %expcol 3
  set %expchar &asc Δ
  set %expmatch ΔΈΝMAÑANAСЕ
run check-position-match

; ==== Check that Magic active characters are taken literally
;
set %test-report "   Magic active characters are taken literally"
run report-status
beginning-of-file
search-forward (a|b)*\s\p{L}[]??E
; Where did we end up, and wat did we match?
  set %curtest Search3
  set %expline 29
  set %expcol 19
  set %expchar &asc N
  set %expmatch (a|b)*\s\p{L}[]??E
run check-position-match

; Show the report...
;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
EOD

# If running them all, leave - but first write out teh buffer if there
# were any failures.
#
if [ "$1" = FULL-RUN ]; then
    cat >>uetest.rc <<'EOD'
!if &not &equ %fail 0
    set $cfname &cat "FAIL-" %test_name
    save-file
!else
    unmark-buffer
!endif
exit-emacs
EOD
# Just leave display showing if being run singly.
else
    cat >>uetest.rc <<'EOD'
unmark-buffer
-2 redraw-display
EOD
fi

# Do it...set the default uemacs if caller hasn't set one.
[ -z "$UE2RUN" ] && UE2RUN="./uemacs -d etc"
$UE2RUN -x ./uetest.rc

if [ "$1" = FULL-RUN ]; then
    if [ -f FAIL-$TNAME ]; then
        echo "$TNAME FAILed"
    else
        echo "$TNAME passed"
    fi
fi
