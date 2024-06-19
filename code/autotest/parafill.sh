#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# Simple testing of wrapping in Wrap mode

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

# Write out the test input file
# It's written here with row and column markers.
#
$cmd "$prog" > autotest.tfile <<EOD
-- 123456789012345678901234567890123456789012345678901234567890123456789
01 Just some text to act as a regression test for test wrapping.  It needs
02 to contain a "selection" of end-of-sentence characters.  Like this?  It
03 also needs to have a number, such as 3.14159, to check that the "."
04 there is not taken as a sentence ender.
05
06 This second paragraph will have some unicode characters in it, with and
07 without combining diacriticals (== accents).  So things like déjà vu and
08 déjà vu (which are not the same as the first contains
09 specifically-accented characters, while the latter uses combining
-- 123456789012345678901234567890123456789012345678901234567890123456789
10 diacriticals.  Then we can have some Rus​sian, Матрица спинового
11 гамильтониана, and the same in Greek, Περιστροφή Χαμιλτονιανής μήτρας,
12 according to Google Translate, although I think it has the wrong meaning
13 for "spin".
14
15 We also need an example of an over-long piece of text with no break in
16 it, so cannot be wrapped itself, such as
17 2,4-bis(tetra-phenyl-arsonium)-penta-thiocyonata-rhenium(VI) or
18 something like that.  It's (Ph₄As)₂Re(NCS)₅, which is deep purple.
19
20 Finally we add a paragrpah at the end of the file which finishes with
21 an overlong line, such as a URL, in order to check that filling this
22 does not wrap to the start of the file http://www.example.com/a_long_url_path/to_make_sure_we_are/too_long_for_the_fiil_column_setting
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
execute-file autotest/check-position.rc

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
  run report-status

!endm

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
; START running the code!
;
find-file autotest.tfile

set %test-report "START: Various paragraph filling tests"
run report-status

; First check the zero-width char is there. In case it was
; accidentally removed in an edit....
;
add-mode Magic
!force search-forward \u{200b}
!if &equ 0 &len &grp 0
    set %test-report "The zero-width char in Russian is missing. Cannot continue"
    run report-status
    select-buffer test-reports
    unmark-buffer
    !finish
!endif
delete-mode Magic

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
set %test-report "  Para1 tests"
run report-status

1 goto-line
60 set-fill-column
fill-paragraph
  set %curtest Para1-fill-paragraphA
  set %expline 6
  set %expcol 1
  set %expchar 10
run check-position

; expect line 4 to end with "there"
4 goto-line
end-of-line
previous-word
  set %curtest Para1-fill-paragraphB
  set %expline 4
  set %expcol 55
  set %expchar &asc t
run check-position

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
  set %expcol 11
  set %expchar &asc e
run check-position

; The "spin". at the end should be on its own
2 previous-line
  set %curtest Para2-justify-paragraphB
  set %expline 15
  set %expcol 11
  set %expchar 34  ; "
run check-position

  set %curtest Para2-justify-paragraph-linetext
  set %expltext "          ~"spin~"."
run check-full-line

; The text should have wrapped on a zero-width break between the ss in Russian
; Check that it did.
;
3 previous-line
  set %curtest Para2-justify-paragraph-zwb1
  set %expltext "          sian, Матрица спинового гамильтониана, and the same in  Greek,"
run check-full-line

; This line should end with the zero-width break
previous-line
  set %curtest Para2-justify-paragraph-zwb2
  set %expltext "          latter uses combining diacriticals.  Then we can have some Rus​"
run check-full-line

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
  set %expcol 1
  set %expchar 10
run check-position

; Line 19 should be a single word (as) with no spaces
; Line 20 should be overlong
; Line 21 should start with "or".
;
19 goto-line
  set %curtest Para3-fill-longline-paragraphB
  set %expline 19
  set %expcol 1
  set %expchar &asc a
run check-position
20 goto-line
end-of-line
backward-character
  set %curtest Para3-fill-longline-paragraphC
  set %expline 20
  set %expcol 60
  set %expchar &asc )
run check-position
21 goto-line
  set %curtest Para3-fill-longline-paragraphD
  set %expline 21
  set %expcol 1
  set %expchar &asc o
run check-position

; Wrap the final paragraph, ending with an overlong line.
; Then fill again and check for no change
;
end-of-file
previous-paragraph
fill-paragraph
  set %curtest FinalPara-CheckNoWrap
  set %expline 29
  set %expcol 1
  set %expchar 10
run check-position

fill-paragraph
  set %curtest FinalPara-CheckNoWrap2
run check-position

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
run check-full-line

6 goto-line
  set %curtest WholeFile-numberlist-B
  set %expltext "  2. This second paragraph will have some unicode characters in it, with"
run check-full-line

17 goto-line
beginning-of-line
  set %curtest WholeFile-numberlist-C
  set %expltext "     2,4-bis(tetra-phenyl-arsonium)-penta-thiocyonata-rhenium(VI)     or"
run check-full-line

unmark-buffer

; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
select-buffer test-reports
newline
insert-string &cat &cat "END: ok: " %ok &cat " fail: " %fail
newline
insert-string &cat %test_name " ended"
EOD

# If running them all, leave - but first write out the buffer if there
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
