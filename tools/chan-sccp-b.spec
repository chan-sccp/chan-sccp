# FILE: spec file for package chan-sccp-b (Version trunk)
# COPYRIGHT: chan-sccp-b.sourceforge.net group 2009
# CREATED BY: Diederik de Groot <ddegroot@sourceforge.net>
# LICENSE: This program is free software and may be modified and distributed under the terms of the GNU Public License version 3.
#          See the LICENSE file at the top of the source tree.
# DATE:     $Date: $
# REVISION: $Revision: $

# norootforbuild
# usedforbuild    aaa_base acl alsa alsa-devel attr audit-libs autoconf automake bash bind-libs bind-utils binutils bison blocxx bzip2 coreutils cpio cpp cpp41 cracklib curl curl-devel cvs cyrus-sasl dahdi-linux dagdi-linux-devel dahdi-linux-kmp-default dahdi-tools dahdi-tools-devel db diffutils e2fsprogs expat file filesystem fillup findutils flex gawk gcc gcc-c++ gcc41 gcc41-c++ gdbm gdbm-devel gettext gettext-devel glibc glibc-devel glibc-locale gpm grep groff gzip info insserv irqbalance kernel-default klogd krb5 less libacl libattr libcom_err libgcc41 libgsm libgsm-devel libidn libidn-devel libjpeg libjpeg-devel libltdl libmudflap41 libnscd libogg libpri libss7 libstdc++-devel libstdc++41 libstdc++41-devel libtiff libtiff-devel libtool libvolume_id libxcrypt libzio limal limal-bootloader limal-perl linux-kernel-headers m4 make man mdadm mkinitrd mktemp module-init-tools ncurses ncurses-devel net-tools netcfg openldap2 openldap2-client openldap2-devel openssl openssl-devel pam pam-modules patch pcre perl perl-Bootloader perl-gettext permissions popt postgresql-devel postgresql-libs procinfo procps psmisc pwdutils rcs readline reiserfs rpm sed  speex speex-devel sqlite sqlite-devel strace sysvinit tar tcpd tcpd-devel texinfo timezone udev unixODBC unixODBC-devel unzip util-linux vim zlib zlib-devel

%define 	distdir %(echo "$DISTDIR") 
%define 	asteriskver %(echo "asterisk${ASTERISKVER}") 
%define 	rpmversion %(echo "${RPMVERSION}_${RPMBRANCH}") 
%define 	rpmrelease %(echo "$RPMREVISION") 

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
%define 	packer %(finger -lp `echo "$USER"` | head -n 1 | cut -d: -f 3)

Name:           chan-sccp-b_%{asteriskver}
BuildRequires: 	bison
BuildRequires: 	m4
BuildRequires: 	%{asteriskver}
BuildRequires:  %{asteriskver}-devel

PreReq:		pwdutils coreutils sed grep

Summary:	Chan_SCCP-b
Version: 	%{rpmversion}
Release:  	%{rpmrelease}.%{disttag}
License: 	GPL
Group:          Productivity/Telephony/Servers
Packager: 	%packer% <chan-sccp-b-release@lists.sourceforge.net>
Vendor:         chan-sccp-b development team <chan-sccp-b-release@lists.sourceforge.net>
URL: 		http://chan-sccp-b.sourceforge.net/
Source: 	%{distdir}.tar.gz
BuildRoot:      %{_tmppath}/%{name}_%{version}-%{release}-build



%description
chan_sccp-b is the SCCP channel connector for your Asterisk PBX. It is the
continued development of the chan_skinny project. It feature improved support
for sccp dev ices and new feature like multiline, dynamic speeddials, shared 
lines etc.

Documentation is available on the chan-sccp-b home page
(http://chan-sccp-b.sourceforge.net/) chan-sccp-b Home Page
(http://chan-sccp-b.sourceforge.net/documentation.shtml) chan-sccp-b Documentation
(http://sourceforge.net/projects/chan-sccp-b) chan-sccp-b SourceForge Page


Authors:
--------
Anthony Shaw 		: 	anthonyshaw@users.sourceforge.net
Marcello Ceschia 	: 	marcelloceschia@users.sourceforge.net
Scotchy 		: 	scotchy@users.sourceforge.net
David Dederscheck 	: 	davidded@users.sourceforge.net
Andrew Lee 		: 	andrewylee@users.sourceforge.net
Diederik de Groot 	: 	ddegroot@users.sourceforge.net
Federico Santulli 	: 	fsantulli@users.sourceforge.net
Geoff Thornton 		: 	gthornton@users.sourceforge.net
macdiver 		: 	macdiver@users.sourceforge.net
Sebastian Vetter 	:	sebbbl@users.sourceforge.net


%prep
echo Building %{name}-%{version}-%{release}...
%setup -n %{distdir}
#%patch0 -p0 
#%patch1 -p 0


%build
CFLAGS="$RPM_OPT_FLAGS"
%configure --with-dynamic-speeddial
make  %{?_smp_mflags}


%install
%{__make} install DESTDIR=%{buildroot}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ChangeLog INSTALL README COPYING AUTHORS 
#doc/html
%{_libdir}/asterisk/modules/chan_sccp.so
#%{_sysconfdir}/asterisk/sccp.conf

%changelog -n chan-sccp-b
* Sun Apr 12 2010 - dkgroot@talon.nl
- Modified for asterisk 1.4.30


###
### eof
###

