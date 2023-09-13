#!/bin/sh
#

TNAME=`basename $0 .sh`
export TNAME

# The original input file
#
cat > Encrypt-IN <<EOD
This is just a small test file to ensure that the Encrypt mode
continues to work in the same way across uemacs changes, so as
to ensure that any file encrypted in an older version can
still be read in the current one.

Gordon Lack - 08 Jul, 2022.
EOD

# What we expect from  encrypting that file, by-hand, using
# ATestEncryptionString as the key
#
base64 -d > Expected-Encrypt <<EOD
gpoOZCm2LwaO52DOqUXdEo0EVN+8dQNZ5bqB5l362hizoDvTbxCiOR7unky+u4tDy56l+bVJ7adJ
T5pP85uxixTSiE3lf1Hz80wSKOu9hEY+QuXiJe+mjm5VDMe0ooMQO1ASyqRhLGK3nXdsL+ohDdTE
cmg7IEIhCPk/dUebZVKf17y2pZeY3Yuco2GrzKeH7I54eY3AUEZDEwwoLlZjksu8J1VUw2BmcYJt
5oeywNHwDByY+CQXrtf8OGBz+ebuUSFVhY/12xKj3ClIsiAhXKfyKVPovRFu21VsZWadByhm8ATp
HHUjwhTVIBK39VX9crSSZsUrIw==
EOD

# Command to get uemacs to read in the encrypted file and write it out,
# decrypted, to Decrypted-OUT
#
cat > uetest.rc <<EOD
read-file Encrypt-IN
add-mode Crypt
write-file Encrypt-OUT ATestEncryptionString
exit-emacs
EOD

# Do it...set the default uemacs if caller hasn't set one.
[ -z "$UE2RUN" ] && UE2RUN="./uemacs -d etc"
$UE2RUN -x ./uetest.rc

# Now check that what was written out (Encrypt-OUT) is what was
# expected.
# Binary files, so use cmp.
#
if cmp -s Expected-Encrypt Encrypt-OUT; then
    echo "File encryption test passed"
    rm -f Encrypt-IN Encrypt-OUT Expected-Encrypt uetest.rc
else
    echo "File encryption test FAILED"
    echo "Encrypt-IN Encrypt-OUT Expected-Encrypt and uetest.rc left for viewing"
fi
