#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

cat > FAIL-$TNAME <<EOD
Test not completed, for some reason.
EOD

# Test matches for strings including NULs

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
    s/\Q^@/chr(0)/eg;   # Replace ^@ with actual NUL
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

; We do not run the standard start-up file, so ggr_opts is left
; as unset.
; This means that we run with the original ^@ display for NUL
; and this needs to be used for column matching.

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Load the check routine
;
execute-file autotest/check-position-matchlen.rc
execute-file autotest/check-group.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Search for NUL"
run report-status

add-mode Exact

; We'll run the tests twice.
; The first time with Ctlgph unset and then again with ti set
;
set $ggr_opts &ban $ggr_opts &bno 0x20
set .ctl_add 1

*start-tests

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward search
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
;
beginning-of-file
search-forward "c~0d"
  set %test-report "search for c~0d"
  run report-status
  set %curtest Search-NUL
  set %expline 2
  set %expcol &add 6 .ctl_add
  set %expchar &asc "e"
  set %expmatchlen 3
  run check-position-matchlen

beginning-of-file
search-forward "c~0d"
reexecute
  set %test-report "searchX2 for c~0d"
  run report-status
  set %curtest Search-NUL
  set %expline 5
  set %expcol &add 6 .ctl_add
  set %expchar &asc "e"
  set %expmatchlen 3
  run check-position-matchlen

end-of-file
search-reverse "c~0d"
  set %test-report "reverse search for c~0d"
  run report-status
  set %curtest Search-NUL
  set %expline 5
  set %expcol 3
  set %expchar &asc "c"
  set %expmatchlen 3
  run check-position-matchlen

end-of-file
search-reverse "c~0d"
reexecute
  set %test-report "reverse searchX2 for c~0d"
  run report-status
  set %curtest Search-NUL
  set %expline 2
  set %expcol 3
  set %expchar &asc "c"
  set %expmatchlen 3
  run check-position-matchlen

add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Forward Magic search
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
;
beginning-of-file
search-forward "(.)~0(.)"
  set %test-report "search for (.)~0(.)"
  run report-status
  set %curtest Magic-Search-NUL
  set %expline 2
  set %expcol &add 6 .ctl_add
  set %expchar &asc "e"
  set %expmatchlen 3
  run check-position-matchlen
  set %grpno 1
  set %expmatch "c"
  run check-group
  set %grpno 2
  set %expmatch "d"
  run check-group

search-forward "c(.)d"
  set %test-report "search for c(.)d"
  run report-status
  set %curtest Magic-Search-NUL
  set %expline 5
  set %expcol &add 6 .ctl_add
  set %expchar &asc "e"
  set %expmatchlen 3
  run check-position-matchlen
  set %grpno 1
  set %expmatch "~0"
  run check-group

; NOTE that this find f^@g, not c^@g
;
end-of-file
search-reverse "(.)~0(.)"
  set %test-report "reverse search for (.)~0(.)"
  run report-status
  set %curtest Magic-Search-NUL
  set %expline 5
  set %expcol &add 7 .ctl_add
  set %expchar &asc "f"
  set %expmatchlen 3
  run check-position-matchlen
  set %grpno 1
  set %expmatch "f"
  run check-group
  set %grpno 2
  set %expmatch "g"
  run check-group

search-reverse "c(.)d"
  set %test-report "reverse search for c(.)d"
  run report-status
  set %curtest Magic-Search-NUL
  set %expline 5
  set %expcol 3
  set %expchar &asc "c"
  set %expmatchlen 3
  run check-position-matchlen
  set %grpno 1
  set %expmatch "~0"
  run check-group

!if &not &ban $ggr_opts 0x20
  set $ggr_opts &bor $ggr_opts 0x20
  set .ctl_add 0
  set %test-report "Repeat all tests with Ctlgph on"
  run report-status
  !goto start-tests
!endif

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
$UE2RUN -c ./uetest.rc

if [ "$1" = FULL-RUN ]; then
    if [ -f FAIL-$TNAME ]; then
        echo "$TNAME FAILed"
    else
        echo "$TNAME passed"
    fi
fi
