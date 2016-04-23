## Welcome to Chan_SCCP

[![Coverity](https://img.shields.io/coverity/scan/7795.svg)](https://scan.coverity.com/projects/dkgroot-chan-sccp-b)
[![Coverity](https://img.shields.io/coverity/ondemand/streams/STREAM.svg)](https://scan.coverity.com/projects/dkgroot-chan-sccp-b)
[![Build Status](http://img.shields.io/travis/marcelloceschia/chan-sccp-b.svg?style=flat)](https://travis-ci.org/marcelloceschia/chan-sccp-b)
[![Github Issues](https://img.shields.io/github/issues/marcelloceschia/chan-sccp-b.svg)](http://github.com/marcelloceschia/chan-sccp-b/issues)
[![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://sourceforge.net/projects/chan-sccp-b/files/latest/download)
[![GitHub license](https://img.shields.io/badge/license-GPL-blue.svg)](https://raw.githubusercontent.com/marcelloceschia/chan-sccp-b/master/LICENSE)
[![Github Releases](https://img.shields.io/github/release/marcelloceschia/chan-sccp-b.svg)](https://github.com/marcelloceschia/chan-sccp-b/releases)
[![LauchpadPPA](https://img.shields.io/badge/Ppa-bin-blue.svg)](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa)
[![BuildOpenSuSe](https://img.shields.io/badge/Build-bin-blue.svg)](http://download.opensuse.org/repositories/home:/chan-sccp-b:/)

Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Prerequisites
Make sure you have the following installed on your system:
- c-compiler:
  - gcc >= 4.4  (note: older not supported)
  - clang >= 3.6  (note: older not supported)
- gnu make
- pbx:
  - asterisk >= 1.6.2
  - asterisk >= 11.21 or asterisk >= 13.7 recommended
  - including source headers and debug symbols (asterisk-dev and asterisk-dbg / asterisk-devel and asterisk-debug-info)
- posix applications like sed, awk, tr

### Configuring
    ./configure [....configure flags you prefer...]

### Build and Install
    make
    make install

For documentation, please visit:
- https://sourceforge.net/p/chan-sccp-b/wiki/

- - - 
Developer Note: When you are making changes to the configure.ac or any of the Makefile.am files you should run ./tools/bootstrap.sh afterwards.

