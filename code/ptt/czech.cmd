;
; NOTE: that ~ is the escape char, so needs to be doubled.
; Comment lines start with ; trailing comments are *not* allowed.
; Leading ^ ties the match to the start of a word.
; These work in either upper or lower case (caseset-on).
store-pttable czech
  a'     U+00e1
  e'     U+00e9
  i'     U+00ed
  o'     U+00f3
; Next one is special at start of a word.
  ^u'    U+00fa
  u'     U+016f
  y'     U+00fd
  c'     U+010d
  d'     U+010f
  n'     U+0148
  r'     U+0159
  s'     U+0161
  t'     U+0165
  z'     U+017e
  c~~    U+010d
  d~~    U+010f
  e~~    U+0115
  n~~    U+0148
  r~~    U+0159
  s~~    U+0161
  t~~    U+0165
  z~~    U+017e
!endm

