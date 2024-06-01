#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of Character Classes within Closures

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
execute-file autotest/check-position-char.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Character Class/Closure tests"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   [\p{Nd}\p{Po}]* searches"
run report-status
beginning-of-file
add-mode Exact
add-mode Magic

; ====
search-forward [\p{Nd}\p{Po}]*\X\n
  set %curtest [\p{Nd}\p{Po}]*\X\n
  set %expline 2
  set %expcol 1
  set %expchar &asc o
  set %expmatch s~n
run check-position-char
; ====
search-forward [\p{Nd}\p{Po}]+\X\n      ; Don't test this one...
; Wipe out memory of this search so we don't find the overlapping ones
forward-character
backward-character
search-forward [\p{Nd}\p{Po}]+\X\n
  set %curtest [\p{Nd}\p{Po}]+\X\n
  set %expline 6
  set %expcol 1
  set %expchar &asc N
  set %expmatch 0,1.2;3:4'5!6*7@8?9~n
run check-position-char
; ====
search-reverse \p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}\p{Nd}\p{Po}
  set %curtest "Search back for NPNPNPNP"
  set %expline 5
  set %expcol 16
  set %expchar &asc 5
  set %expmatch 5!6*7@8?
run check-position-char

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
