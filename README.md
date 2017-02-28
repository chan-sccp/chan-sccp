## Welcome to Chan_SCCP

[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Build Status](http://img.shields.io/travis/chan-sccp/chan-sccp.svg?style=flat&branch=develop)](https://travis-ci.org/chan-sccp/chan-sccp)
[![Coverity](https://img.shields.io/coverity/scan/8656.svg)](https://scan.coverity.com/projects/chan-sccp)
[![Github Issues](https://img.shields.io/github/issues/chan-sccp/chan-sccp/bug.svg)](https://github.com/chan-sccp/chan-sccp/issues)
[![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://github.com/chan-sccp/chan-sccp/releases/latest)
[![Github Releases](https://img.shields.io/github/release/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/releases)
[![Documentation](https://img.shields.io/badge/docs-wiki-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki)
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
  - chan_skinny module is prevented from loading in /etc/asterisk/modules.conf
- standard posix compatible applications like sed, awk, tr

### Building from source
#### Using git (recommended)
##### Clone github repository (once)
    git clone https://github.com/chan-sccp/chan-sccp.git chan-sccp
    cd chan-sccp

##### Update to latest state
    cd chan-sccp
    git fetch
    git pull

#### Using Released tar.gz
retrieve the tar.gz from [latest release](https://github.com/chan-sccp/chan-sccp/releases/latest) and save it to /tmp/chan-sccp_latest.tar.gz

    mkdir chan-sccp
    cd chan-sccp
    tar xvfz /tmp/chan-sccp_latest.tar.gz

### Configuring
    ./configure [....configure flags you prefer...]

_Note: For more information about the possible configure flags, check:_
    ./configure --help 

_Note: When you are making changes to configure.ac, autoconf / or Makefile.am files you should run:_
    ./tools/bootstrap.sh

### Build and Install
    make -j2 && make install && make reload

### Binaries
We also provide prebuild binaries for:
- [Ubuntu Lauchpad (PPA)](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa)
- Asterisk-11
  - [Debian-8.0](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-11/Debian_8.0/)
  - [OpenSuSE-13.2](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-11/openSUSE_13.2/)
- Asterisk-13
  - [OpenSuSE-13.2](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/openSUSE_13.2/)
  - [OpenSuSE-LEAP4.2](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/openSUSE_Leap_42.1/)
  - [Fedora-23](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_23/)
  - [Fedora-24](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_24/)
  - [Fedora-25](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_25/)
- Asterisk-14
  - [OpenSuSE-13.2](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-14/openSUSE_13.2/)
  - [OpenSuSE-LEAP4.2](http://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-14/openSUSE_Leap_42.1/)

### Wiki
You can find more information and documentation on our [![Wiki](https://img.shields.io/badge/Wiki-new-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki/)

### Chat
[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

### Donate
If you like our project, then please consider to 
[![donation](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_pp_142x27.png)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com)

### License
[![GitHub license](https://img.shields.io/badge/license-GPL-blue.svg)](https://raw.githubusercontent.com/chan-sccp/chan-sccp/master/LICENSE)
