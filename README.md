## Welcome to Chan_SCCP

[![Build Status](http://img.shields.io/travis/chan-sccp/chan-sccp.svg?style=flat)](https://travis-ci.org/chan-sccp/chan-sccp)
[![Coverity](https://img.shields.io/coverity/scan/8656.svg)](https://scan.coverity.com/projects/chan-sccp)
[![Github Issues](https://img.shields.io/github/issues/chan-sccp/chan-sccp/bug.svg)](https://github.com/chan-sccp/chan-sccp/issues)
[![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://github.com/chan-sccp/chan-sccp/releases/latest)
[![Github Releases](https://img.shields.io/github/release/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/releases)
[![Documentation](https://img.shields.io/badge/docs-wiki-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki)
[![LauchpadPPA](https://img.shields.io/badge/Ppa-bin-blue.svg)](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa)
[![BuildOpenSuSE](https://img.shields.io/badge/Build-bin-blue.svg)](http://download.opensuse.org/repositories/home:/chan-sccp-b:/)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com).

Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Prerequisites
Make sure you have the following installed on your system:
- c-compiler:
  - gcc >= 4.4  (note: older not supported, higher advised)
  - clang >= 3.6  (note: older not supported, higher advised)
- gnu make
- pbx:
  - asterisk >= 1.6.2 (absolute minimum)
  - asterisk >= 11.21 or asterisk >= 13.7 recommended
  - including source headers and debug symbols (asterisk-dev and asterisk-dbg / asterisk-devel and asterisk-debug-info)
- standard posix compatible applications like sed, awk, tr

### Configuring
    ./configure [....configure flags you prefer...]

For more information about the possible configure flags, check:

    ./configure --help 

Note: When you are making changes to configure.ac, autoconf / or Makefile.am files you should run:

    ./tools/bootstrap.sh

### Build and Install
    make
    make install

### Wiki
You can find more information and documentation on our [![Wiki](https://img.shields.io/badge/Wiki-old-blue.svg)](https://sourceforge.net/p/chan-sccp-b/wiki/Home/)
[![Wiki](https://img.shields.io/badge/Wiki-new-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki/)

### Donate
If you like our project, then please consider to 
[![donation](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_pp_142x27.png)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com)

### License
[![GitHub license](https://img.shields.io/badge/license-GPL-blue.svg)](https://raw.githubusercontent.com/chan-sccp/chan-sccp/master/LICENSE)
