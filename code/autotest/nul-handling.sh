#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Some test to check that we can handles NULs in variable values.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
# This contains all of the tests
#
cat >uetest.rc <<'EOD'

set %test_name &env TNAME

select-buffer nul-tests

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0
delete-mode wrap
1 select-buffer

; Run a test in %test and compare the result to %expect
;
store-procedure run-test
  drop-pin
  select-buffer test-reports
    !if &seq %val %expect
        set %test-report &cat "OK: " %test
        set %ok &add %ok 1
    !else
        set %test-report &ptf "FAIL: %s - gave %s (exp: %s)" %test %val %expect
        set %fail &add %fail 1
    !endif
  insert-string %test-report
  newline
  back-to-pin
!endm

; Start by checking we can create a NUL
;
set %nul "~0"
set %test "create a NUL"
set %expect 1
set %val &len %nul
run run-test

; Now a NUL in the middle of a string
;
set %test "check an internal NUL"
set %int "ABC~0DEF"
set %expect 7
set %val &len %int
run run-test

insert-string "===~n~0~n===~n"

; Now cat this into a string...
;
set %str &cat "ABC " &cat %nul " DEF"
set %test "cat a NUL"
set %expect 9
set %val &len %str
run run-test
insert-string "===~n"
insert-string %str
insert-string "~n===~n"

; Now use &ptf to put this into a string...
;
set %test "ptf with a NUL"
set %str &ptf "%s%s%s" "ABC " "~0" " DEF"
set %expect 9
set %val &len %str
2 write-message %str
run run-test
insert-string "===~n"
insert-string %str
insert-string "~n===~n"
unmark-buffer

select-buffer "test-reports"
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
EOD

# If running them all, leave - but first write out the buffer if there
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
