## Welcome to Chan_SCCP

[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)
[![Build TR Status](http://img.shields.io/travis/chan-sccp/chan-sccp.svg?style=flat&branch=develop)](https://travis-ci.com/chan-sccp/chan-sccp)
[![Build GH Status](https://github.com/chan-sccp/chan-sccp/workflows/CI/badge.svg)](https://github.com/chan-sccp/chan-sccp/actions?query=workflow%3ACI)
[![Coverity](https://img.shields.io/coverity/scan/8656.svg)](https://scan.coverity.com/projects/chan-sccp)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/chan-sccp/chan-sccp.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/chan-sccp/chan-sccp/context:cpp)
[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/issues "Average time to resolve an issue")
[![Percentage of issues still open](http://isitmaintained.com/badge/open/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/issues?utf8=✓&q=is%3Aopen+is%3Aissue+-label%3Aenhancement+ "Percentage of issues still open")
[![Download Chan-SCCP channel driver for Asterisk](https://img.shields.io/sourceforge/dt/chan-sccp-b.svg)](https://github.com/chan-sccp/chan-sccp/releases/latest)
[![Github Releases](https://img.shields.io/github/release/chan-sccp/chan-sccp.svg)](https://github.com/chan-sccp/chan-sccp/releases)
[![Documentation](https://img.shields.io/badge/docs-wiki-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.svg)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com)
[![Liberapay](https://img.shields.io/liberapay/receives/chan-sccp.svg?logo=liberapay)](https://liberapay.com/chan-sccp/donate).

Chan_SCCP is free software. Please see the file COPYING for details.
For documentation, please see the files in the doc subdirectory.
For building and installation instructions please see the INSTALL file.

### Table of Contents

* [Table of Contents](#table-of-contents)
* [Wiki](#Wiki)
* [Chat](#Chat)
* [Quick Start](#Quick-Start)
  * [Prerequisites](#Prerequisites)
  * [Building from source](#Building-from-source)
  * [Configuring](#Configuring)
  * [Build and Install](#Build-and-Install)
  * [Required Asterisk Modules](#Required-Asterisk-Modules)
* [Binaries](#Binaries)
* [Mailinglist](#Mailinglist)
* [Donate](#Donate)
* [License](#License)

### Wiki
You can find more information and documentation on our [![Wiki](https://img.shields.io/badge/Wiki-new-blue.svg)](https://github.com/chan-sccp/chan-sccp/wiki/).
The wiki contains detailed guides on how to setup and configure chan-sccp, using it directly with plain asterisk or integration into FreePBX.
You can also find dialplan snippets and usefull hints and tips on our wiki.

### Chat
Engage with our members and developers directly via:
[![Gitter](https://badges.gitter.im/chan-sccp/chan-sccp.svg)](https://gitter.im/chan-sccp/chan-sccp?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge).
Looking forward to seeing you there.

## Quick Start
### Prerequisites
Make sure you have the following installed on your system:
- c-compiler:
  - gcc >= 4.6  (note: older not supported, higher advised)
  - clang >= 3.6  (note: older not supported, higher advised)
- gnu make
- libraries:
  - libxml2-dev / libxml2-devel
  - libxslt1-dev / libxslt1-devel
  - gettext
  - libssl-dev / openssl-devel
- pbx:
  - asterisk >= 11 (absolute minimum)
  - asterisk >= 16 or 18 recommended
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
We provide prebuild binaries for:
- [Ubuntu Lauchpad (PPA)](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa)
  This resposity is build for amd64, arm64, armhf, i386, powerpc, ppc64el and s390x

  Note: The Ubuntu PPA Packages are build against the asterisk version delivered with ubuntu on the ppa build platform. The chan-sccp driver
  is compatible only with the exact asterisk version it was build against. Please check the build log, to make sure you have the correct one.
  See [PPA Package Details](https://launchpad.net/~chan-sccp-b/+archive/ubuntu/ppa/+packages).
- [OpenSuSE Build Service](https://build.opensuse.org/project/show/home:chan-sccp-b):
  [OpenSuSE Repository Lookup](https://software.opensuse.org/search?utf8=%E2%9C%93&baseproject=ALL&q=sccp)
  - Asterisk-11
    - [Debian-8.0](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-11/Debian_8.0/)
  - Asterisk-13
    - [Debian-9.0](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Debian_9.0/)
    - [Fedora-23](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_23/)
    - [Fedora-24](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_24/)
    - [Fedora-25](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_25/)
    - [Fedora-26](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/Fedora_26/)
    - [Rasbian 9](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Raspbian_9.0) (armv7l)
    - [Ubuntu-16.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_16.04/)
    - [Ubuntu-16.10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_16.10/)
    - [Ubuntu-17.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_17.04/)
    - [Ubuntu-17.10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_17.10/)
    - [Ubuntu-18.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-13/xUbuntu_18.04/)
  - Asterisk-16 LTS
    - [Debian-10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Debian_10/) (x86_64,i586)
    - [Debian-Testing](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Debian_Testing/) (x86_64)
    - [Debian-Unstable](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Debian_Unstable/) (x86_64)
    - [Fedora-28](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_28/) (x86_64)
    - [Fedora-29](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_29/) (x86_64,i586,armv7l,aarch64,ppc64le)
    - [Fedora-30](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_30/) (x86_64,i586,armv7l,aarch64,ppc64le)
    - [Fedora-31](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Fedora_31/) (x86_64,i586,armv7l,aarch64,ppc64le)
    - [Rasbian 10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/Raspbian_10) (armv7l)
    - [Ubuntu 19.10](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/xUbuntu_19.10) (x86_64)
    - [Ubuntu 20.04](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-16/xUbuntu_20.04) (x86_64)
  - Asterisk-18 LTS
    - [openSUSE-Leap_15.1](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Leap_15.1/) (x86_64)
    - [openSUSE-Leap_15.2](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Leap_15.2/) (x86_64)
    - [openSUSE-Leap_15.3](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Leap_15.3/) (x86_64,aarch64)
    - [openSUSE-Factory](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Factory/) (x86_64,i586)
    - [openSUSE-Factory_ARM](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Factory_ARM/) (aarch64,armv6l,armv7l)
    - [openSUSE_Factory_PPC](https://download.opensuse.org/repositories/home:/chan-sccp-b:/asterisk-18/openSUSE_Factory_PPC/) (ppc64,ppc64le)

### Mailinglist

- chan-sccp Discussion: 
  - [Subscribe](https://lists.sourceforge.net/lists/listinfo/chan-sccp-b-discussion)
  - [Archive](https://sourceforge.net/p/chan-sccp-b/mailman/chan-sccp-b-discussion)
  - [Search](https://sourceforge.net/p/chan-sccp-b/mailman/search/?mail_list=chan-sccp-b-discussion) 

- chan-sccp Releases: 
  - [Subscribe](https://lists.sourceforge.net/lists/listinfo/chan-sccp-b-releases)
  - [Archive](https://sourceforge.net/p/chan-sccp-b/mailman/chan-sccp-b-releases)
  - [Search](https://sourceforge.net/p/chan-sccp-b/mailman/search/?mail_list=chan-sccp-b-releases) 
  
### Donate
If you like our project, then please consider to 
[![donation](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_pp_142x27.png)](https://www.paypal.com/cgi-bin/webscr?item_name=Donation+to+Chan-SCCP+channel+driver+for+Asterisk&locale.x=en_US&cmd=_donations&business=chan.sccp.b.pp%40gmail.com)

### License
[![GitHub license](https://img.shields.io/badge/license-GPL-blue.svg)](https://raw.githubusercontent.com/chan-sccp/chan-sccp/master/LICENSE)

### GPG Key
E774 D1A4 6210 4F41 C844  897E AFA7 2825 0A1B ECD6
