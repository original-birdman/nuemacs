#!/bin/sh
#

# Check that searching works if Magic modeis used, then it is switched
# off.
# At one point the step_scan was no reset, so the pattern structure for
# the Magic search was repeated.
# Short and simple
#

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

# -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
# Write out the test input file
# It's written here with row and column markers.
#
awk $AWKARG "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Greek text: οὐδέν
02 In UPPER ΟὐΔΈΝ.
03
04 Now some Russian сейчас
05 In UPPER СЕЙЧАС
06
07 We'll stop here...
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
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Magic On then Off tests"
execute-procedure report-status

; ==== Run in Magic, non-Exact 
;
set %test-report "   сейчас searches"
execute-procedure report-status
beginning-of-file
delete-mode Exact
add-mode Magic

2 search-forward сейчас
; Where did we end up
  set %curtest Search-Magic-caseoff
  set %expline 5
  set %expcol 16
  set %expchar 10
  set %expmatch "СЕЙЧАС"
execute-procedure check-position

; Now switch Magic off and Exact on. Search for ASCII
;
set %test-report "   stop searches"
execute-procedure report-status
beginning-of-file
add-mode Exact
delete-mode Magic
search-forward stop
; Where did we end up
  set %curtest Search- NoMagic-caseon
  set %expline 7
  set %expcol 11
  set %expchar &asc " "
  set %expmatch "stop"
execute-procedure check-position


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; Show the report...
;
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
-2 redraw-display
EOD

./uemacs -c etc/uemacs.rc -x ./uetest.rc
