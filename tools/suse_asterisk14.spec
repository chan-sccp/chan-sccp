#
# spec file for package chan-sccp-b (Version trunk)
#
#

# norootforbuild
# usedforbuild    aaa_base acl alsa alsa-devel attr audit-libs autoconf automake bash bind-libs bind-utils binutils bison blocxx bzip2 coreutils cpio cpp cpp41 cracklib curl curl-devel cvs cyrus-sasl dahdi-linux dagdi-linux-devel dahdi-linux-kmp-default dahdi-tools dahdi-tools-devel db diffutils e2fsprogs expat file filesystem fillup findutils flex gawk gcc gcc-c++ gcc41 gcc41-c++ gdbm gdbm-devel gettext gettext-devel glibc glibc-devel glibc-locale gpm grep groff gzip info insserv irqbalance kernel-default klogd krb5 less libacl libattr libcom_err libgcc41 libgsm libgsm-devel libidn libidn-devel libjpeg libjpeg-devel libltdl libmudflap41 libnscd libogg libpri libss7 libstdc++-devel libstdc++41 libstdc++41-devel libtiff libtiff-devel libtool libvolume_id libxcrypt libzio limal limal-bootloader limal-perl linux-kernel-headers m4 make man mdadm mkinitrd mktemp module-init-tools ncurses ncurses-devel net-tools netcfg openldap2 openldap2-client openldap2-devel openssl openssl-devel pam pam-modules patch pcre perl perl-Bootloader perl-gettext permissions popt postgresql-devel postgresql-libs procinfo procps psmisc pwdutils rcs readline reiserfs rpm sed  speex speex-devel sqlite sqlite-devel strace sysvinit tar tcpd tcpd-devel texinfo timezone udev unixODBC unixODBC-devel unzip util-linux vim zlib zlib-devel

%define         origname     chan-sccp-b

Name:           chan-sccp-b
BuildRequires: 	bison
BuildRequires: 	doxygen
BuildRequires: 	m4
BuildRequires: 	asterisk
BuildRequires:  asterisk-devel
BuildRequires:  graphviz

PreReq:		pwdutils coreutils sed grep

Summary:	Chan_SCCP-b
Version: 	3.0.0
Release: 	RC1
License: 	GPL
Group:          Productivity/Telephony/Servers
Packager: 	Diederik de Groot <ddegroot@sourceforge.net>
URL: 		http://chan-sccp-b.sourceforge.net/
Source: 	trunk.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build



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

%package asterisk
Summary:	The Asterisk Open Source PBX
Group:          Productivity/Telephony/Servers

%description	asterisk
chan-sccp-b is an channel extension for asterisk


%prep
%setup -n %{origname}_%{version}
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

