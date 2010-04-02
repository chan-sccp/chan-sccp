#
# rpm spec file for aterm
#

Summary: Chan_SCCP-b
Name: chan_sccp-b
Version: 0.4.2
Release: 4%{?susevers:.%{susevers}}
License: GPL
Group: User Interface/X
Packager: Diederik de Groot <ddegroot@sourceforge.net>
URL: http://chan-sccp-b.sourceforge.net/
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
chan_sccp-b is the SCCP channel connector for your Asterisk PBX. It is the
continued development of the chan_skinny project. It feature improved support
for sccp dev ices and new feature like multiline, dynamic speeddials, shared 
lines etc.


%changelog
2010-03-30  marcelloceschia

	* [r1402] src/chan_sccp.c: fixing mwi start/stop issue

2010-03-29  ddegroot

	* [r1401] src/Makefile.in, src/sccp_hint.c, src/sccp_protocol.h,
	  src/sccp_socket.c, src/sccp_utils.c, src/sccp_utils.h: Extended
	  AST_EXTENSION Mapping to include state 9
	  Added ARR2STR Mapping for SCCP_EXTENSIONSTATE
	  Extended ExtensionStatus to Asterisk Mapping in sccp_utils
	  Changed timing in sccp_socket back to 1000 mu_sec

2010-03-28  ddegroot

	* [r1400] ChangeLog, configure,
	  doc/03_Building_and_Installation_Guide.doc,
	  doc/04_Setup_Guide.doc, doc/Makefile.in, libtool: Replaced
	  SEP??????? with SEP[MacAddr] in documentation

	* [r1399] src/chan_sccp.c, src/sccp_cli.c: Jitterbuffer handling
	  for newer Asterisk versions

	* [r1398] ChangeLog, Makefile.in, aclocal.m4, configure,
	  configure.ac, libtool: Added Jitterbuffer Handling for newer
	  Asterisk Versions

	* [r1397] src/sccp_socket.c: Updated sccp_session_send2 to reduce
	  socket timing and close socket correctly on failure

	* [r1396] src/sccp_device.c: Updated sccp_dev_displayprompt to send
	  different messages according to the protocol level in use by the
	  phone

	* [r1395] src/sccp_actions.c: Implemented DialedPhoneBookAckMessage
	  Implemented ServiceURLStatDynamicMessage

	* [r1394] src/Makefile.in, src/chan_sccp.c, src/sccp_protocol.h:
	  Replaced unknown messages 0x0149 and 0x0159



%prep
%setup -q


%build
CFLAGS="%{optflags}" \
./configure \
  --prefix=%{_prefix}
make  %{?_smp_mflags}


%install
if [ ! %{buildroot} = "/" ]; then %{__rm} -rf %{buildroot}; fi
make DESTDIR=%{buildroot} install


%clean
if [ ! %{buildroot} = "/" ]; then %{__rm} -rf %{buildroot}; fi


%files
%defattr(-,root,root)
%doc ChangeLog INSTALL README
%{_prefix}/usr/lib/asterisk/modules


###
### eof
###
