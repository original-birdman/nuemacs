#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Test values of environment variables including NULs

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
if type perl >/dev/null 2>&1; then
    : OK
else
    echo "This test REQUIRES perl"
    exit 1
fi

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
# It's written directly in Perl, as trying to get a NUL in using
# here-documents or awk hasn't worked.
#

cat - > autotest.pl <<'EOD'
open my $ofh, ">", "autotest.tfile" or die;
local $\ = "\n";
while (<DATA>) {
    next if (/^--/);
    chomp;
    s/\Q^@/chr(0)/eg;  # Replace ^@ with actual NUL
    print $ofh substr($_, 3);
}
close $ofh;
exit;
__DATA__
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 AAA
02 abc^@def^@ghi
03 ZZZ
04 AAA
05 abc^@def^@ghi
06 ZZZ
EOD

perl autotest.pl
status=$?
rm -f autotest.pl
[ $status -ne 0 ] && exit $status

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $matchlen are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

execute-file autotest/report-status.rc

store-procedure check-var
;   This expected value must be set, since this tests it.
; %var            the var to test
; %expmatch         what it expects to see
;
  !if &seq %expmatch &ind %var
    set %test-report &ptf "%s: var %s OK" %curtest %var
    set %ok &add %ok 1
  !else
    set %test-report &ptf "%s: var %s WRONG! got: %s%" %curtest %var &ind %var
    set %test-report &cat %test-report &cat " - expected: " %expmatch
    set %fail &add %fail 1
  !endif
  run report-status
!endm


set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Load the check routine
;
execute-file autotest/check-line.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Variables with NUL"
run report-status

2 goto-line
  set %curtest "Checking $line"
  set %var "$line"
  set %expmatch "abc~0def~0ghi"
  run check-var

3 forward-character
  set %curtest "Checking $curchar"
  set %var "$curchar"
  set %expmatch "0"
  run check-var

beginning-of-file
  search-forward "f~0g"
  set %curtest "Checking $match in search"
  set %var "$match"
  set %expmatch "f~0g"
  run check-var
  set %curtest "Checking $search"
  set %var "$search"
  set %expmatch "f~0g"
  run check-var

beginning-of-file
  1 replace-string "f~0g" xy~0~0y
  set %curtest "Checking $replace"
  set %var "$replace"
  set %expmatch "xy~0~0y"
  run check-var
  

select-buffer test-reports
newline
insert-string &ptf "END: ok: %s fail: %s~n%s ended" %ok %fail %test_name

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
0 exit-emacs
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
