## Welcome to Chan_SCCP

[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Build Status](http://img.shields.io/travis/chan-sccp/chan-sccp.svg?style=flat&branch=develop)](https://travis-ci.org/chan-sccp/chan-sccp)
[![Coverity](https://img.shields.io/coverity/scan/8656.svg)](https://scan.coverity.com/projects/chan-sccp)
[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/issues "Average time to resolve an issue")
[![Percentage of issues still open](http://isitmaintained.com/badge/open/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/issues?utf8=✓&q=is%3Aopen+is%3Aissue+-label%3Aenhancement+ "Percentage of issues still open")
[![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://github.com/chan-sccp/chan-sccp/releases/latest)
[![Github Releases](https://img.shields.io/github/release/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/releases)
[![Documentation](https://img.shields.io/badge/docs-wiki-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com).

<!--
[![Github Issues](https://img.shields.io/github/issues/chan-sccp/chan-sccp/bug.svg)](https://github.com/chan-sccp/chan-sccp/issues)
-->

Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Prerequisites
Make sure you have the following installed on your system:
- c-compiler:
  - gcc >= 4.4  (note: older not supported, higher advised)
  - clang >= 3.6  (note: older not supported, higher advised)
- gnu make
- libraries:
  - libxml2-dev
  - libxslt1-dev
  - gettext
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

### Required Asterisk Modules

Make sure you have the following asterisk modules loaded before loading the chan_sccp
module:
 - app_voicemail
 - bridge_simple
 - bridge_native_rtp
 - bridge_softmix
 - bridge_holding
 - res_stasis
 - res_stasis_device_state

### Binaries
We also provide prebuild binaries for:
- [Ubuntu Lauchpad (PPA)](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa)

  Note: The Ubuntu PPA Packages are build against the asterisk version delivered with ubuntu on the ppa build platform. The chan-sccp driver
  is compatible only with the exact asterisk version it was build against. Please check the build log, to make sure you have the correct one.
  See [PPA Package Details](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa/+packages).
- Asterisk-11
  - [Debian-8.0](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-11/Debian_8.0/)
- Asterisk-13
  - [Debian-9.0](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Debian_9.0/)
  - [Fedora-23](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_23/)
  - [Fedora-24](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_24/)
  - [Fedora-25](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_25/)
  - [Fedora-26](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_26/)
  - [Ubuntu-16.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_16.04/)
  - [Ubuntu-16.10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_16.10/)
  - [Ubuntu-17.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_17.04/)
  - [Ubuntu-17.10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_17.10/)
  - [Ubuntu-18.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_18.04/)
- Asterisk-16
  - [Debian-10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Debian_10/)
  - [Fedora-28](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_28/)
  - [Fedora-29](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_29/)
  - [Fedora-30](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_30/)
  - [Fedora-Rawhide](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_Rawhide/) (unstable)
  - [openSUSE-Leap_15.0](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Leap_15.0/)
  - [openSUSE-Leap_42.2](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Leap_42.2/)
  - [openSUSE-Leap_42.3](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Leap_42.3/)
  - [openSUSE-Factory](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Factory/)
  - [openSUSE-Factory_ARM](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Factory_ARM/)
  - [openSUSE_Factory_PPC](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/openSUSE_Factory_PPC/)

### Wiki
You can find more information and documentation on our [![Wiki](https://img.shields.io/badge/Wiki-new-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki/)

### Mailinglist

- chan-sccp Discussion: 
  - [Subscribe](https://lists.sourceforge.net/lists/listinfo/chan-sccp-b-discussion)
  - [Archive](https://sourceforge.net/p/chan-sccp-b/mailman/chan-sccp-b-discussion)
  - [Search](https://sourceforge.net/p/chan-sccp-b/mailman/search/?mail_list=chan-sccp-b-discussion) 

- chan-sccp Releases: 
  - [Subscribe](https://lists.sourceforge.net/lists/listinfo/chan-sccp-b-releases)
  - [Archive](https://sourceforge.net/p/chan-sccp-b/mailman/chan-sccp-b-releases)
  - [Search](https://sourceforge.net/p/chan-sccp-b/mailman/search/?mail_list=chan-sccp-b-releases) 
  
### Chat
[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

### Donate
If you like our project, then please consider to 
[![donation](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_pp_142x27.png)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com)

### License
[![GitHub license](https://img.shields.io/badge/license-GPL-blue.svg)](https://raw.githubusercontent.com/chan-sccp/chan-sccp/master/LICENSE)
