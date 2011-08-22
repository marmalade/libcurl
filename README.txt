libcurl on airplay
==================

This module can be used to build the standard libcurl library into airplay
applications.

The source is downloaded from:

    http://curl.haxx.se/download/curl-7.21.7.tar.gz

Compilation requires some patches included into the package.

External c-ares asynchronous resolver has been added as an option.
NOTE THAT THE C-ARES REQUIRES data/etc/resolv.conf FILE EXISTS for Marmalade environment!

Custom asynchronous resolver replacement is used as a DEFAULT option
to utilize Marmalade-native s3eLookup call.

Copy one of libcurl-fake-ares.mkf or libcurl-cares.mkf to libcurl.mkf file to
choose appropriate library option.

The included example demonstrates power of libcurl downloading lot of files
using request queue to restrict number of simultaneous downloads.

------------ NOTE -------------------
The libcurl porting project requires a configure extension for Marmalade/AirplaySDK,
find it at http://github.com/marmalade/Configure-Extension and install first
