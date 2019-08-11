#!/bin/bash

failureHints=''
echo "Generating x509 cerificates and session token..."

rm res/x509.cert res/x509.key res/session.token 2> /dev/null

#generate x509
openssl req -nodes -new -x509 -keyout res/x509.key -out res/x509.cert -subj '/CN=domain/O=owner/C=NT' 2> /dev/null

if [ -f res/x509.key ] && [ -f res/x509.cert ]; then
  failureHints+='x509 SUCCESS. '
else
  failureHints+='x509 FAILED. '
fi

#generate base session token
sha512sum daemon/keydefs.h | head -n1 | sed -e 's/\s.*$//' > res/session.token

if [ -f res/session.token ]; then
  failureHints+='token SUCCESS. '
else
  failureHints+='token FAILED. '
fi

echo "Summary: $failureHints"
