# FILE: spec file for package chan-sccp (Version trunk)
# COPYRIGHT: https://github.com/chan-sccp/chan-sccp group 2009
# CREATED BY: Diederik de Groot <ddegroot@users.sf.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date: $
# REVISION: $Revision: $

# norootforbuild
# usedforbuild    aaa_base acl alsa alsa-devel attr audit-libs autoconf automake bash bind-libs bind-utils binutils bison blocxx bzip2 coreutils cpio cpp cpp41 cracklib curl curl-devel cvs cyrus-sasl dahdi-linux dagdi-linux-devel dahdi-linux-kmp-default dahdi-tools dahdi-tools-devel db diffutils e2fsprogs expat file filesystem fillup findutils flex gawk gcc gcc-c++ gcc41 gcc41-c++ gdbm gdbm-devel gettext gettext-devel glibc glibc-devel glibc-locale gpm grep groff gzip info insserv irqbalance kernel-default klogd krb5 less libacl libattr libcom_err libgcc41 libgsm libgsm-devel libidn libidn-devel libjpeg libjpeg-devel libltdl libmudflap41 libnscd libogg libpri libss7 libstdc++-devel libstdc++41 libstdc++41-devel libtiff libtiff-devel libtool libvolume_id libxcrypt libzio limal limal-bootloader limal-perl linux-kernel-headers m4 make man mdadm mkinitrd mktemp module-init-tools ncurses ncurses-devel net-tools netcfg openldap2 openldap2-client openldap2-devel openssl openssl-devel pam pam-modules patch pcre perl perl-Bootloader perl-gettext permissions popt postgresql-devel postgresql-libs procinfo procps psmisc pwdutils rcs readline reiserfs rpm sed  speex speex-devel sqlite sqlite-devel strace sysvinit tar tcpd tcpd-devel texinfo timezone udev unixODBC unixODBC-devel unzip util-linux vim zlib zlib-devel

%define		tarsourcedir %(echo "${SOURCEDIR}")
%define 	asteriskver %(echo "asterisk${ASTERISKVER}") 
%define 	rpmversion %(echo "${RPMVERSION}_${RPMBRANCH}") 
%define 	rpmrelease %(echo "${RPMREVISION}")
%define         _varlibdir %{_localstatedir}/lib

%define 	is_mandrake %(test -e /etc/mandrake-release && echo 1 || echo 0)
%define 	is_suse %(test -e /etc/SuSE-release && echo 1 || echo 0)
%define 	is_fedora %(test -e /etc/fedora-release && echo 1 || echo 0)

%define 	dist redhat
%define 	disttag rh
%define 	asteriskver asterisk

%if %is_mandrake
		%define dist mandrake
		%define disttag mdk
		%define asteriskver asterisk
%endif
%if %is_suse
		%define dist suse
		%define disttag suse
		%define asteriskver %(echo "asterisk${ASTERISKVER}") 
%endif
%if %is_fedora
		%define dist fedora
		%define disttag rhfc
		%define asteriskver asterisk
%endif
# %define 	packer %##(finger -lp `echo "$USER"` | head -n 1 | cut -d: -f 3)

Name:           chan-sccp_%{asteriskver}
BuildRequires: 	bison
BuildRequires: 	m4
BuildRequires: 	%{asteriskver}
BuildRequires:  %{asteriskver}-devel

# PreReq:	pwdutils coreutils sed grep

Summary:	chan-sccp
Version: 	%{rpmversion}
Release:  	%{rpmrelease}.%{disttag}
License: 	GPL
Group:          Productivity/Telephony/Servers
Packager: 	%packer% <chan-sccp-b-release@lists.sf.net>
Vendor:         chan-sccp development team <chan-sccp-b-release@lists.sf.net>
URL: 		https://github.com/chan-sccp/chan-sccp
Source: 	%{tarsourcedir}.tar.gz
BuildRoot:      %{_tmppath}/%{name}_%{version}-%{release}-build


%description
chan_sccp-b is the SCCP channel connector for your Asterisk PBX. It is the
continued development of the chan_skinny project. It feature improved support
for sccp dev ices and new feature like multiline, dynamic speeddials, shared 
lines etc.

Documentation is available on the chan-sccp home page
(https://github.com/chan-sccp/chan-sccp) chan-sccp Home Page
(https://github.com/chan-sccp/chan-sccp/wiki) chan-sccp Documentation
(http://sourceforge.net/projects/chan-sccp-b) Old chan-sccp SourceForge Page


Authors:
--------
Anthony Shaw 		: 	anthonyshaw@users.sf.net
Marcello Ceschia 	: 	marcelloceschia@users.sf.net
Scotchy 		: 	scotchy@users.sf.net
David Dederscheck 	: 	davidded@users.sf.net
Andrew Lee 		: 	andrewylee@users.sf.net
Diederik de Groot 	: 	ddegroot@users.sf.net
Federico Santulli 	: 	fsantulli@users.sf.net
Geoff Thornton 		: 	gthornton@users.sf.net
macdiver 		: 	macdiver@users.sf.net
Sebastian Vetter 	:	sebbbl@users.sf.net


%prep
echo Building %{name}-%{version}-%{release}...
%setup -n %{tarsourcedir}
#%patch0 -p0 

%build
autoreconf
CFLAGS="$RPM_OPT_FLAGS"
%configure --enable-conference --enable-distributed-devicestate --enable-video
make  %{?_smp_mflags}

%install
%{__make} install DESTDIR=%{buildroot}
mkdir -p ${rpm_build_root}/etc/asterisk/
%{__cp} conf/sccp.conf %{rpm_build_root}/etc/asterisk/

mkdir -p %{buildroot}%{_varlibdir}/asterisk/documentation/thirdparty/
%{__cp} chan_sccp-en_US.xml %{buildroot}%{_varlibdir}/asterisk/documentation/thirdparty/chan_sccp-en_US.xml

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ChangeLog INSTALL README COPYING AUTHORS 
#doc/html
%{_libdir}/asterisk/modules/chan_sccp.so
%{_sysconfdir}/asterisk/sccp.conf
%{_varlibdir}/asterisk/documentation/thirdparty/chan_sccp-en_US.xml

%changelog -n chan-sccp
* Thu Nov 9 2017 Diederik de Groot <ddegroot@talon.nl>
- Added chan_sccp-en_US.xml
* Wed Dec 2 2015 Steven Haigh <netwiz@crc.id.au>
- Used entries from Steven
* Tue Nov 10 2015 Diederik de Groot <ddegroot@talon.nl>
- Updates
* Mon Apr 12 2010 Diederik de Groot <ddegroot@talon.nl>
- Modified for asterisk 1.4.30


###
### eof
###

