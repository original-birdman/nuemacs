;
; NOTE: that ~ is the escape char, so needs to be doubled.
; Comment lines start with ; trailing comments are *not* allowed.
; Leading ^ ties the match to the start of a word.
; These (mostly) work in either upper or lower case (caseset-on).
store-pttable russian
; Need caseset-off here to stop recursive match eating
; sucessive ` or ' (so ```` only produces 1 Ъ as it keeps matching ъ`
; when ignorinfg case)
   caseset-off
   ъ`    Ъ
   ь'    Ь
   caseset-on
   ``    Ъ
   Y     Ы
   ''    Ь
   шцh   щ
   shцh  щ
   shch  щ
; Next map-from is Cyrllic с
   сh    ш
   цh    ч
; Next map-from is Latin с
   ch    ч
   ыu    ю
   yu    ю
   ыa    я
   ya    я
   ыo    ё
   yo    ё
   зh    ж
   zh    ж
   кh    х
   kh    х
   е'    э
   e'    э
   a     а
   b     б
   v     в
   g     г
   d     д
   e     е
   z     з
   i     и
   j     й
   k     к
   l     л
   m     м
   n     н
   o     о
   p     п
   r     р
; Next map-to is Cyrllic с
   s     с
   t     т
   u     у
   f     ф
   x     х
   c     ц
   `     ъ
   y     ы
   '     ь
   q     э
!endm
