libcurl on airplay
==================

This module can be used to build the standard libcurl library into airplay
applications.

The source is downloaded from:

    http://curl.haxx.se/download/curl-7.21.3.tar.gz

Compilation requires some patches included into the package.

Custom c-ares asynchronous resolver replacement has been added to utilize s3eLookup call.

The included example will get two files from the Internet.

NOTE: needs a Configure-Extension for Marmalade/AirplaySDK!
