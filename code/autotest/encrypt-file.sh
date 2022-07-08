#!/bin/sh
#

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
E8KrkON5e+F3iKOoFF26ABJBHkOX2g8MN6baAwcdj8z9gbbTBixkmRVpr9Tsel2myjO72CcwWJ7j
c1un4fum/kV6xxhfsAUdy6sNmhdvrRbFrOh6guQquEKo3lT0+H/ZmT+yAGCzFNHbaLk/vCv2I2nH
D3wGUAn6KLCNNqSN1FY4+ob2exapZHUIfgj1QJkwIcIyzFQs6mr+mzrohTDPpVwG5yutkmL/w07f
7zgImVIImV9y3KUsORPHjzsLBuN5jGg/GubRRvsl+NqxwB/tz7KKRwcyMh0PEJ/NHqZyVy0AIS7u
xIOeCWwzkBljDxhUXUpHOrYx3w==
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

# Do it...
./uemacs -c etc/uemacs.rc -x ./uetest.rc

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
echo "press <CR> to continue"
read dnc
