#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Magic-mode properties (part2)

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

execute-file autotest/report-status.rc

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Load the check routine
;
execute-file autotest/check-position.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
find-file autotest.tfile
add-mode Magic

set %test-report "START: Various property tests (part 2)"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   \p{Nd}\p{Po} searches"
run report-status
beginning-of-file
; ====
search-forward \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest Search1
  set %expline 5
  set %expcol 12
  set %expchar &asc 3
  set %expmatch 0,1.2;
run check-position
search-forward \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest Search2
  set %expline 5
  set %expcol 20
  set %expchar &asc 7
  set %expmatch 3:4'5!6*
run check-position

set %test-report "  \P searches"
run report-status
beginning-of-file
; ====
search-forward 789\n
  set %curtest "Placing search"
  set %expline 4
  set %expcol 1
  set %expchar &asc H
  set %expmatch 789~n
run check-position
; ====
search-forward \P{L}
  set %curtest Search not Letter
  set %expline 4
  set %expcol 6
  set %expchar &asc " "
  set %expmatch ,
run check-position

set %test-report "  \p{SM}{5} search"
run report-status
beginning-of-file
; ====
search-forward \p{SM}{5}
  set %curtest Search1
  set %expline 9
  set %expcol 7
  set %expchar &asc ~~
  set %expmatch +<=>|
run check-position

set %test-report "  6-class search"
run report-status
beginning-of-file
; ====
search-forward \p{Lm}\p{Sm}\p{Ps}\p{Pe}\p{Nd}\p{Po}
  set %curtest Search1
  set %expline 13
  set %expcol 12
  set %expchar &asc z
  set %expmatch ˊ±{]3@
run check-position
end-of-file
; ====
search-reverse \p{Lu}\p{ll}\p{Po}\p{Nd}\p{Pe}\p{Ps}
  set %curtest "Reverse search"
  set %expline 14
  set %expcol 6
  set %expchar &asc T
  set %expmatch Td:8)[
run check-position

set %test-report "  negative reverse search"
run report-status
; ====
!force search-reverse \p{M}
  set %curtest "Reverse search for Mark (none there)"
  set %expline 14
  set %expcol 6
  set %expchar &asc T
  set %expmatch ""
run check-position

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
