#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Run some arithmetic tests and put the result into a viewable buffer.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
# This contains all of the tests
#
cat >uetest.rc <<'EOD'

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Run a test in %test and compare the result to %expect
;
store-procedure run-test
    set .res &ind %test
; Use string test, as some results returns strings (ZDIV, NAN, INF,
;  +INF or TOOBIG
;
    !if &seq .res %expect
        set %test-report &cat "OK: " %test
        set %ok &add %ok 1
    !else
        set %test-report &ptf "FAIL: %s - gave %s (exp: %s)" %test .res %expect
        set %fail &add %fail 1
    !endif
  insert-string %test-report
  newline
!endm

set %test "&neg 99"
set %expect -99
run run-test

set %test "&abs -123"
set %expect 123
run run-test

set %test "&add 1 2"
set %expect 3
run run-test

set %test "&sub 123 99"
set %expect 24
run run-test

set %test "&tim 99 11"
set %expect 1089
run run-test

set %test "&div 10 3"
set %expect 3
run run-test

set %test "&div 30 0"
set %expect ZDIV
run run-test

set %test "&mod 55 7"
set %expect 6
run run-test

set %test "&mod 55 0"
set %expect ZDIV
run run-test

set %test "&equ 7 7"
set %expect TRUE
run run-test

set %test "&les 5 6"
set %expect TRUE
run run-test

set %test "&gre 5 6"
set %expect FALSE
run run-test

set %test "&rad 12.6 18.3"
set %expect 30.9
run run-test

set %test "&rsu 0.3 12.7"
set %expect -12.4
run run-test

set %test "&rti 3.3 7.2"
set %expect 23.76
run run-test

set %test "&rti 3.3E333 7.2E333"
set %expect INF
run run-test

set %test "&rti -3.3E333 7.2E333"
set %expect -INF
run run-test

set %test "&rti -3.3E333 -7.2E333"
set %expect INF
run run-test

set %test "&rdv 346.71 12.7"
set %expect 27.3
run run-test

set %test "&rdv 346.71 0"
set %expect INF
run run-test

set %test "&rpw 12.3 4"
set %expect 22888.6641
run run-test

set %test "&r2i 12.493672"
set %expect 12
run run-test

; When running under valgrind the TOOBIG result was always 0.
; But With the switch 64-bit ints for macro variables, they now
; get MAX-64BITINT (arm64) or MIN-64BITINT (x64)
;; I suspect this is a valgrind "feature".
; So check whether we are running under valgrind
; UE2RUN contains what we will run

; Hmmmm.... whether TOOBIG is returned seems to depend on
; library/valgrind version, so we really want ot be able to allow
; either!
;
set .TOOBIG "TOOBIG"
!if &gre &sin &env UE2RUN "valgrind" 0
    set .TOOBIG &bli 0x7fffffffffffffff     ; Max 64-bit +ve
; Using 0x8000000000000000 actually sets 0x7fffffffffffffff (strtoll
; feature), so just use integer overflow...
;
    !if &seq $proc_type x86_64
        set .TOOBIG &add .TOOBIG 1
    !endif
!endif

set %test "&r2i 12.493672E123"
set %expect .TOOBIG
run run-test

; Test that -INF can be used as an input number
;
set %test "&rti -3.3E333 -INF"
set %expect INF
run run-test

set %test "&r2i INF"
set %expect .TOOBIG
run run-test

; Test for NAN or -NAN.
; FreeBSD gives NAN, but Linux gives NAN or -NAN!
; So use &xla to remove any - from -NAN before checking against NAN
;
set %test "&seq NAN &xla - ~"~" &rti INF 0"
set %expect FALSE
run run-test

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
