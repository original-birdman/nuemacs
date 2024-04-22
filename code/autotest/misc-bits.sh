#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Quick test of variosu things

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
01 Some text for testing     white-space deletion
02 Some text for testing     white-space leaving
03 Some text for testing     white-space deletion
04 Some text for testing     white-space leaving
EOD

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
execute-file autotest/check-position.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Miscellaneuos tests"
run report-status

beginning-of-file

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should collapse multiple-spaces to one
;
set %test-report "leave-one-white"
run report-status

4 next-word             ; reach the multi-space part
leave-one-white
  set %curtest " standard"
  set %expline 1
  set %expcol 23
  set %expchar &asc "w"
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Should collapse multiple-spaces to none
;
set %test-report "white-delete"
run report-status

2 goto-line
4 next-word             ; reach the multi-space part
white-delete
  set %curtest " standard"
  set %expline 2
  set %expcol 22
  set %expchar &asc "w"
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; This should ADD a space where there was not one.
;
set %test-report "leave-one-white"
run report-status

3 goto-line
-2 leave-one-white
  set %curtest " force space"
  set %expline 3
  set %expcol 2
  set %expchar &asc "S"
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Try to delete spaces when there are none.
; Shodul return False (so have ot force it).
;
set %test-report "white-delete"
run report-status
4 goto-line
!force white-delete
  set %curtest " no spaces to remove"
  set %expline 4
  set %expcol 1
  set %expchar &asc "S"
run check-position

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; This buffer is now shorter than it was.
; Check it is what we expect if we write it out.
;
save-file
select-buffer md5_check
set %test-report "write-truncate"
pipe-command "md5sum autotest.tfile"
  set %curtest "Check overwrite with shorter file"
  set .expect "6b2dc4aa30087d55c0fb4ed16e3fe3da  autotest.tfile"
  !if &equ $line .expect
    set %test-report &cat %curtest &cat " - line OK: " $line
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG line, got: " $line
    set %test-report &cat %test-report &cat " - expected: " .expect
    set %fail &add %fail 1
  !endif
  run report-status
select-buffer "autotest.tfile"
delete-other-windows

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Test overwrite-string
; This will be done on the "edited" autotest.tfile
; Just overwrite at the dnf of line 1 and check it extends...
;
set %test-report "overwrite-string"
1 goto-line
end-of-line
3 backward-character
drop-pin                ; So we can test what is there later
set .orig_col $curcol
set .ov_text "∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)"
set .ov_text_len &len .ov_text
overwrite-string .ov_text

; Are we at the correct column, and at e-o-l?
;
  set %curtest "Overwrite extended line?"
  set %expline 1
  set %expcol &add .orig_col .ov_text_len
  set %expchar 10
run check-position

; Check the character at the makr is what we expect.
;
back-to-pin
; Do we return the correct column, and see the correct char?
;
  set %curtest "Check start of overwritten string"
  set %expline 1
  set %expcol .orig_col
  set %expchar &asc &lef .ov_text 1
run check-position

unmark-buffer

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
