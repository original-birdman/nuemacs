#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple test that we can match at the start and end of lines in
# Magic mode.
# And that the matched string is correct...

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
01 Start with (Start and enD) and enD
02 sTart with (sTart and EnD) and EnD
03 STart with (STart and End) and End
04 EOF
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

execute-file autotest/check-matchcount.rc

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
find-file autotest.tfile
add-mode Exact
add-mode Magic

set %test-report "START: Start/end line tests for Magic"
execute-procedure report-status

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "   Case sensitive"
execute-procedure report-status

; ====
beginning-of-file
search-forward ^Start
  set %curtest Search-^Start
  set %expline 1
  set %expcol 6
  set %expchar &asc " "
  set %expmatch Start
execute-procedure check-position-match

end-of-file
search-reverse ^Start
  set %curtest Search-^Start-back-from-end
  set %expline 1
  set %expcol 1
  set %expchar &asc S
  set %expmatch Start
execute-procedure check-position-match

; ====
beginning-of-file
set %mcount 0
*again1
  !force search-forward ^Start
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Searchrpt-^Start
  set %expline 1
  set %expcol 6
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 1
execute-procedure check-matchcount

; ====
beginning-of-file
delete-mode Exact
set %mcount 0
*again2
  !force search-forward ^start
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search-^start-NonExact
  set %expline 3
  set %expcol 6
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 3
execute-procedure check-matchcount

; ====
; Now search back for them all
set %mcount 0
*again3
  !force search-reverse ^START
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again3
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search-^START-NonExact-rev
  set %expline 1
  set %expcol 1
  set %expchar &asc S
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 3
execute-procedure check-matchcount

;   Now the End ones...
; ====
add-mode Exact
beginning-of-file
search-forward End$
  set %curtest Search-End$
  set %expline 3
  set %expcol 35
  set %expchar 10
  set %expmatch End
execute-procedure check-position-match

end-of-file
search-reverse End$
  set %curtest Search-End$-back-from-end
  set %expline 3
  set %expcol 32
  set %expchar &asc E
  set %expmatch End
execute-procedure check-position-match

; ====
beginning-of-file
set %mcount 0
*again4
  !force search-forward End$
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again4
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Searchrpt-End$
  set %expline 3
  set %expcol 35
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 1
execute-procedure check-matchcount

; ====
beginning-of-file
delete-mode Exact
set %mcount 0
*again5
  !force search-forward end$
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again5
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search-end$-NonExact
  set %expline 3
  set %expcol 35
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 3
execute-procedure check-matchcount

; ====
; Now search back for them all
set %mcount 0
*again6
  !force search-reverse END$
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again6
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search-END$-NonExact-rev
  set %expline 1
  set %expcol 32
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position-match
  set %expcount 3
execute-procedure check-matchcount


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
