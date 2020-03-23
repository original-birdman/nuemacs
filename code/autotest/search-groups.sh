#!/bin/sh
#

# Simple testing of Magic-mode groups

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the testfile
#
prog='$1 != "--" {print substr($0, 4);}'

# Need the -b because of the non-Unicode (invalid) 0xf1 bytes.
# But Entware systems don't understand it, and work without it.
#
if awk -b '{}' /dev/null >/dev/null 2>&1; then
    AWKARG=-b
else
    AWKARG=
fi

# Write out the test input file
# It's written here with row and column markers.
#
awk $AWKARG "$prog" > autotest.tfile <<EOD
-- 0123456789012345678901234567890123456789012345678901234567890123456789
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03   (but the ὐ has no upper-case equiv, so stays in lowercase!!!)
04
05 Now some Russian сейчас numbers 77712 (arabic ٧٧٧١٢) [Devenagari ७७७१२]
06 In UPPER СЕЙЧАС NUMBERS 77712 (ARABIC ٧٧٧١٢) [DEVENAGARI ७७७१२]
07
08 Can this be found case-sensitive and insensitive?
-- 0123456789012345678901234567890123456789012345678901234567890123456789
09
10 οὐδένmañanaСЕЙЧАС
11 οὐδένmañanaсейчас
12 ΟὐΔΈΝMAÑANAСЕЙЧАС
13
14 And don't forget that I need to check that a match (ANY MATCH!) isn't
15 followed by a combining character (which would raise odd issues for
16 replace). Some more numbers + text 912 TEST.
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
  insert-raw-string %test-report
  newline
  1 select-buffer     ; Back to buffer from whence we came
!endm

set %fail 0
set %ok 0

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
store-procedure check-position
;   Expects these to have been set, since this tests them all.
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
    set %test-report &cat %test-report &cat " expected: " %expchar
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " - at WRONG char, got: " %pchar
    set %fail &add %fail 1
  !endif

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
store-procedure check-group
;   This expected value must be set, since this tests it.
; %grpno            group to test
; %expmatch         group text
  !if &seq %expmatch &grp %grpno
    set %test-report &cat %curtest &cat " group " &cat %grpno " OK"
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " group " &cat %grpno " WRONG! got: " &cat &grp %grpno &cat " expected: " %expmatch
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various Magic group tests"
execute-procedure report-status

beginning-of-file
add-mode Exact
add-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  (Upper) followed by (lower)"
execute-procedure report-status

set %mcount 0
*again1
  !force search-forward "(\p{Lu}+)(\p{Ll}+)"
  !if &seq $force_status PASSED
     set %mcount &add %mcount 1
     !goto again1
  !endif
; Where did we end up, and how many searches succeeded?
  set %curtest Search3
  set %expline 16
  set %expcol 14
  set %expchar &asc " "
  set %expmatch ""      ; We'll have failed, so no match
execute-procedure check-position
  set %expcount 11
execute-procedure check-matchcount

; Reverse search and check match
  search-reverse "(\p{Lu}+)(\p{Ll}+)"
  set %curtest Search3-reversed
  set %expline 16
  set %expcol 10
  set %expchar &asc S
  set %expmatch "Some"
execute-procedure check-position

  set %grpno 1
  set %expmatch S
execute-procedure check-group
  set %grpno 2
  set %expmatch ome
execute-procedure check-group


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-raw-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc
