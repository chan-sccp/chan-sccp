#!/usr/bin/env perl
#
# Copyright (c) 2014, Cynjut Consulting Services, LLC. 
# Version 0.8 
#
# Author: Dave Burgess (burgess@cynjut.net)
#
# The program is in the public domain.
#
# Known Issue:
#  - Program does not handle multiple inheritance, so (!,7960) does not work.
#  - The '-g' option seems backwads. The rule of least astonishment says that
#    '-g' should include (not exclude) global data entries.
#

use Getopt::Std;

#
# When we get ready to populate the database, we will be setting these
# fields. Note that these are the field names from the database table. If they 
# match the entries from the sccp.conf, etal, there are no changes that need to 
# be made.
#
# Deprecated values are not handled correctly at this point, so if you have fields 
# in your sccp.conf that match up with the old 'pre-10' semantics. If you want to 
# add deprecated values to the code, look for the bit that changes the devicetype 
# to type.
#
# The strangest bit that people ask about is the '-g' option. Some people don't 
# want to add the explicit values from the [general] context to the database. If 
# you don't, add the '-g'. To change this behavior, just switch the 'general' 
# template names around. As long as the one that you want to hit is called 
# "general" (with or without the -g), you're golden.
#

@device_fields = qw(devicetype addon description tzoffset transfer cfwdall cfwdbusy dtmfmode imageversion deny permit dndFeature directrtp earlyrtp mwilamp mwioncall pickupexten pickupcontext pickupmodeanswer private privacy nat softkeyset audio_tos audio_cos video_tos video_cos conf_allow conf_play_general_announce conf_play_part_announce conf_mute_on_entry conf_music_on_hold_class conf_show_conflist setvar disallow allow backgroundImage ringtone);

@line_fields = qw(id pin label description context incominglimit transfer mailbox vmnum cid_name cid_num trnsfvm secondary_dialtone_digits secondary_dialtone_tone musicclass language accountcode echocancel silencesuppression callgroup pickupgroup amaflags dnd setvar name);


# Perl trim function to remove whitespace from the start and end of the string
sub trim($) {
	my $string = shift;
	$string =~ s/^\s+//;
	$string =~ s/\s+$//;
	return $string;
}
# Left trim function to remove leading whitespace
sub ltrim($) {
	my $string = shift;
	$string =~ s/^\s+//;
	return $string;
}
# Right trim function to remove trailing whitespace
sub rtrim($) {
	my $string = shift;
	$string =~ s/\s+$//;
	return $string;
}
# Equal trim function to remove equal signs
sub rem_equ($) {
	my $string = shift;
	$string =~ s/\s*=\s*/=/;
	$string =~ s/\;.*//;
}

#
# Process infile, including included files.
#
# Uses the global "$pathname" to find the files based on input.
#
sub process($) {
    my $filename = shift;
    $filename = $pathname.$filename;
    my $output = "";
    my $input;
    unless (open($input, "<", $filename)) {
	print STDERR "Can't open $filename: $!\n";
	return;
    }

    while (<$input>) { # note use of indirection
	my $text = rtrim($_);
	if (/^#include .*/) {
	    $text =~ s/^#include //;
	    $output .= process($text);
	} else {
	    $text =~ s/\s*=\s*/=/;
	    $text =~ s/\;.*//;
	    if (length($text) > 1) {
		$output .= "$text\n";
	    }
	}
    }
    return $output;
}

#
# First, let's open the sccp.conf file.
#

getopts('pgo:f:');

if (length($opt_f) > 0) {
    $inf = $opt_f;
}
 
if (length($inf) < 1) {
    print "Usage: sccp [-g] [-p] [-o outfile] -f infile\n";
    print " -f (REQUIRED) uses the full path name for all #include files (if used)\n";
    print " -g suppresses the inclusion of items from the [general] context\n";
    print " -p replaces the MySQL 'REPLACE' with 'INSERT' and 'UPDATE'\n";
    print " -o sends the output of the program to /dev/stdout or the file specified.\n";
    die();
}

$pathname = $inf;
if (index(" $pathname",'/') > 0) {
    $finder = length($pathname);
    while (substr($pathname,$finder-1,1) ne '/') {
	$finder--;
    }
    $inf = substr($pathname,$finder);
    $pathname = substr($pathname,0,$finder);
} else {
    $pathname = './';
}

if (length($opt_o) > 0) {
    $outf = $opt_o;
} else {
    $outf = "/dev/stdout";
}

#
# Read the input file into memory so we can chop it up.
#
$file =  process($inf);
open($outfile,">",$outf);

#
# Right now, '-g' excludes the '[general]' context as defaults.
# If you want '-g' to include the [general] context, switch these or 
# use "if (! $opt_g) ..."
#
if ($opt_g) {
    $general = 'donotinclude';
} else {
    $general = 'general';
}
for (split /^/, $file) {
    $line = trim($_);
    if (/\(\!\)/) {
	$line =~ /\[(.*)\]/;
	$context = "template";
	$template = $1;
    } elsif (/\[.*\]/) {
	if (/\(/) {
	    $line =~ /\[(.*)\].*\((.*)\)/;
	    $context = $1;
	    $template = $2;
#
# If we have a new context with a template, we need to preload the
# context filled out with the default values. This loop should do that.
#
# In addition, we need to pull in all of the 'general' values from the 
# 'general' context so that the defaults from that context make it into
# the records. We use a variable called general with the string "general" 
# in it to allow for indirection.
#
	    for (keys(%$general)) {
		$key = $_;
		$value = $$general{$key};
		$$context{$key} = $value;
	    }
	    $templates{$template} = 1;
	    for (keys(%$template)) {
		$key = $_;
		$value = $$template{$key};
		$$context{$key} = $value;
	    }
	} else {
	    $line =~ /\[(.*)\]/;
	    $context = $1;
	    $template = "";
	    for (keys(%$general)) {
		$key = $_;
		$value = $$general{$key};
		$$context{$key} = $value;
	    }
	}
    } else {
#
# At this point, for each line we know the context and the template for 
# this entry.
#
	if (index($line,'=') > 1) {
	    ($field, $value) = split("=",$line);
	    if ($field eq 'allow' || $field eq 'debug') {
		$value = $$context{$field}.",$value";
		if (substr($value,0,1) eq ',') {
		    $value = substr($value,1);
		}
	    }
	    if ($field eq 'button') {
		$button_index{$context} += 1;
		$field = 'button' . $button_index{$context};
	    }
	    if ($context eq 'template') {
		$templates{$template} = 1;
		$$template{$field} = $value;
	    } else {
		$contexts{$context} = 1;
		$$context{$field} = $value;
	    }
	}
    }
}
for (keys %contexts) {
    $context = $_;
    $query = '';
    for (keys(%$context)) {
	$type = $$context{'type'};
	if ($type eq 'device') {
	    if ($opt_p) {
		$query = "INSERT INTO sccpdevice SET ";
	        $query .= "name = '$context';\n ";
		$query .= "UPDATE sccpdevice SET ";
	    } else {
	        $query = "REPLACE INTO sccpdevice SET ";
	    }
	    $query .= "name = '$context' ";
	    foreach (@device_fields) {
		$field = $_;
		$altcon = 'general';
		$value = $$altcon{$field};
		$value = $$context{$field};
		if (length($value) > 0) {
		    if ($field eq 'devicetype') {
			$query .= ", type = '$value'";
		    } else {
		    	$query .= ", $field = '$value'";
		    }
		}
	    }
	    $query .= ";\n";
	}
	if ($type eq 'line') {
	    if ($opt_p) {
		$query = "INSERT INTO sccpline SET ";
	        $query .= "name = '$context';\n ";
		$query .= "UPDATE sccpline SET ";
	    } else {
	        $query = "REPLACE INTO sccpline SET ";
	    }
	    $query .= "name = '$context' ";
	    foreach (@line_fields) {
		$field = $_;
		$value = $$context{$field};
		if (length($value) > 0) {
		    $query .= ", $field = '$value'";
		}
	    }
	    $query .= ";\n";
	}
    }
    if (length($query) > 1) {
	print $outfile $query;
    }
}
#
# The lines and devices can happen together, but the buttons need special 
# attention
#
for (keys(%contexts)) {
    $context = $_;
    $type = $$context{'type'};
    for (keys(%$context)) {
	$value = $_;
	if (substr($value,0,6) eq 'button') {
	    $instance = substr($value,6);
	    $optline = $$context{$value};
	    ($type,$name,$options,$more) = split(',',$optline);
	    if ($opt_p) {
		$query = "INSERT INTO buttonconfig SET ";
	        $query .= "name = '$context';\n ";
		$query .= "UPDATE buttonconfig SET ";
	    } else {
	        $query = "REPLACE INTO buttonconfig SET ";
	    }
	    $query .= "device = '$context'";
	    $query .= ", instance = $instance";
	    if ($type ne 'line' && $type ne 'speeddial' && 
		$type ne 'service' && $type ne 'feature' && 
		$type ne 'empty') {
		$type = 'speeddial';
	    }
	    $query .= ", type = '$type'";
	    $query .= ", name = '$name'";
	    $options = trim($options);
	    $more = trim($more);
	    if (length($more) > 1) {
		$options .= ','.$more;
	    }
	    $query .= ",options = '$options'";
	    $query .= ";\n";
	    print $outfile $query;
	}
    }
}
close($outfile);
