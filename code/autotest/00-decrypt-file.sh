#!/bin/sh
#

# The original input file - so what we expect when we decrypt.
#
cat > Expected-Decrypt <<EOD
This is just a small test file to ensure that the Encrypt mode
continues to work in the same way across uemacs changes, so as
to ensure that any file encrypted in an older version can
still be read in the current one.

Gordon Lack - 08 Jul, 2022.
EOD

# The result of encrypting that file, by-hand, using
# ATestEncryptionString as the key
#
base64 -d > Decrypt-IN <<EOD
gpoOZCm2LwaO52DOqUXdEo0EVN+8dQNZ5bqB5l362hizoDvTbxCiOR7unky+u4tDy56l+bVJ7adJ
T5pP85uxixTSiE3lf1Hz80wSKOu9hEY+QuXiJe+mjm5VDMe0ooMQO1ASyqRhLGK3nXdsL+ohDdTE
cmg7IEIhCPk/dUebZVKf17y2pZeY3Yuco2GrzKeH7I54eY3AUEZDEwwoLlZjksu8J1VUw2BmcYJt
5oeywNHwDByY+CQXrtf8OGBz+ebuUSFVhY/12xKj3ClIsiAhXKfyKVPovRFu21VsZWadByhm8ATp
HHUjwhTVIBK39VX9crSSZsUrIw==
EOD

# Command to get uemacs to read in the encrypted file and write it out,
# decrypted, to Decrypt-OUT
#
cat > uetest.rc <<EOD
add-mode Crypt
read-file Decrypt-IN ATestEncryptionString
delete-mode Crypt
write-file Decrypt-OUT
exit-emacs
EOD

# Do it...
./uemacs -c etc/uemacs.rc -x ./uetest.rc

# Now check that what was written out (Decrypt-OUT) is what was
# expected (Expected-Decrypt).
#
if diff Decrypt-OUT Expected-Decrypt > /dev/null; then
    echo "File decryption test passed"
    rm -f Expected-Decrypt Decrypt-IN Decrypt-OUT uetest.rc
else
    echo "File decryption test FAILED"
    echo "Expected-Decrypt Decrypt-IN Decrypt-OUT and uetest.rc left for viewing"
fi
