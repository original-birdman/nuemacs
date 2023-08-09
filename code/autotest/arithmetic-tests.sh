#!/bin/sh
#

# Run some arithmetic tests and put the result into a viewable buffer.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
# This contains all of the tests
#
cat >uetest.rc <<'EOD'

set %test_name arithmetic-test

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
execute-procedure run-test

set %test "&abs -123"
set %expect 123
execute-procedure run-test

set %test "&add 1 2"
set %expect 3
execute-procedure run-test

set %test "&sub 123 99"
set %expect 24
execute-procedure run-test

set %test "&tim 99 11"
set %expect 1089
execute-procedure run-test

set %test "&div 10 3"
set %expect 3
execute-procedure run-test

set %test "&div 30 0"
set %expect ZDIV
execute-procedure run-test

set %test "&mod 55 7"
set %expect 6
execute-procedure run-test

set %test "&mod 55 0"
set %expect ZDIV
execute-procedure run-test

set %test "&equ 7 7"
set %expect TRUE
execute-procedure run-test

set %test "&les 5 6"
set %expect TRUE
execute-procedure run-test

set %test "&gre 5 6"
set %expect FALSE
execute-procedure run-test

set %test "&rad 12.6 18.3"
set %expect 30.9
execute-procedure run-test

set %test "&rsu 0.3 12.7"
set %expect -12.4
execute-procedure run-test

set %test "&rti 3.3 7.2"
set %expect 23.76
execute-procedure run-test

set %test "&rti 3.3E333 7.2E333"
set %expect INF
execute-procedure run-test

set %test "&rti -3.3E333 7.2E333"
set %expect -INF
execute-procedure run-test

set %test "&rti -3.3E333 -7.2E333"
set %expect INF
execute-procedure run-test

set %test "&rdv 346.71 12.7"
set %expect 27.3
execute-procedure run-test

set %test "&rdv 346.71 0"
set %expect INF
execute-procedure run-test

set %test "&rpw 12.3 4"
set %expect 22888.6641
execute-procedure run-test

set %test "&r2i 12.493672"
set %expect 12
execute-procedure run-test

set %test "&r2i 12.493672E123"
set %expect TOOBIG
execute-procedure run-test

; Test that -INF can be used as an input number
;
set %test "&rti -3.3E333 -INF"
set %expect INF
execute-procedure run-test

set %test "&r2i INF"
set %expect TOOBIG
execute-procedure run-test

newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
unmark-buffer
-2 redraw-display
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
