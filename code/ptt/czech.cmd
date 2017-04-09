;
; NOTE: that ~ is the escape char, so needs to be doubled.
; Comment lines start with ; trailing comments are *not* allowed.
; Leading ^ ties the match to te start of a word.
;  (currently may not work perfectly as uemacs doesn't know about
;   non-ASCII letters yet)
;
store-pttable czech
  A'     U+00c1
  a'     U+00e1
  E'     U+00c9
  e'     U+00e9
  E~~    U+0114
  e~~    U+0115
  I'     U+00cd
  i'     U+00ed
  O'     U+00d3
  o'     U+00f3
  U'     U+00da
  ^u'    U+00fa
  u'     U+016f
  Y'     U+00dd
  y'     U+00fd
  C'     U+010c
  c'     U+010d
  D'     U+010e
  d'     U+010f
  N'     U+0147
  n'     U+0148
  R'     U+0158
  r'     U+0159
  S'     U+0160
  s'     U+0161
  T'     U+0164
  t'     U+0165
  Z'     U+017d
  z'     U+017e
  C~~    U+010c
  c~~    U+010d
  D~~    U+010e
  d~~    U+010f
  N~~    U+0147
  n~~    U+0148
  R~~    U+0158
  r~~    U+0159
  S~~    U+0160
  s~~    U+0161
  T~~    U+0164
  t~~    U+0165
  Z~~    U+017d
  z~~    U+017e
!endm

