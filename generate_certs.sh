#!/bin/bash

failureHints=''
echo "Generating x509 certificates and session token..."

# if the user ran ./build-all before gcc/Qt is installed, or if they change keys, this should change.
rm res/session.token 2> /dev/null

#generate base session token
sha512sum daemon/keydefs.h | head -n1 | sed -e 's/\s.*$//' > res/session.token

if [ -f daemon/keydefs.h ] || [ -f res/session.token ]; then
  failureHints+='token SUCCESS. '
else
  failureHints+='token FAILED. '
fi

#generate x509 if necessary
if [ ! -f res/x509.cert ] || [ ! -f res/x509.key ]; then
  openssl req -nodes -new -x509 -keyout res/x509.key -out res/x509.cert -subj '/CN=domain/O=owner/C=NT' 2> /dev/null
fi

if [ -f res/x509.key ] && [ -f res/x509.cert ]; then
  failureHints+='x509 SUCCESS. '
else
  failureHints+='x509 FAILED. '
fi

echo "Summary: $failureHints"
