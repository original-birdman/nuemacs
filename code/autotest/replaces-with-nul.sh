#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

cat > FAIL-$TNAME <<EOD
Test not completed, for some reason.
EOD

# Test replaces with strings including NULs

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

set %test-report "START: Replaces with NUL"
run report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward replaces
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
;
add-mode Exact
beginning-of-file
replace-string "c~0d" "~0cd~0"
  set %curtest "Replace NUL string"
  2 goto-line
  set %expline "ab~0cd~0ef~0ghi"
  run check-line
  5 goto-line
  set %expline "ab~0cd~0ef~0ghi"
  run check-line

unmark-buffer
read-file autotest.tfile
add-mode Magic
beginning-of-file
replace-string "(.)(~0)(.)" "${0} ${1} ${2} ${3}"
  set %curtest "Replace NUL string magically"
  2 goto-line
  set %expline "abc~0d c ~0 d ef~0g f ~0 g hi"
  run check-line
  5 goto-line
  set %expline "abc~0d c ~0 d ef~0g f ~0 g hi"
  run check-line

unmark-buffer
read-file autotest.tfile
add-mode Magic
beginning-of-file
replace-string "(.)(~0)(.)" "${&ptf ~" match2 was >>${2}<< ~"}"
  set %curtest "Replace NUL string magically with ptf call"
  2 goto-line
  set %expline "ab match2 was >>~0<< e match2 was >>~0<< hi"
  run check-line
  5 goto-line
  set %expline "ab match2 was >>~0<< e match2 was >>~0<< hi"
  run check-line

;
select-buffer test-reports
newline
insert-string &ptf "END: ok: %s fail: %s~n%s ended" %ok %fail %test_name

EOD

# If running them all, leave - but first write out the buffer if there
# were any failures.
#
if [ "$1" = FULL-RUN ]; then
    cat >>uetest.rc <<'EOD'
    set $cfname &cat "FAIL-" %test_name
    !if &not &equ %fail 0
        save-file
    !else
        shell-command &ptf "rm -f %s" $cfname
    unmark-buffer
!endif
0 exit-emacs
EOD
# Just leave display showing if being run singly.
else
    cat >>uetest.rc <<'EOD'
unmark-buffer
shell-command &ptf "rm -f FAIL-%s" %test_name
set $cfname %test_name
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
