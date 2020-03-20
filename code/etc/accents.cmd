;
; Just to make it simpler to add accented character if you forget
; or aren't used to/can't remember the AltGr mapping.
; This contains most of the European Scripts/Latin-1 Supplement chars
; The Latin Extended-A section contains many others, but nothing
; obviously used by Western European languages.
; The mappings are to a single Unicode point, not char+combining accent.
; So you may need Equiv mode for searching.
;
store-pttable accents
;
    caseset-on
; A's (either case)
    a`  à
    a'  á
    a^  â
    a~  ã
    a"  ä
    a0  å
; C (either case)
    c+  ç
; E's (either case)
    e`  è
    e'  é
    e^  ê
    e"  ë
; I's (either case)
    i`  ì
    i'  í
    i^  î
    i"  ï
; O's (either case)
    o`  ò
    o'  ó
    o^  ô
    o~  õ
    o"  ö
    o/  ø
; U's (either case)
    u`  ù
    u'  ú
    u^  û
    u"  ü
; y (either case)
    y'  ý
; y (lowercase only)
    case-set off
    y"  ÿ
; n (either case)
    case-set on
    n~  ñ
; Misc
; Spanish
    ??  ¿
; Eth
    d-  ð
; Thorn
    t+  þ
!endm
