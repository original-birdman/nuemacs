#!/bin/sh
#

# Simple testing of wrapping in Wrap mode

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
01 Just some text to act as a regression test for test wrapping.  It needs
02 to contain a "selection" of end-of-sentence characters.  Like this?  It
03 also needs to have a number, such as 3.14159, to check that the "."
04 there is not taken as a sentence ender.
05
06 This second paragraph will have some unicode characters in it, with and
07 without combining diacriticals (== accents).  So things like déjà vu and
08 déjà vu (which are not the same as the first contains
09 specifically-accented characters, while the latter uses combining
-- 0123456789012345678901234567890123456789012345678901234567890123456789
10 diacriticals.  Then we can have some Rus​sian, Матрица спинового
11 гамильтониана, and the same in Greek, Περιστροφή Χαμιλτονιανής μήτρας,
12 according to Google Translate, although I think it has the wrong meaning
13 for "spin".
14
15 We also need an example of an over-long piece of text with no break in
16 it, so cannot be wrapped itself, such as
17 2,4-bis(tetra-phenyl-arsonium)-penta-thiocyonata-rhenium(VI) or
18 something like that.  It's (Ph₄As)₂Re(NCS)₅, which is deep purple.
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
store-procedure check-full-line
;   Expects these to have been set, since this tests them all.
; %expltext     the expected line
;
  !if &seq $line %expltext
    set %test-report &cat %curtest &cat " OK text for: line " $curline
    set %ok &add %ok 1
  !else
    set %test-report &cat %curtest &cat " WRONG text for: line " $curline
    set %test-report &cat %test-report &cat " expected: " &chr 10
    set %test-report &cat %test-report %expltext
    set %test-report &cat %test-report &cat &chr 10 &cat " got: " &chr 10
    set %test-report &cat %test-report $line
    set %fail &add %fail 1
  !endif
  execute-procedure report-status

!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various paragraph filling tests"
execute-procedure report-status

; First check the zero-width char is there. In case it was
; accidentally removed in an edit....
;
add-mode Magic
!force search-forward \u{200b}
!if &equ 0 &len &grp 0
    set %test-report "The zero-width char in Russian is missing. Cannot continue"
    execute-procedure report-status
    select-buffer test-reports
    unmark-buffer
    !finish
!endif
delete-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Para1 tests"
execute-procedure report-status

1 goto-line
60 set-fill-column
fill-paragraph
  set %curtest Para1-fill-paragraphA
  set %expline 6
  set %expcol 0
  set %expchar 10
execute-procedure check-position

; expect line 4 to end with "there"
4 goto-line
end-of-line
previous-word
  set %curtest Para1-fill-paragraphB
  set %expline 4
  set %expcol 54
  set %expchar &asc t
execute-procedure check-position

; Paragraph2 tests
; next-paragraph is actually end of current one
next-paragraph
next-word
previous-word       ; Now at b-o-l in para2
10 forward-character
72 set-fill-column
; Justify to current offset. -ve == r/margin justiy.
-1 justify-paragraph
  set %curtest Para2-justify-paragraphA
  set %expline 17
  set %expcol 10
  set %expchar &asc e
execute-procedure check-position

; The "spin". at the end should be on its own
2 previous-line
  set %curtest Para2-justify-paragraphB
  set %expline 15
  set %expcol 10
  set %expchar 34  ; "
execute-procedure check-position

  set %curtest Para2-justify-paragraph-linetext
  set %expltext "          ~"spin~"."
execute-procedure check-full-line

; The text should have wrapped on a zero-width break between the ss in Russian
; Check that it did.
;
3 previous-line
  set %curtest Para2-justify-paragraph-zwb1
  set %expltext "          sian, Матрица спинового гамильтониана, and the same in  Greek,"
execute-procedure check-full-line

; This line should end with the zero-width break
previous-line
  set %curtest Para2-justify-paragraph-zwb2
  set %expltext "          latter uses combining diacriticals.  Then we can have some Rus​"
execute-procedure check-full-line

; Now on to para3, which has an over-long line
;
; next-paragraph is actually end of current one
next-paragraph
next-word
previous-word       ; Now at b-o-l in para2
55 set-fill-column
fill-paragraph
  set %curtest Para3-fill-longline-paragraphA
  set %expline 23
  set %expcol 0
  set %expchar 10
execute-procedure check-position

; Line 19 should be a single word (as) with no spaces
; Line 20 should be overlong
; Line 21 should start with "or".
;
19 goto-line
  set %curtest Para3-fill-longline-paragraphB
  set %expline 19
  set %expcol 0
  set %expchar &asc a
execute-procedure check-position
20 goto-line
end-of-line
backward-character
  set %curtest Para3-fill-longline-paragraphC
  set %expline 20
  set %expcol 59
  set %expchar &asc )
execute-procedure check-position
21 goto-line
  set %curtest Para3-fill-longline-paragraphD
  set %expline 21
  set %expcol 0
  set %expchar &asc o
execute-procedure check-position

; Re-read file...
unmark-buffer
read-file autotest.tfile

; Mark the whole original file then make a justified list.
set-mark
end-of-file
72 set-fill-column
-1 numberlist-region

2 goto-line
  set %curtest WholeFile-numberlist-A
  set %expltext "     needs to contain a ~"selection~" of end-of-sentence characters.  Like"
execute-procedure check-full-line

6 goto-line
  set %curtest WholeFile-numberlist-B
  set %expltext "  2. This second paragraph will have some unicode characters in it, with"
execute-procedure check-full-line

17 goto-line
beginning-of-line
  set %curtest WholeFile-numberlist-C
  set %expltext "     2,4-bis(tetra-phenyl-arsonium)-penta-thiocyonata-rhenium(VI)     or"
execute-procedure check-full-line

unmark-buffer

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
unmark-buffer
-2 redraw-display
EOD

# Run the tests...
#
./uemacs -c etc/uemacs.rc -x ./uetest.rc