#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of word movement

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

; Declare the test table
;
store-pttable test
;
    caseset-on
    a`  a-backquote-déjà
    a'  a-apostro~nphe~ a-apostro~nphe~n
    caseset-capinit1
    b`  b-backquote
    b'  b-apostro~nphe~ b-apostro~nphe~n
    caseset-capinitall
    c`  c-backquote
    c'  c-apostrophe
    caseset-lowinit1
    d`  D-BACKQUOTE
    d'  D-APOSTROPHE
    caseset-lowinitall
    e`  E-BACKQUOTE
    e'  E-APOSTROPHE
    caseset-off
    f`  f-backquote
    f'  f-apostrophe
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-expansion
;   %curtest    description of the test
;   %expected   what we expect
;   %got        what we actually got
;
  !if &seq %got %expected
    set %test-report &cat %curtest &cat " - expanded OK: " %got
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG expansion, got: " %got
    set %test-report &cat %test-report &cat " - expected: " %expected
    set %fail &add %fail 1
  !endif
  run report-status
!endm

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
select-buffer ptt_test
add-mode Phon
set-pttable test

set %test-report "START: PTT tests"
run report-status

set .test "a`"
set %curtest &cat "caseset-on expand: " .test
set %expected "a-backquote-déjà"
set %got &ptx .test
run check-expansion

set .test "A`"
set %curtest &cat "caseset-on UP expand: " .test
set %expected "A-BACKQUOTE-DÉJÀ"
set %got &ptx .test
run check-expansion

set .test "a'"
set %curtest &cat "caseset-on expand multiline: " .test
set %expected "a-apostro~nphe~ a-apostro~nphe~n"
set %got &ptx .test
run check-expansion

set .test "b`"
set %curtest &cat "caseset-capinit1 expand: " .test
set %expected "b-backquote"
set %got &ptx .test
run check-expansion

set .test "B`"
set %curtest &cat "caseset-capinit1 UP expand: " .test
set %expected "B-backquote"
set %got &ptx .test
run check-expansion

set .test "b'"
set %curtest &cat "caseset-capinit1 expand multiline: " .test
set %expected "b-apostro~nphe~ b-apostro~nphe~n"
set %got &ptx .test
run check-expansion

set .test "B'"
set %curtest &cat "caseset-capinit1 UP expand multiline: " .test
set %expected "B-apostro~nphe~ b-apostro~nphe~n"
set %got &ptx .test
run check-expansion

set .test "c`"
set %curtest &cat "caseset-capinitall expand: " .test
set %expected "c-backquote"
set %got &ptx .test
run check-expansion

set .test "C'"
set %curtest &cat "caseset-capinitall expand: " .test
set %expected "C-Apostrophe"
set %got &ptx .test
run check-expansion

set .test "D`"
set %curtest &cat "caseset-lowinit1 expand: " .test
set %expected "D-BACKQUOTE"
set %got &ptx .test
run check-expansion

set .test "d'"
set %curtest &cat "caseset-lowinit1 expand: " .test
set %expected "d-APOSTROPHE"
set %got &ptx .test
run check-expansion

set .test "E`"
set %curtest &cat "caseset-lowinitall expand: " .test
set %expected "E-BACKQUOTE"
set %got &ptx .test
run check-expansion

set .test "e'"
set %curtest &cat "caseset-lowinitall expand: " .test
set %expected "e-aPOSTROPHE"
set %got &ptx .test
run check-expansion

set .test "f`"
set %curtest &cat "caseset-off expand: " .test
set %expected "f-backquote"
set %got &ptx .test
run check-expansion

set .test "F'"
set %curtest &cat "caseset-off UP (no match) expand: " .test
set %expected .test
set %got &ptx .test
run check-expansion




; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
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
