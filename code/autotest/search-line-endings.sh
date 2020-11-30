#!/bin/sh
#

# Simple test that we can match at the start and end of lines in
# Magic mode.
# And that the matched string is correct...

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
awk "$prog" > autotest.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
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
store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to whence we came
!endm

set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   Expects these to have been set, since it tests them all.
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
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

  !if &equ $curcol %expcol
    set %test-report &cat %curtest &cat " - column OK: " $curcol
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - WRONG column, got: " $curcol
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
  set  %test-report &cat %test-report &cat " (match: " &cat $match ")"
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
    set %test-report &cat %curtest &cat " match count WRONG, got: " &cat %mcount &cat " expected: " %expcount
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

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
  set %expcol 5
  set %expchar &asc " "
  set %expmatch Start
execute-procedure check-position

end-of-file
search-reverse ^Start
  set %curtest Search-^Start-back-from-end
  set %expline 1
  set %expcol 0
  set %expchar &asc S
  set %expmatch Start
execute-procedure check-position

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
  set %expcol 5
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
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
  set %expcol 5
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
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
  set %expcol 0
  set %expchar &asc S
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 3
execute-procedure check-matchcount

;   Now the End ones...
; ====
add-mode Exact
beginning-of-file
search-forward End$
  set %curtest Search-End$
  set %expline 3
  set %expcol 34
  set %expchar 10
  set %expmatch End
execute-procedure check-position

end-of-file
search-reverse End$
  set %curtest Search-End$-back-from-end
  set %expline 3
  set %expcol 31
  set %expchar &asc E
  set %expmatch End
execute-procedure check-position

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
  set %expcol 34
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
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
  set %expcol 34
  set %expchar 10
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
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
  set %expcol 31
  set %expchar &asc e
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 3
execute-procedure check-matchcount


;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
