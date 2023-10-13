#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

rm -f FAIL-$TNAME

# The original input file - so what we expect when we decrypt.
#
cat > Expected-Decrypt <<'EOD'
This is just a small test file to ensure that the Encrypt mode
continues to work in the same way across uemacs changes, so as
to ensure that any file encrypted in an older version can
still be read in the current one.

Gordon Lack - 08 Jul, 2022.
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to 0x3001
#
base64 -d > Decrypt-nUemacs <<'EOD'
V2hQaG0wTXU0XTE4SFpvIDNlTWd1TSRtOnNEPyRPSFkwbDh5Zi8kW35+fmR2PGNEfEVLImp9YlB7
X3l8MioKX0g3KDsncSFQPz9Dcnk6OlE6K3woTHJ2WHs0I3ooaEgwSWlqY2ddeCFKMTR0NlBPKCB+
OSF7TmEuPG5tUGkKM3FMLjttWjcnd09PdC12ZGU0JzR7cHBCbCt6UTkmdmUlPylNK201NGdeYSpb
XGBhIyQ7SzMqeTY8CkFLZGRmQk9XQSc6bV5aISEiZDFfZW50P3lcdTdaQEt3awoKd215UHZKSkBZ
OmF1LjQ+Z10kdUtNLjI9c0IqCg==
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key with $crypt_mode set to 0x3002
#
base64 -d > Decrypt-pre-GGR4.120 <<'EOD'
V2hQaG0wTXU0XTE4SFpvIDNlTWd1TSRtOnNEPyRPSFkwbDh5Zi8kW35+fmR2PGNEfEVLImp9YlB7
X3l8MioKX0g3KDsncSFQPz9Dcnk6OlE6K3woTHJ2WHs0I3ooaEgwSWlqY2ddeCFKMTR0NlBPKCB+
OSF7TmEuPG5tUGkKM3FMLjttWjcnd09PdC12ZGU0JzR7cHBCbCt6UTkmdmUlPylNK201NGdeYSpb
XGBhIyQ7SzMqeTY8CkFLZGRmQk9XQSc6bV5aISEiZDFfZW50P3lcdTdaQEt3awoKd215UHZKSkBZ
OmF1LjQ+Z10kdUtNLjI9c0IqCg==
EOD

# The result of encrypting that file, by-hand, using
# SecondTestEncryptionString as the key with $crypt_mode set to 0x02
#
base64 -d > Decrypt-GGR4.120 <<'EOD'
Gy921YLpH98DQJ4O0+1+nhmX9GcMjRRv7bxx31/pjhyhVofXW9xm7qEhqRWAUHIAbCTiIqYDgBut
nGrpdwnlEpQ935Mx1X4dHvaZmGUPoEEbAYlxu4MaHQO8axQGv5k5TKtK3apSBhcAzG8s14qojDwT
xoMv2/fgLhJBpkWBEur+nXleRiQMGywh66DRZx8KUOOnfHi117vDemptZmY6gIBXrdK2DCQVJgLV
KDdMLkBILSZrGi8foKvA1vQRfVpxyXOQqaTdvOA6i8fbUicZTYOvvu99ZYOq/2J4cny8DCBHvFJE
WJQrw/47+12BQYr9W5hptHzZWQ==
EOD

# The result of encrypting that file, by-hand, using
# ThirdTestEncryptionString as the key with $crypt_mode set to 0x01
#
base64 -d > Decrypt-GGR4.121 <<'EOD'
GNfw80ju4jqEeJiwEFat2/80P2i4AzA7YM+bycziLWuoDl+Qw+URRJTqNHeXH5DN9GxpZ57bGmik
MpzoJkfGYXTM/V60B2SAGp3/sB6JyzDa7zPr21O8a1/iD2n8E47MmEKCtSN8+cKxLYLvUKyFFIb0
XOxZ046HE49Pf//8JatxqBWqPNZuMeF5FpV5z0a/v6ATsmEeA3gvsD/6nl8V+l0M8EvQ2Tv8vHUU
FvG1WBPLZhAnAJZHWHQy+8JykYgyP0Uc9r/ecDxmwJZ5nLd5X0cr66/uajguNchOoTstMQfeIjUW
ALv64s+eFarYaHO5SROH/gaFFw==
EOD

# Command to get uemacs to read in the encrypted file and write it out,
# decrypted, to Decrypt-OUT
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

store-procedure check-decrypt
  !force execute-procedure compare-buffers
  !if &seq PASSED $force_status
    set %test-report &cat %test_name ": OK"
    set %ok &add %ok 1
    shell-command &cat "rm -f " %test_name
  !else
    set %test-report &cat %test_name ": FAILED"
    set %fail &add %fail 1
  !endif
  execute-procedure report-status
!endm

; Get the expected results into buffers now...before CRYPT is on
find-file Expected-Decrypt
set %tbuf1 Expected-Decrypt

; Read in the first test, setting a global encryption key
;
set %test_name Decrypt-nUemacs
select-buffer TEST
set $crypt_mode 0x7001
add-mode Crypt
set-encryption-key ATestEncryptionString
read-file %test_name
set %tbuf2 TEST
select-buffer test-reports
execute-procedure check-decrypt

; We'll do this one by re-using the same buffer.
; This will test the global encryption key under the new mode.
; We have to remove the previous TEST buffer first
;
set %test_name Decrypt-pre-GGR4.120
delete-buffer TEST
select-buffer TEST
set $crypt_mode 0x7002
add-mode Crypt
read-file %test_name
set %tbuf2 TEST
select-buffer test-reports
execute-procedure check-decrypt

; Read this one into its own new buffer, with crypt_mode set to use a
; per-buffer key, but giving that as part of the read-file command
;
set %test_name Decrypt-GGR4.120
select-buffer %test_name
set $crypt_mode 0x2
add-mode Crypt
read-file %test_name SecondTestEncryptionString
set %tbuf2 %test_name
select-buffer test-reports
execute-procedure check-decrypt

; For this one we'll set the (per-buffer) encryption key separately
; before reading in the file.
;
set %test_name Decrypt-GGR4.121
select-buffer %test_name
set $crypt_mode 0x1
add-mode Crypt
set-encryption-key ThirdTestEncryptionString
read-file %test_name
set %tbuf2 %test_name
select-buffer test-reports
execute-procedure check-decrypt
;
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
        echo "Files left: " *Decrypt*
    else
        echo "$TNAME passed"
        rm -f Expected-Decrypt
    fi
fi
