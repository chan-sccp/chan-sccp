## Welcome to Chan_SCCP
Travis Continues Integration Status: [![Travis](http://img.shields.io/travis/marcelloceschia/chan-sccp-b.svg?style=flat)](https://travis-ci.org/marcelloceschia/chan-sccp-b)

[![Download Chan-SCCP channel driver for Asterisk](https://sourceforge.net/sflogo.php?type=8&group_id=186378)](https://sourceforge.net/p/chan-sccp-b/): [![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://sourceforge.net/projects/chan-sccp-b/files/latest/download)

Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Prerequisites
- autoconf (>2.6.0)
- automake (>1.10)
- libtool (>2.2.2)
- m4 (>1.4.5)

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

