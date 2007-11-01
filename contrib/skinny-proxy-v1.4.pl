#!/usr/bin/perl -w

# Additions to use it as an Cisco SKINNY Gateway by:
# Cliff Albert, (c) 2002
# Code cleanup, fixes and some additional features:
# tuxje, Juerd
#
# Original code by:
# http://slinky.scrye.com/~tkil/perl/port-forward
# by Tkil <tkil@scrye.com>, 1999-05-21
#
# released under the same terms as Perl itself.  please use and share.

use strict;
use IO::Socket;
use IO::Select;
use POSIX;
use Socket qw(INADDR_ANY PF_INET SOCK_DGRAM sockaddr_in inet_ntoa inet_aton);
use Sys::Hostname;

use vars qw(%messages %rmessages);

$| = 1;

my $debugMode = 1;
my %debug_info;
my $version = "0.2.5";

my ($remote_rtp, $local_rtp, $sel, $proxy_host, $server_host, $local_host);

use constant conn_proto  => "tcp";    # listen protocol
use constant io_buf_size => 1024;     # how much data should we snarf at once?
use constant listen_max  => 10;       # max clients

my $proxy_port  = 2000;
my @local_ports = 4666;
my $server_port = 2000;
my (%used_ports, %calls);

# now some hashes to map client connections to server connections.
my (%client_of_server, %server_of_client);

# and one more hash, from socket name to real socket:
my %socket_of;


sub debug {
    return unless $debugMode;
    if (ref $_[0] eq 'HASH') {
	for (values %{ $_[0] }) {
	    s/([\0\cA-\cZ])/"\e[7m" . chr(64 + ord $1) . "\e[0m"/eg;
	    s/ /\e[30;1m*\e[0m/g;
	}
	print map "  $_: $_[0]{$_}\n", grep !/^_/, keys %{$_[0]};
	return;
    }
    print STDERR map "$_\n", @_;
}

sub udp_forward {

    # $local_rtp = local_host:port
    # $remote_rtp = server_host:port
    my $server_addr;

    die "Bad listen address\n" if $local_rtp !~ /([^:]+):(\d+)/;
    my $server_saddr =
      $1 eq "*" ? sockaddr_in($2, INADDR_ANY) : sockaddr_in($2, inet_aton($1));

    die "Bad forward address\n" if $remote_rtp !~ /([^:]+):(\d+)/;
    my $forward_saddr = sockaddr_in($2, inet_aton($1));
    my $forward_addr  = "$1:$2";

    socket(SERVSOCK, PF_INET, SOCK_DGRAM, getprotobyname('udp'))
      or die "socket: $!";
    bind(SERVSOCK, $server_saddr) || die "Couldn't bind ($!)";

    # scan all the servers
    print "Forwarding started\n";

    while (1) {
        my $rin = '';
        vec($rin, fileno(SERVSOCK), 1) = 1;

        select(my $rout = $rin, undef, undef, undef) or next;
	
	my ($send_addr, $send_saddr);

	my $paddr = recv(SERVSOCK, my $buf, 2048, 0) or die "Couldn't recv ($!)";

	my ($fromport, $addr) = sockaddr_in($paddr);
	my $fromaddr = inet_ntoa($addr);

	if ($forward_addr eq "$fromaddr:$fromport") {
	    $send_addr  = $server_addr;
	    $send_saddr = $server_saddr;
	} else {
	    $server_saddr = $paddr;
	    $server_addr  = "$fromaddr:$fromport";
	    $send_addr    = $forward_addr;
	    $send_saddr   = $forward_saddr;
	}

	defined send(SERVSOCK, $buf, 0, $send_saddr)
	  or die "Couldn't send ($!)";
    }

}

sub dectodot {
    my $addr = shift;
    my $fourth;
    my $third;
    my $second;
    my $first;

    $first  = $addr % 256;
    $addr   = ($addr - $first) / 256;
    $second = ($addr % 256);
    $addr   = ($addr - $second) / 256;
    $third  = $addr % 256;
    $addr   = ($addr - $third) / 256;
    $fourth = $addr % 256;
    return "$first.$second.$third.$fourth";
}

sub add_client_sock {
    my $client = shift;

    if ($debugMode) {
        $debug_info{$client} = $client->peerhost . ":" . $client->peerport;
        debug "received client: $debug_info{$client}";
    }

    # open the proxied connection...
    my $server = IO::Socket::INET->new(
        PeerAddr => $server_host,
        PeerPort => $server_port,
        Proto    => conn_proto
    ) or die "Couldn't connect to $server_host:$server_port ($!)";

    if ($debugMode) {
        $debug_info{$server} = $server->peerhost . ":" . $server->peerport;
        debug "opened server: $debug_info{$server}";
    }

    # now populate the hashes.
    $socket_of{$client}        = $client;
    $socket_of{$server}        = $server;
    $client_of_server{$server} = $client;
    $server_of_client{$client} = $server;

    # and add both socket to the IO::Select object
    $sel->add($client);
    $sel->add($server);
}

sub remove_socket {
    my $sock = shift;

    if ($debugMode) {
        debug "removing socket: $debug_info{$sock}";
        delete $debug_info{$sock};
    }

    # determine the "other half" of this socket, removing it from the
    # hash as we go.
    my $dest_sock;
    if (exists $client_of_server{$sock}) {
        $dest_sock = delete $client_of_server{$sock};
        delete $server_of_client{$dest_sock};
    } elsif (exists $server_of_client{$sock}) {
        $dest_sock = delete $server_of_client{$sock};
        delete $client_of_server{$dest_sock};
    } else {
        warn "couldn't find sock in 'remove_socket'";
        return;
    }

    if ($debugMode) {
        debug "removing socket: $debug_info{$dest_sock}";
        delete $debug_info{$dest_sock};
    }

    # remove them from the rest of the hashes, as well.
    delete $socket_of{$sock};
    delete $socket_of{$dest_sock};

    # and from the IO::Select object
    $sel->remove($sock);
    $sel->remove($dest_sock);

    # and close them.
    $sock->close;
    $dest_sock->close;
}

sub process_data {
    my $sock = shift;

    # determine the "other half" of this socket.
    my $dest_sock;
    my $source;
    if (exists $client_of_server{$sock}) {
        $dest_sock = $client_of_server{$sock};
        $source    = "CallManager";
    } elsif (exists $server_of_client{$sock}) {
        $dest_sock = $server_of_client{$sock};
        $source    = "Phone";
    } else {
        warn "couldn't find sock in 'process_data'";
        return;
    }

    # read the actual data.  punt if we error.
    my $buffer;
    my $n_read = sysread($sock, $buffer, io_buf_size);
    unless ($n_read) {
        remove_socket($sock);
        return;
    }

    # Packets zijn kuddedieren...
    my @packets;
    my $copy = $buffer;
    while (length $copy) {
	push @packets, { };
	my ($length) = unpack 'la*', $copy;
	my $data = substr $copy, 0, $length + 8, '';
	@{ $packets[-1] }{qw/length reserved message_id content/} =
	  unpack "llla*", $data;
    }

    # Don't just stand there; do something! :)
    for my $packet (@packets) {
	
	my $message_id = $packet->{message_id};
	if (not length $message_id) {
	    debug "Strange packet";
	    next;
	}
	my $content = $packet->{content};
	
	my $hexid = sprintf "0x%x", $message_id;
	
	my $attr = exists $messages{$message_id}
	  ? [ @{ $messages{$message_id} } ]
	  : [ "UndocumentedMessage $hexid" ];
	
	my $message = shift @$attr;
	
	debug "$source: $message";

	my %data;
	if (@$attr) {
	    my $template = '';
	    my @names;
	    my @ip;
	    for (@$attr) {
		die "Malformed attr ($_)" if not /([^_]+)_(.*)/;
		if ($1 eq '*') { # IP
		    $template .= 'l';
		    push @ip, $2;
		} else {
		    $template .= $1;
		}
		push @names, $2;
	    }
	    @data{@names} = unpack $template, $content;
	    $_ = dectodot $_ for @data{@ip};
	}

	if ($message eq 'CloseReceiveChannel') {
            my $pid = $calls{ $data{PassThruPartyID} };

            print
	      "Terminating UDP forward for call $data{PassThruPartyID}, pid " .
	      "$pid with port $used_ports{$pid}"
		unless $debugMode;

        
	    debug "Killing UDP forwarder at $pid for call " .
	      $data{PassThruPartyID};
            kill(15, $pid);
            print "." unless $debugMode;

            debug "Removing $used_ports{$pid} from $pid and returning to pool";
            push @local_ports, $used_ports{$pid};
            delete $used_ports{$pid};
            print "." unless $debugMode;

            debug "Removing $pid from calls pool";
            delete $calls{ $data{PassThruPartyID} };
            print ". done\n" unless $debugMode;

        } elsif ($message eq 'StationRegisterReject') {
            my $payload = pack("lla33", 0, 0x99, $data{DisplayMessage});
            $buffer = pack("la*", length($payload), $payload);
        } elsif ($message eq 'DefineTimedate') {;
            my @data = unpack("llllllllla*", $content);
	    $data[1] -= 1;
	    if ($data[0] > 1900) {
		$data[0] -= 1900;
	    } elsif ($data[0] < 70) {
		$data[0] += 100
	    }
	    @data{qw/Date _content/} = (
		strftime(
		    "%d/%m/%y %H:%M:%S",
		    @data[6, 5, 4, 3, 1, 0, 2], 0, 0
		),
		$data[-1]
	    );
        } elsif ($message eq 'CallInfoMessage') {
            print "Incoming call from $data{CallingPartyName} (" .
	      "$data{CallingParty}) for $data{CalledPartyName} (" .
	      "$data{CalledParty})\n"
              unless $debugMode;
        } elsif ($message eq 'StartMediaTransmission') {
            print "Call established with "
              . "$data{RemoteIPAddr}:$data{RemotePortNumber}\n";
        } elsif ($message eq 'OpenReceiveChannelAck') {
            my $local_port = pop @local_ports;
            debug "Assigned port $local_port";

            $buffer = pack(
		"lllllll",
                $packet->{length},
		$packet->{reserved},
		$message_id,
		$data{ORCStatus},
                unpack("V", inet_aton($local_host)),
                $local_port,
		$data{PassThruPartyID}
	    );
	    
            $remote_rtp = "$data{ipAddress}:$data{PortNumber}";
            $local_rtp = "$local_host:$local_port";

            if (my $pid = fork) {
                debug "Forked a child (pid $pid)";

                debug
                  "Removed $local_port from the pool and assigning it to $pid";
                $used_ports{$pid} = $local_port;

                debug "Registered call $data{PassThruPartyID} to $pid";
                $calls{ $data{PassThruPartyID} } = $pid;

                print "Created UDP forward for call $data{PassThruPartyID}, "
		  . "pid $pid with port $local_port\n"
                  unless $debugMode;
            } elsif (defined $pid) {
                udp_forward();
            } else {
		die "Could not fork ($!)";
	    }
        } elsif ($message eq 'UndocumentedMessage') {
	    $data{Content} = $content;
	}

	debug \%data if keys %data;
    }

    # now forward it along
    syswrite($dest_sock, $buffer, $n_read)
      or remove_socket($sock);
}

print qq{WARNING! This program is considered ALPHA quality and is probably
VERY buggy. Use at your own risk.

};

if (@ARGV != 3) {
    print STDERR "Usage: $0 listenip forwardip callmanager\n";
    exit 1;
}

($proxy_host, $local_host, $server_host) = @ARGV;
$proxy_port  = $1 if $proxy_host  =~ s/:(\d+)$//;
$server_port = $1 if $server_host =~ s/:(\d+)$//;

if ($local_host =~ s/:([\d-]+)//) {
    if ($1 =~ /(\d+)-(\d+)/) {
        print "Forwarding from $local_host:$1-$2\n";
        @local_ports = $1 .. $2;
    } else {
        @local_ports = $1;
        print "Forwarding from $local_host:$1\n";
    }
}

print "Setting up listening socket on $proxy_host:$proxy_port\n";

# setup listening port
my $listen_sock = IO::Socket::INET->new(
    LocalAddr => $proxy_host,
    LocalPort => $proxy_port,
    Proto     => conn_proto,
    Type      => SOCK_STREAM,
    Listen    => listen_max,
    Reuse     => 1
) or die "Couldn't bind listening socket $proxy_host:$proxy_port ($!)";

# create the IO::Select that will control our universe.  add the
# listening socket.
$sel = IO::Select->new($listen_sock);

while (my @handles = IO::Select::select($sel, undef, $sel)) {

    # remove any sockets that are in error
    my %removed;
    for (@{ $handles[2] }) {
        remove_socket($_);
        $removed{$_} = 1;
    }

    # get input from each active socket
    for my $sock (grep { not exists $removed{$_} } @{ $handles[0] }) {
        # any new sockets?
        if ($sock == $listen_sock) {
            my $new_sock = $listen_sock->accept or die "Couldn't accept ($!)";
            add_client_sock($new_sock);
        } else {
            # just move along.
            process_data($sock);
        }
    }
}

BEGIN {
%messages = (
0x200 => [ Oops                     => qw() ],
0x11b => [ RegisterTokenReject      => qw() ], ##
0x11a => [ RegisterTokenAck         => qw() ],
0x119 => [ BackSpaceReqMessage      => qw() ], ##
0x118 => [ UnregisterAckMessage     => qw() ], ##
0x117 => [ DeactivateCallPlaneMessage => qw() ],
0x116 => [ ActivateCallPlaneMessage => qw() ], ## 
0x115 => [ ClearNotifyMessage       => qw() ],
0x114 => [ DisplayNotifyMessage     => qw() ], ##
0x113 => [ ClearPromptStatusMessage => qw() ], ##
0x112 => [ DisplayPromptStatusMessage => qw() ], ##
0x111 => [ CallStateMessage         => qw() ], ## 
0x110 => [ SelectSoftKeysMessage    => qw() ],
0x109 => [ SoftKeySetResMessage     => qw() ],
0x108 => [ SoftKeyTemplateResMessage => qw() ],
0x107 => [ ConnectionStatisticsReq  => qw() ], ##
0x106 => [ CloseReceiveChannel      => qw( l_ConferenceID l_PassThruPartyID ) ],
0x105 => [ OpenReceiveChannel       => qw( l_ConferenceID l_PassThruPartyID
                                           l_MilliSecondPacketSize
					   l_PayLoadCapability l_EchoCancelType
					   l_G723BitRate ) ],
0x104 => [ StopMulticastMediaTransmission => qw( l_ConferenceID l_PassThruPartyID ) ],
0x103 => [ StopMulticastMediaReception => qw( l_ConferenceID l_PassThruPartyID ) ],
0x102 => [ StartMulticastMediaTransmission => qw() ], ##
0x101 => [ StartWulticastMediaRecepion => qw() ], ##
0x100 => [ KeepAliveAckMessage      => qw() ],
0x9f  => [ ResetMessage             => qw( l_DeviceResetType ) ],
0x9e  => [ ServerResMessage         => qw() ],
0x9d  => [ StationRegisterReject    => qw( a33_DisplayMessage ) ],
0x9c  => [ EnunciatorCommandMessage => qw() ],
0x9b  => [ CapabilitiesReqMessage   => qw() ],
0x9a  => [ ClearDisplay             => qw() ],
0x99  => [ DisplayTextMessage       => qw( a33_TextMessage ) ],
0x98  => [ VersionMessage           => qw( Z16_Version ) ],
0x97  => [ ButtonTemplateMessage    => qw() ],
0x96  => [ StopSessionTransmission  => qw( *_RemoteIPAddr l_SessionType ) ],
0x95  => [ StartSessionTransmission => qw( *_RemoteIPAddr l_SessionType ) ],
0x94  => [ DefineTimedate           => qw() ], #
0x93  => [ ConfigStatMessage        => qw( A16_DeviceName l_StationUserID
                                           l_StationUserInstance
					   A16_UserName A16_ServerName
					   l_NumberLines l_NumberSpeedDials ) ],
0x92  => [ LineStatMessage          => qw( l_LineNumber A24_LineDirNumber
                                           A40_LineDisplayName ) ],
0x91  => [ SpeedDialStatMessage     => qw( l_SpeedDialNumber
                                           A24_SpeedDialDirNumber
					   A40_SpeedDialDisplayName ) ],
0x90  => [ ForwardStatMessage       => qw( l_ActiveForward l_LineNumber
                                           l_ForwardAllActive
					   A24_ForwardAllNumber
					   l_ForwardBusyActive
					   A24_ForwardBusyNumber
					   l_ForwardNoAnswerAnswerActive
					   A24_ForwardNoAnswerNumber ) ],
0x8f  => [ CallInfoMessage          => qw( A40_CallingPartyName A24_CallingParty
                                           A40_CalledPartyName A24_CalledParty
                                           l_LineInstance l_CallIdentifier
					   l_CallType
					   A40_originalCalledPartyName
					   A24_originalCalledParty ) ],
0x8d  => [ StopMediaReception       => qw() ],
0x8c  => [ StartMediaReception      => qw() ],
0x8b  => [ StopMediaTransmission    => qw( l_ConferenceID l_PassThruPartyID ) 
                                       ],
0x8a  => [ StartMediaTransmission   => qw( l_ConferenceID l_PassThruPartyID
                                           *_RemoteIPAddr l_RemotePortNumber
                                           l_MilliSecondPacketSize
					   l_PayLoadCapability l_PrecedenceValue
		                           l_SilenceSuppresion
					   S_MaxFramesPerPacket l_G723BitRate )
                                       ],
0x89  => [ SetMicroModeMessage      => qw() ],
0x88  => [ SetSpeakerModeMessage    => qw() ],
0x87  => [ SetHkFDetectMessage      => qw() ],
0x86  => [ SetLampMessage           => qw( l_Stimulus l_StimulusInstance
                                           l_LampMode ) ],
0x85  => [ SetRingerMessage         => qw( l_Ringer ) ],
0x83  => [ StopToneMessage          => qw() ],
0x82  => [ StartToneMessage         => qw( l_DeviceTone ) ],
0x81  => [ RegisterAckMessage       => qw( l_KeepAliveInterval A6_DateTemplate
                                           l_SecondaryKeepAliveInterval ) ],
0x29  => [ RegisterTokenReq         => qw( A16_DeviceName l_StationUserID
                                           l_StationInstance l_IPAddress
					   l_DeviceType ) ],
0x28  => [ SoftKeyTemplateReqMessage => qw() ],
0x27  => [ UnregisterMessage        => qw() ],
0x26  => [ SoftKeyEventMessage      => qw( l_SoftKeyEvent l_LineInstance
                                           l_CallIdentifier ) ],
0x25  => [ SoftKeySetReqMessage     => qw() ],
0x24  => [ OffHookWithCgpnMessage   => qw( A24_CalledParty ) ],
0x23  => [ ConnectionStatisticsRes  => qw( A24_DirectoryNumber l_CallIdentifier
                                           l_StatsProcessingType l_PacketsSent
					   l_OctetsSent l_PacketsRecv
					   l_OctetsRecv l_PacketsLost l_Jitter
					   l_Latency ) ],
0x22  => [ OpenReceiveChannelAck    => qw( l_ORCStatus *_ipAddress l_PortNumber
                                           l_PassThruPartyID ) ],
0x21  => [ MulticastMediaReceptionAck => qw( l_ReceptionStatus l_PassThruPartyID ) ],
0x20  => [ AlarmMessage             => qw( l_AlarmSeverity A80_DisplayMessage
                                           l_AlarmParam1 *_AlarmParam2 ) ],
0x12  => [ ServerReqMessage         => qw() ],
0x11  => [ MediaPortListMessage     => qw() ],
0x10  => [ CapabilitiesResMessage   => qw() ],
0x0f  => [ VersionReqMessage        => qw() ],
0x0e  => [ ButtonTemplateReqMessage => qw() ],
0x0d  => [ TimeDateReqMessage       => qw() ],
0x0c  => [ ConfigStateReqMessage    => qw() ],
0x0b  => [ LineStatReqMessage       => qw( l_LineNumber ) ],
0x0a  => [ SpeedDialStatReqMessage  => qw( l_SpeedDialNumber ) ],
0x09  => [ ForwardStatReqMessage    => qw( l_LineNumber ) ],
0x08  => [ HookFlashMessage         => qw() ],
0x07  => [ OnHook                   => qw() ],
0x06  => [ OffHook                  => qw() ], 
0x05  => [ StimulusMessage          => qw( l_Stimulus l_StimulusInstance) ],
0x04  => [ EnblocCallMessage        => qw( l_CalledParty ) ],
0x03  => [ KeypadButtonMessage      => qw( l_Button ) ],
0x02  => [ IpPortMessage            => qw( l_PortNumber ) ],
0x01  => [ RegisterMessage          => qw( A16_PhoneName l_StationUserId
                                           l_StationInstance *_IPAddress
                                           l_DeviceType l_MaxStreams) ],
0     => [ KeepAliveMessage         => qw() ],
);
}
