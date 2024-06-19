#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# The original input file
#
cat > Encrypt-IN <<'EOD'
This is just a small test file to ensure that the Encrypt mode
continues to work in the same way across uemacs changes, so as
to ensure that any file encrypted in an older version can
still be read in the current one.

Gordon Lack - 08 Jul, 2022.
EOD

# The command we will use to check results (hence the ./)
#
SCR_NAME="./comp-file.sh"
export SCR_NAME
cat > $SCR_NAME <<'EOD'
#/bin/sh
#
file="$1"
if cmp -s "$file" "${file}-NOW"; then
    rm -f "$file" "${file}-NOW"
    res=0
else
    echo FAILED     # And leave files there
    res=1
fi
exit $res
EOD
chmod +x $SCR_NAME

# What we expect from  encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to various
# values
#
base64 -d > Encrypt-nUemacs <<'EOD'
V2hQaG0wTXU0XTE4SFpvIDNlTWd1TSRtOnNEPyRPSFkwbDh5Zi8kW35+fmR2PGNEfEVLImp9YlB7
X3l8MioKX0g3KDsncSFQPz9Dcnk6OlE6K3woTHJ2WHs0I3ooaEgwSWlqY2ddeCFKMTR0NlBPKCB+
OSF7TmEuPG5tUGkKM3FMLjttWjcnd09PdC12ZGU0JzR7cHBCbCt6UTkmdmUlPylNK201NGdeYSpb
XGBhIyQ7SzMqeTY8CkFLZGRmQk9XQSc6bV5aISEiZDFfZW50P3lcdTdaQEt3awoKd215UHZKSkBZ
OmF1LjQ+Z10kdUtNLjI9c0IqCg==
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to 0x3002
#
base64 -d > Encrypt-pre-GGR4.120 <<'EOD'
V2hQaG0wTXU0XTE4SFpvIDNlTWd1TSRtOnNEPyRPSFkwbDh5Zi8kW35+fmR2PGNEfEVLImp9YlB7
X3l8MioKX0g3KDsncSFQPz9Dcnk6OlE6K3woTHJ2WHs0I3ooaEgwSWlqY2ddeCFKMTR0NlBPKCB+
OSF7TmEuPG5tUGkKM3FMLjttWjcnd09PdC12ZGU0JzR7cHBCbCt6UTkmdmUlPylNK201NGdeYSpb
XGBhIyQ7SzMqeTY8CkFLZGRmQk9XQSc6bV5aISEiZDFfZW50P3lcdTdaQEt3awoKd215UHZKSkBZ
OmF1LjQ+Z10kdUtNLjI9c0IqCg==
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to 0x02
#
base64 -d > Encrypt-GGR4.120 <<'EOD'
kn/RD7g4eh6s2jeDJY8Rg9NCYqUzriRMtWBJk/dt/hmLLbggb+RLyWVY2lSrf6s4sGSNhPNSuT/F
kDK9V9WsQY8Cbgt0AZUL8Bm2ozLKR+CkSLqrVPCNV51a8aFgeT/X4hnALuZ5Ki/0rDsDhwUI14Ur
uoMc0sl76rLNNuEBflJy5XxJFdq1vsOadCtd+L6RwB792sbDLem9mHRnQCj0EyPoCRLiBPfy897M
GTQxEfn+/AAwVk8reRIFB/H6UgfmDCc3P1Of7dxD0uneQN3sDS5bd5cAGUNVok8bRNoGMkZZ2tiy
zt1gTnWNPAt3i9ROe6RYjVOGUQ==
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to 0x01
#
base64 -d > Encrypt-GGR4.121 <<'EOD'
E8KrkON5e+F3iKOoFF26ABJBHkOX2g8MN6baAwcdj8z9gbbTBixkmRVpr9Tsel2myjO72CcwWJ7j
c1un4fum/kV6xxhfsAUdy6sNmhdvrRbFrOh6guQquEKo3lT0+H/ZmT+yAGCzFNHbaLk/vCv2I2nH
D3wGUAn6KLCNNqSN1FY4+ob2exapZHUIfgj1QJkwIcIyzFQs6mr+mzrohTDPpVwG5yutkmL/w07f
7zgImVIImV9y3KUsORPHjzsLBuN5jGg/GubRRvsl+NqxwB/tz7KKRwcyMh0PEJ/NHqZyVy0AIS7u
xIOeCWwzkBljDxhUXUpHOrYx3w==
EOD

# Command to get uemacs to read in the file and write it out
# under various encrypted modes.
# NEED TO FIGURE OUT HOW TO TEST OUTPUT FILES IN UEMACS
# WHAT ABOUT ONE OF THE SPAWN COMMAND TO RUN cmp???
#
cat > uetest.rc <<'EOD'
; Some uemacs code to run tests on testfile
; Put test results into a specific buffer (test-reports)...
; ...and switch to that buffer at the end.

; After a search I need to check that $curcol, $curline $curchar and
; $matchlen are what I expect them to be.
;
; -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-

execute-file autotest/report-status.rc

set %test_name &env TNAME

select-buffer test-reports
insert-string &cat %test_name " started"
newline
set %fail 0
set %ok 0

; Define a function to compare two buffers.
; Good enough for the small ones we have here...
; No way to pass parematers here, so we'll compare; %tbuf1 with $tbuf2
;
store-procedure compare-buffers

  set .sbuf $cbufname
; Set a sanity limit - just in case...
  set .lc 100
  set .res FAILED
  select-buffer %tbuf1
  beginning-of-file
  select-buffer %tbuf2
  beginning-of-file

  !while &gre .lc 0
    select-buffer %tbuf1

    set .b1line $line
    !force next-line
    set .b1eof $force_status

    select-buffer %tbuf2

    set .b2line $line
    !force next-line
    set .b2eof $force_status

    !if &not &seq .b1eof .b2eof
        !break
    !endif
    !if &not &seq .b1line .b2line
        !break
    !endif

    !if &seq .b1eof FAILED
        set .res OK
        !break
    !endif

    set .lc &sub .lc 1

  !endwhile

  select-buffer .sbuf
  !if &seq .res OK
    !return
  !else
    !finish
  !endif
!endm

store-procedure check-encrypt
  shell-command &ptf "%s %s" &env "SCR_NAME" %test_name
  !if &equ $rval 0
      set %test-report &cat %test_name ": OK"
      set %ok &add %ok 1
  !else
      set %test-report &cat %test_name ": FAILED"
      set %fail &add %fail 1
  !endif
  run report-status
!endm

; Read in the original file
select-buffer ORIG
read-file Encrypt-IN
;
; Write this out using nUmeacs crypt mode
set %test_name Encrypt-nUemacs
set $crypt_mode 0x3001
add-mode Crypt
set-encryption-key ATestEncryptionString
write-file &cat %test_name "-NOW"
; Run a filter to compare what we wrote vs what we expext
run check-encrypt
!force kill-buffer %test_name

; Now Encrypt-pre-GGR4.120
;
; We'll do this one by re-using the same buffer.
; This will test re-encrypting the key under the new mode.
; We have to remove the previous contents of TEST first
;
set %test_name Encrypt-pre-GGR4.120
select-buffer ORIG
set $crypt_mode 0x3002
write-file &cat %test_name "-NOW"
run check-encrypt
!force kill-buffer %test_name

; Read this one from a new buffer, giving the encryption
; key as part of the read-file command
;
set %test_name Encrypt-GGR4.120
select-buffer NOT-ORIG
delete-buffer ORIG
select-buffer ORIG
read-file Encrypt-IN
set $crypt_mode 0x2
add-mode Crypt
write-file &cat %test_name "-NOW" ATestEncryptionString
run check-encrypt
!force kill-buffer %test_name

; For this one we'll set the encryption key separately
;
set %test_name Encrypt-GGR4.121
select-buffer NOT-ORIG
delete-buffer ORIG
select-buffer ORIG
read-file Encrypt-IN
set $crypt_mode 0x1
add-mode Crypt
set-encryption-key ATestEncryptionString
write-file &cat %test_name "-NOW"
run check-encrypt
!force kill-buffer %test_name

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
    cat >>uetest.rc <<EOD           # Allow var expansion here!
!if &not &equ %fail 0
    set \$cfname "FAIL-$TNAME"
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
        echo "Files left: " *Encrypt*
    else
        echo "$TNAME passed"
        rm -f Encrypt-IN
    fi
fi
rm -f $SCR_NAME
