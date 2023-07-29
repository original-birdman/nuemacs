#!/bin/sh
#

# This needs gawk -b to work, so if it doesn't - stop now...
#
gawk -b {exit} </dev/null >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo >&2 "No working 'gawk -b'. Ignoring search-boundaries test"
    echo -n "Press <CR> to continue"
    read dnc
    exit 0
fi

# Test for searching with hits at beginning/end of line/buffer.

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# Need the -b because of the non-Unicode (invalid) 0xf1 bytes.
#
# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
gawk -b "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Able was I ere I was Elba
02 Just some text to be searched in each direction
03 that will produce hist at the start and end of liens and buffer.
04 Able was I ere I was Elba
EOD

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the uemacs start-up file, that will run the tests
#
cat >uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $match are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to whence we came
!endm

set %test_name search-boundaries

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   These expected values must be set, since this tests them all.
; %expline      the line for the match
; %expcol       the column for the match
; %expchar      the expected "char" at point (R/h side) at the match
; %expmatch     the text of the match
;
  !if &equ $curline %expline
    set %test-report &cat %curtest &cat " - line OK: " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG line, got: " $curline
    set %test-report &cat %test-report &cat " - expected: " %expline
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curcol %expcol
    set %test-report &cat %curtest &cat " - column OK: " $curcol
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG column, got: " $curcol
    set %test-report &cat %test-report &cat " - expected: " %expcol
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curchar 10
    set %pchar "\n"
  !else
    set %pchar &chr $curchar
  !endif
  !if &equ $curchar %expchar
    set %test-report &cat %curtest &cat " - at OK: " %pchar
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - at WRONG char, got: " %pchar
    set %test-report &cat %test-report &cat " expected: " %expchar
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &seq $match %expmatch
    set %test-report &cat %curtest &cat " - matched OK: " %expmatch
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - match WRONG, got: " $match
    set %test-report &cat %test-report &cat " expected: " %expmatch
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-matchcount
;   This expected value must be set, since this tests it.
; %expcount         number of successful matches
  !if &equ %mcount %expcount
    set %test-report &cat %curtest &cat " match count OK: " %mcount
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " match count WRONG, got: " %mcount
    set %test-report &cat %test-report &cat " - expected: " %expcount
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various boundary searches"
execute-procedure report-status

; ==== Run in EXACT mode
;
set %test-report " forward Elba search"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic

set %mcount 0
*again1
  !force search-forward Elba
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Forward Elba search"
  set %expline 4
  set %expcol 26
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 2
execute-procedure check-matchcount

; Now run a reverse search for Able
set %mcount 0
*again2
  !force search-reverse Able
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again2
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Reverse Able search"
  set %expline 1
  set %expcol 1
  set %expchar &asc A
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 2
execute-procedure check-matchcount

; Now repeat the above in Magic mode with a magic string that
; will match the same things.
;
add-mode Magic
set %mcount 0
*again3
  !force search-forward "Elb."
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again3
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Forward Elb. magic search"
  set %expline 4
  set %expcol 26
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 2
execute-procedure check-matchcount

; Now run a reverse search for Able
set %mcount 0
*again4
  !force search-reverse ".ble"
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again4
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest "Reverse .ble magic search"
  set %expline 1
  set %expcol 1
  set %expchar &asc A
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 2
execute-procedure check-matchcount


; Show the report...
;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
unmark-buffer
-2 redraw-display
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
