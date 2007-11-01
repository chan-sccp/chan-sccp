#!/usr/bin/perl -w

# $Id: skinny-proxy-v1.2.pl,v 1.1 2004/07/28 02:04:37 czmok Exp $

# Additions to use it as an Cisco SKINNY Gateway by:
# Cliff Albert, (c) 2002
# Code cleanup, fixes and some additional features:
# tuxje
#
# Original code by:
# http://slinky.scrye.com/~tkil/perl/port-forward
# by Tkil <tkil@scrye.com>, 1999-05-21
#
# released under the same terms as Perl itself.  please use and share.

use strict;
use Socket qw(INADDR_ANY PF_INET SOCK_DGRAM sockaddr_in inet_ntoa inet_aton);
use Sys::Hostname;

use IO::Socket;
use IO::Select;

my $debugMode = 1;
my %debug_info;
my $version = "0.2.1";

my ($remote_rtp, $local_rtp, $sel, $proxy_host, $server_host, $local_host);

use constant conn_proto => "tcp";		# listen protocol
use constant io_buf_size => 1024;		# how much data should we snarf at once?
use constant listen_max => 10;			# max clients

my $proxy_port = 2000;
my @local_ports = 4666;
my $server_port = 2000;
my (%used_ports, %calls);

# now some hashes to map client connections to server connections.
my (%client_of_server, %server_of_client);

# and one more hash, from socket name to real socket:
my %socket_of;

# ============================================================
# subs
sub debug
{
	print STDERR join("\n", @_) . "\n" if $debugMode;
}

sub udp_forward
{
	# $local_rtp = local_host:port
	# $remote_rtp = server_host:port
	my $server_addr;

	die "Bad listen address\n" if $local_rtp !~ /([^:]+):(\d+)/;
	my $server_saddr = $1 eq "*" ? sockaddr_in ($2, INADDR_ANY) : sockaddr_in ($2, inet_aton($1));

	die "Bad forward address\n" if $remote_rtp !~ /([^:]+):(\d+)/;
	my $forward_saddr = sockaddr_in ($2, inet_aton($1));
	my $forward_addr = "$1:$2";

	socket (SERVSOCK, PF_INET, SOCK_DGRAM, getprotobyname('udp')) || die "socket: $!";
	bind (SERVSOCK, $server_saddr) || die "bind: $!";

	# scan all the servers
	print "Forwarding started\n";

	while ( 1 )
	{
		my $rin = '';
		vec ($rin, fileno(SERVSOCK), 1) = 1;

		if ( select(my $rout = $rin, undef, undef, undef) ) {
			my ($send_addr, $send_saddr);

			(my $paddr = recv (SERVSOCK, my $buf, 2048, 0)) || die "recv: $!";
	
			my ($fromport, $addr) = sockaddr_in ($paddr);
			my $fromaddr = inet_ntoa($addr);

			if ( "$fromaddr:$fromport" eq $forward_addr ) {
				$send_addr = $server_addr;
				$send_saddr = $server_saddr;
			} else {
				$server_saddr = $paddr;
				$server_addr = "$fromaddr:$fromport";
				$send_addr = $forward_addr;
				$send_saddr = $forward_saddr;
			}

			die "send $send_addr: @!" unless defined send (SERVSOCK, $buf, 0, $send_saddr);
    		}
	}

}

sub dectodot
{	
   my $addr = shift;
   my $fourth;
   my $third;
   my $second;
   my $first;

     $first= $addr % 256;
     $addr = ($addr - $first) / 256;
     $second = ($addr % 256);
     $addr = ($addr - $second) / 256;
     $third = $addr % 256;
     $addr = ($addr - $third) / 256;
     $fourth = $addr % 256;
     return "$first.$second.$third.$fourth";
}

sub add_client_sock 
{
   my $client = shift;

   if ($debugMode) {
     $debug_info{$client} = $client->peerhost . ":" . $client->peerport;
     debug "received client: $debug_info{$client}";
   }

   # open the proxied connection...
   my $server = IO::Socket::INET->new(PeerAddr	=> $server_host,
				      PeerPort	=> $server_port,
				      Proto	=> conn_proto)
     or die "opening server socket: $!";

   if ($debugMode) {
     $debug_info{$server} = $server->peerhost . ":" . $server->peerport;
     debug "opened server: $debug_info{$server}";
   }

   # now populate the hashes.
   $socket_of{$client} = $client;
   $socket_of{$server} = $server;
   $client_of_server{$server} = $client;
   $server_of_client{$client} = $server;

   # and add both socket to the IO::Select object
   $sel->add($client);
   $sel->add($server);
}

# ============================================================

sub remove_socket
{
   my $sock = shift;

   if ($debugMode) {
     debug "removing socket: $debug_info{$sock}";
     delete $debug_info{$sock};
   }

   # determine the "other half" of this socket, removing it from the
   # hash as we go.
   my $dest_sock;
   if (exists $client_of_server{$sock}) 
   {
      $dest_sock = delete $client_of_server{$sock};
      delete $server_of_client{$dest_sock};
   }
   elsif (exists $server_of_client{$sock})
   {
      $dest_sock = delete $server_of_client{$sock};
      delete $client_of_server{$dest_sock};
   }
   else 
   {
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

# ============================================================

sub process_data
{
   my $sock = shift;

   # determine the "other half" of this socket.
   my $dest_sock;
   my $source;
   if (exists $client_of_server{$sock}) 
   {
      $dest_sock = $client_of_server{$sock};
      $source = "CallManager";
   }
   elsif (exists $server_of_client{$sock})
   {
      $dest_sock = $server_of_client{$sock};
      $source = "Phone";
   }
   else 
   {
      warn "couldn't find sock in 'process_data'";
      return;
   }

   # read the actual data.  punt if we error.
   my $buf;
   my $n_read = sysread($sock, $buf, io_buf_size);
   unless ($n_read) {
      remove_socket($sock);
      return;
   }

   ### if you have any modifications you want to make to the data,
   ### here's where to do it.  the only added complication is this: if
   ### you want to do stuff line-by-line, you have to split stuff up
   ### for yourself (and, incidentally, you have to keep fragments of
   ### lines across 'process_data' invocations).  using <$sock> is
   ### *BAD* when you're using select.

   my ($data_length, $reserved, $message_id, $content, $junk) = 0;
   my $not_done = 1;
   my $large_packet = 0;
   my $orig_content = $buf;

   while ($not_done) {
	debug "--------------------------------------------------------";
	debug "Source: $source";
	debug "Length: " . length($orig_content);
	($data_length, $reserved, $message_id, $content) = unpack("llla*",$orig_content);
	if ((length($content) > $data_length)) {
		$large_packet = 1;
	} else {
		$large_packet = 0;
	} 
	debug "Data-Length: $data_length";
	debug sprintf("Message-Type: %x",$message_id);

   if ($message_id == 0x200) {
	 debug "Oops";
   } elsif ($message_id == 0x106) {
	 debug "CloseReceiveChannel";
	 my ($ConferenceID, $PassThruPartyID);
	 ($ConferenceID, $PassThruPartyID,$content) = unpack ("lla*",$content);
	 my $pid = $calls{$PassThruPartyID};

	 print "Terminating UDP forward for call $PassThruPartyID, pid $pid with port $used_ports{$pid}" unless $debugMode;

	 debug "Killing UDP forwarder at $pid for call $PassThruPartyID";
	 kill(15, $pid);
	 print "." unless $debugMode;

	 debug "Removing $used_ports{$pid} from $pid and returning to pool";
	 push(@local_ports, $used_ports{$pid});
	 delete $used_ports{$pid};
	 print "." unless $debugMode;

	 debug "Removing $pid from calls pool";
	 delete $calls{$PassThruPartyID};
	 print ". done\n" unless $debugMode;

   } elsif ($message_id == 0x105) {
	 debug "OpenReceiveChannel";
	 my ($ConferenceID, $PassThruPartyID, $MilliSecondPacketSize, $PayLoadCapability, $EchoCancelType, $G723BitRate);
	 ($ConferenceID, $PassThruPartyID, $MilliSecondPacketSize, $PayLoadCapability, $EchoCancelType, $G723BitRate,$content) = unpack("lllllla*",$content);
   } elsif ($message_id == 0x100) {
	 debug "KeepAliveAckMessage";
   } elsif ($message_id == 0x9d) {
	 debug "StationRegisterReject";
	 my $DisplayMessage = unpack("Z33", $content);
	 debug "DisplayMessage: $DisplayMessage";
	 my $payload = pack("llA33", 0, 0x99, "$DisplayMessage");
	 $buf = pack("la*", length($payload), $payload);
   } elsif ($message_id == 0x9b) {
	 debug "CapabilitiesReqMessage";
   } elsif ($message_id == 0x99) {
	 debug "DisplayTextMessage";
	 my $TextMessage = unpack ("A33",$content);
   } elsif ($message_id == 0x98) {
	 debug "VersionMessage";
	 my $Version = unpack ("Z16",$content);
	 debug "Version: $Version"
   } elsif ($message_id == 0x97) {
	 debug "ButtonTemplateMessage";
   } elsif ($message_id == 0x96) {
	 debug "StopSessionTransmission";
   } elsif ($message_id == 0x95) {
	 debug "StartSessionTransmission";
   } elsif ($message_id == 0x94) {
	 debug "DefineTimedate";
	 my ($dateYear, $dateMonth, $dayofWeek, $dateDay, $dateHour, $dateMinute, $dateSeconds, $dateMSeconds, $dateTimestamp);
	 ($dateYear,$dateMonth,$dayofWeek,$dateDay,$dateHour,$dateMinute,$dateSeconds,$dateMSeconds,$dateTimestamp,$content) = unpack ("llllllllla*",$content);
	 debug "$dateDay/$dateMonth/$dateYear";
   } elsif ($message_id == 0x93) {
	 debug "ConfigStatMessage";
   } elsif ($message_id == 0x92) {
	 debug "LineStatMessage";
   } elsif ($message_id == 0x91) {
	 debug "SpeedDialStatMessage";
   } elsif ($message_id == 0x90) {
	 debug "ForwardStatMessage";
   } elsif ($message_id == 0x8f) {
	 debug "CallInfoMessage";
	 my ($CallingPartyName, $CallingParty, $CalledPartyName, $CalledParty, $LineInstance, $CallIdentifier, $CallType, $originalCalledPartyName, $originalCalledParty);
	 ($CallingPartyName,$CallingParty,$CalledPartyName,$CalledParty,$LineInstance,$CallIdentifier,$CallType,$originalCalledPartyName,$originalCalledParty,$content) = unpack ("A40A24A40A24lllA40A24a*",$content);
        debug "CallingPartyName: $CallingPartyName";
        debug "CallingParty: $CallingParty";
        debug "CalledPartyName: $CalledPartyName";
        debug "CalledParty: $CalledParty";
        debug "LineInstance: $LineInstance";
        debug "CallIdentifier: $CallIdentifier";
        debug "CallType: $CallType";
        debug "originalCalledPartyName: $originalCalledPartyName";
        debug "originalCalledParty: $originalCalledParty";
	print "Incoming call from $CallingPartyName ($CallingParty) for $CalledPartyName ($CalledParty)\n" unless $debugMode;
   } elsif ($message_id == 0x8b) {
	 debug "StopMediaTransmission";
	 my ($ConferenceID, $PassThruPartyID);
	 ($ConferenceID,$PassThruPartyID,$content) = unpack ("lla*",$content);
   } elsif ($message_id == 0x8a) {
	 debug "StartMediaTransmission";
   	 my ($ConferenceID, $PassThruPartyID, $RemoteIPAddr, $RemotePortNumber, $MilliSecondPacketSize, $PayLoadCapability, $PrecedenceValue, $SilenceSuppresion, $MaxFramesPerPacket, $G723BitRate);
	 ($ConferenceID,$PassThruPartyID,$RemoteIPAddr,$RemotePortNumber,$MilliSecondPacketSize,$PayLoadCapability,$PrecedenceValue,$SilenceSuppresion,$MaxFramesPerPacket,$G723BitRate,$content) = unpack("llllllllSla*",$content);
	 print "Call established with " . dectodot($RemoteIPAddr) . ":$RemotePortNumber\n";

   } elsif ($message_id == 0x86) {
	 debug "SetLampMessage";
	 my ($Stimulus, $StimulusInstance, $LampMode);
	 ($Stimulus, $StimulusInstance, $LampMode,$content) = unpack ("llla*",$content);
   } elsif ($message_id == 0x85) {
	 debug "SetRingerMessage";
	 my $Ringer;
	 ($Ringer,$content) = unpack ("la*",$content);
   } elsif ($message_id == 0x83) {
	 debug "StopToneMessage";
   } elsif ($message_id == 0x82) {
	 debug "StartToneMessage";
	 my $DeviceTone;
	 ($DeviceTone,$content) = unpack ("la*",$content);
	 debug "Device-Tone: $DeviceTone";
   } elsif ($message_id == 0x81) {
	 debug "RegisterAckMessage";
	 my ($KeepAliveInterval, $DateTemplate, $SecondaryKeepAliveInterval);
	 ($KeepAliveInterval,$DateTemplate,$SecondaryKeepAliveInterval,$content) = unpack ("lA6la*",$content);
   } elsif ($message_id == 0x25) {
	 debug "SoftKeySetReqMessage";
   } elsif ($message_id == 0x22) {
	 debug "OpenReceiveChannelAck";
	 my ($ORCStatus, $ipAddress, $PortNumber, $PassThruPartyID);
	 ($ORCStatus, $ipAddress, $PortNumber, $PassThruPartyID,$content) = unpack ("lllla*", $content);
	 debug "ORCStatus: $ORCStatus";
	 debug "IPAddress: " . dectodot($ipAddress) . ":$PortNumber";
	 my $local_port = pop(@local_ports);
	 debug "Assigned port $local_port";
	 $buf = pack("lllllll", $data_length, $reserved, $message_id, $ORCStatus, unpack("V", inet_aton($local_host)), $local_port, $PassThruPartyID);
	 $remote_rtp = sprintf("%s:%i",dectodot($ipAddress),$PortNumber);
	 $local_rtp = "$local_host:$local_port";

	 if (my $pid = fork()) {
	 	debug "Forked a child (pid $pid)";

		debug "Removed $local_port from the pool and assigning it to $pid";
		$used_ports{$pid} = $local_port;

		debug "Registered call $PassThruPartyID to $pid";
		$calls{$PassThruPartyID} = $pid;

	 	print "Created UDP forward for call $PassThruPartyID, pid $pid with port $local_port\n" unless $debugMode;
	 } else {
		udp_forward();
	 }

   } elsif ($message_id == 0x20) { 
	 debug "AlarmMessage";
         my ($AlarmSeverity, $DisplayMessage, $AlarmParam1, $AlarmParam2);
	 ($AlarmSeverity,$DisplayMessage,$AlarmParam1,$AlarmParam2,$content) = unpack ("lA80lla*",$content);
	 debug "Alarm-Severity: $AlarmSeverity";
	 debug "Display-Message: $DisplayMessage";
	 debug "Alarm-Param1: $AlarmParam1";
	 debug "Alarm-Param2: " . dectodot($AlarmParam2);
   } elsif ($message_id == 0x10) {
	 debug "CapabilitiesResMessage";
   } elsif ($message_id == 0x0f) {
	 debug "VersionReqMessage";
   } elsif ($message_id == 0x0e) {
	 debug "ButtonTemplateReqMessage";
   } elsif ($message_id == 0x0d) {
	 debug "TimeDateReqMessage";
   } elsif ($message_id == 0x0c) {
	 debug "ConfigStateReqMessage";
   } elsif ($message_id == 0x09) {
	 debug "ForwardStatReqMessage";
   } elsif ($message_id == 0x08) {
	 debug "HookFlashMessage";
   } elsif ($message_id == 0x07) {
	 debug "OnHook";
   } elsif ($message_id == 0x06) {
	 debug "OffHook"; 
   } elsif ($message_id == 0x05) {
	 debug "StimulusMessage";
   } elsif ($message_id == 0x04) {
	 debug "EnblocCallMessage";
   } elsif ($message_id == 0x03) {
	 debug "KeypadButtonMessage";
   } elsif ($message_id == 0x02) {
	 debug "IpPortMessage";
	 my $PortNumber = unpack ("S",$content);
	 debug "Port-Number: $PortNumber";
   } elsif ($message_id == 0x01) {
	 debug "RegisterMessage";
	 my ($PhoneName, $StationUserId, $StationInstance, $IPAddress, $DeviceType, $MaxStreams);
	 ($PhoneName,$StationUserId,$StationInstance,$IPAddress,$DeviceType,$MaxStreams,$content) = unpack ("A16llllla*",$content);
	 debug "Phone-Name: $PhoneName";
	 debug "Station-User-Id: $StationUserId";
	 debug "IP-Address: " . dectodot($IPAddress);
   } elsif ($message_id == 0) {
	 debug "KeepAliveMessage";
   } else { 
   	 debug "UndocumentedMessage";
	 debug unpack("a*", $content);
   }

   if ($large_packet == 0) {
     $not_done = 0;
   } else {
     $orig_content = $content;
   }
   } 

   # now forward it along
   syswrite($dest_sock, $buf, $n_read)
     or remove_socket($sock);
}

# ============================================================
# main

print qq{WARNING! This program is considered ALPHA quality and is probably
VERY buggy. Use at your own risk.

};

if(scalar @ARGV != 3) {
	print STDERR "Usage: $0 listenip forwardip callmanager\n";
	exit 1;
}

($proxy_host, $local_host, $server_host) = @ARGV;
$proxy_port  = $1 if $proxy_host =~ s/:(\d+)$//;
$server_port = $1 if $server_host =~ s/:(\d+)$//;

if($local_host =~ s/:([\d-]+)//) {
        if($1 =~ /(\d+)-(\d+)/) {
		print "Forwarding from $local_host:$1-$2\n";
                @local_ports = $1 .. $2;
        } else { @local_ports = $1; print "Forwarding from $local_host:$1\n"; };
}


print "Setting up listening socket on $proxy_host:$proxy_port\n";

# setup listening port
my $listen_sock = IO::Socket::INET->new(LocalAddr  => $proxy_host,
					LocalPort  => $proxy_port,
					Proto	   => conn_proto,
					Type	   => SOCK_STREAM, 
					Listen	   => listen_max,
					Reuse	   => 1) || die("Could not bind listening socket ($proxy_host:$proxy_port): $!");

# create the IO::Select that will control our universe.  add the
# listening socket.
$sel = IO::Select->new($listen_sock);

while (my @handles = IO::Select::select($sel, undef, $sel)) 
{
   # remove any sockets that are in error
   my %removed;
   foreach ( @{ $handles[2] } ) 
   {
      remove_socket($_);
      $removed{$_} = 1;
   }

   # get input from each active socket
 READABLE_SOCKET:
   foreach my $sock ( @{ $handles[0] } ) 
   {

      # make sure that that socket hasn't gone *blammo* yet
      next READABLE_SOCKET
	if exists $removed{$sock};

      # any new sockets?
      if ($sock == $listen_sock) 
      {
	 my $new_sock = $listen_sock->accept
	   or die "accept: $!";
	 add_client_sock($new_sock);
      }
      else 
      {
	 # just move along.
	 process_data($sock);
      }
   }
}
