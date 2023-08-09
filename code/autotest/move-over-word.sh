#!/bin/sh
#

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
store-procedure report-status
  select-buffer test-reports
  insert-string %test-report
  newline
  1 select-buffer     ; Back to buffer from whence we came
!endm

set %test_name move-over-word

select-buffer test-reports
insert-string &cat %test_name " started"
newline
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

!endm

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
execute-procedure check-position

; This should stop at the zero-width break in Shoulder.
25 previous-word
  set %curtest BackWord2
  set %expline 3
  set %expcol 58
  set %expchar &asc o
execute-procedure check-position


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
execute-procedure check-position

; Get to the space after сейчас
35 next-word
  set %curtest ForwWord-ON
  set %expline 14
  set %expcol 26
  set %expchar &asc " "
execute-procedure check-position

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
execute-procedure check-position

; Get to the н in на
35 next-word
  set %curtest ForwWord-OFF
  set %expline 14
  set %expcol 27
  set %expchar &asc н
execute-procedure check-position


; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
unmark-buffer
-2 redraw-display
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc
