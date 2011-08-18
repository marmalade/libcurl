libcurl on airplay
==================

This module can be used to build the standard libcurl library into airplay
applications.

The source is downloaded from:

    http://curl.haxx.se/download/curl-7.21.3.tar.gz

Compilation requires some patches included into the package.

External c-ares asynchronous resolver has been added to the DEFAULT build.
NOTE THAT THE C-ARES REQUIRES data/etc/resolv.conf FILE EXISTS for Marmalade environment!

Custom c-ares asynchronous resolver replacement may be used to utilize Marmalade-native s3eLookup call.
Just copy libcurl-fake-ares.mkf to the libcurl.mkf file to attach custom resolver instead of c-ares.
Original libcurl.mkf is just a copy of the libcurl-cares.mkf.

The included example will get two files from the Internet.

NOTE: needs a Configure-Extension for Marmalade/AirplaySDK!
