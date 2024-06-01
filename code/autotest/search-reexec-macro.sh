#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple test that using search with a reexeced macro searches
# for the given string, not a rexec of the previous search.
#

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
01 match - ]xyzzy[
02 EOF
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

execute-file autotest/check-group.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-count
;   This expected value must be set, since this tests it.
; %expcalls         call count we should have reached
  !if &equ %expcalls %calls
    set %test-report &cat %curtest &cat " calls " &cat %calls " OK"
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " calls WRONG! got: " %calls
    set %test-report &cat %test-report &cat " - expected: " %expcalls
    set %fail &add %fail 1
  !endif
  run report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
find-file autotest.tfile
add-mode Exact

set %test-report "START: Various Character Class tests"
run report-status

set %calls 0
store-procedure tester
    set %test-report "Entering tester"
    run report-status
    search-forward xyzzy
    search-reverse atc
    set %calls &add %calls 1
    set %test-report "Leaving tester"
    run report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   search in reexec macro"
run report-status
beginning-of-file

run tester
  set %curtest "Run tester"
  set %expline 1
  set %expcol 2
  set %expchar &asc a
  set %expmatch atc
run check-position
  set %expcalls 1
run check-count

run tester
  set %curtest "Run tester again"
  set %expline 1
  set %expcol 2
  set %expchar &asc a
  set %expmatch atc
run check-position
  set %expcalls 2
run check-count

run tester    ; Must run it again to be able rexecute it!
2 reexecute
  set %curtest "Run tester again"
  set %expline 1
  set %expcol 2
  set %expchar &asc a
  set %expmatch atc
run check-position
  set %expcalls 5
run check-count


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
