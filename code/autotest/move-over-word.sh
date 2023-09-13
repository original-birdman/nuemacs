#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of word movement

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='next if (/^--/); chomp; print substr($_, 3);'

# Write out the test input file.
# It's written here with row and column markers.
#
perl -lne "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 We need some text to test word movement.
02 
03 This is a line containing a zero-width break after Sh. Sh​oulder.
04 
05 This ia a "normal" line
06
07 Now lines with accents and othe multi-byte chars.
08 There are three ñ characters here but the bytes differ.
09    mañana mañana ma�ana
10
-- 123456789012345678901234567890123456789012345678901234567890123456789
11 Now some Greek
12  Σὲ γνωρίζω ἀπὸ τὴν κόψη
13 and Russian
14  Зарегистрируйтесь сейчас на Десятую Международную Конференцию по
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
execute-file autotest/check-position-match.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various word mode tests"
execute-procedure report-status

; First check the zero-width char is there. In case it was
; accidentally removed in an edit....
;
add-mode Magic
!force search-forward \u{200b}
!if &equ 0 &len &grp 0
    set %test-report "The zero-width char is missing. Cannot continue"
    execute-procedure report-status
    select-buffer test-reports
    unmark-buffer
    !finish
!endif
delete-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "Backwards tests"
execute-procedure report-status

; These are independent of Forwword mode
;
end-of-file
20 previous-word
  set %curtest BackWord1
  set %expline 9
  set %expcol 4
  set %expchar &asc m
execute-procedure check-position-match

; This should stop at the zero-width break in Shoulder.
25 previous-word
  set %curtest BackWord2
  set %expline 3
  set %expcol 58
  set %expchar &asc o
execute-procedure check-position-match


; Forward with ggr_opts Forwword OFF
;
set $ggr_opts &bor $ggr_opts 0x02
beginning-of-file
; Get to the closing " of '"normal"'
25 next-word
  set %curtest ForwWord-ON
  set %expline 5
  set %expcol 18
; 34 = "
  set %expchar 34
execute-procedure check-position-match

; Get to the space after сейчас
35 next-word
  set %curtest ForwWord-ON
  set %expline 14
  set %expcol 26
  set %expchar &asc " "
execute-procedure check-position-match

; Forward with ggr_opts Forwword OFF
;
set $ggr_opts &bxor $ggr_opts 0x02
beginning-of-file
; Get to the start of "line".
25 next-word
  set %curtest ForwWord-OFF
  set %expline 5
  set %expcol 20
  set %expchar &asc l
execute-procedure check-position-match

; Get to the н in на
35 next-word
  set %curtest ForwWord-OFF
  set %expline 14
  set %expcol 27
  set %expchar &asc н
execute-procedure check-position-match

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
