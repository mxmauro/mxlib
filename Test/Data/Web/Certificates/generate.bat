@ECHO OFF
"..\..\..\..\Utilities\OpenSSL\openssl.exe" genpkey -algorithm RSA -outform PEM > webserver_ssl_priv_key.pem
REM NOT SUPPORTED YET ON MAJOR BROWSERS "..\..\..\..\Utilities\OpenSSL\openssl.exe" genpkey -algorithm ED25519 -outform PEM > webserver_ssl_priv_key.pem
"..\..\..\..\Utilities\OpenSSL\openssl.exe" req -config webserver.conf -new -x509 -nodes -key webserver_ssl_priv_key.pem -keyform PEM -days 1825 -outform PEM -out webserver_ssl_cert.pem
