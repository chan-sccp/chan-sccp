## Welcome to Chan_SCCP
Travis Continues Integration Status: [![Travis](http://img.shields.io/travis/marcelloceschia/chan-sccp-b.svg?style=flat)](https://travis-ci.org/marcelloceschia/chan-sccp-b)

Coverity: [![Coverity](https://img.shields.io/coverity/scan/7795.svg)](https://scan.coverity.com/projects/dkgroot-chan-sccp-b) 

[![Download Chan-SCCP channel driver for Asterisk](https://sourceforge.net/sflogo.php?type=8&group_id=186378)](https://sourceforge.net/p/chan-sccp-b/): [![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://sourceforge.net/projects/chan-sccp-b/files/latest/download)


Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Prerequisites
Make sure you have the following installed on your system:
- gcc >= 4.4 or clang >= 3.6
  note: older compilers are not supported !
- gnu make
- posix applications like sed, awk, tr
- asterisk-1.6.2 or higher
  note: asterisk-11 or asterisk-13 recommended

### Configuring
    ./configure

### Build and Install
    make
    make install

### Build Documentation
    make doxygen

For documentation, please visit:
- https://sourceforge.net/p/chan-sccp-b/wiki/
- http://chan-sccp-b.sourceforge.net/doc/index.xhtml

- - - 
Note: When you are making changes to the configure.ac or any of the Makefile.am files you should run ./tools/bootstrap.sh afterwards.

