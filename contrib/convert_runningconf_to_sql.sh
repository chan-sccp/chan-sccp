#!/bin/bash

OUTPUT=output.sql
echo "-- generated output"  >$OUTPUT

declare -A lines
declare -A devices
asterisk -rx "sccp show lines" | grep "0" | while read line
do 
	devname=`echo "$line" |cut -c58-73 |sed -e 's/^ *//' -e 's/ *$//'`
	
	echo "devname: $devname"
	if test "$devname" == "--" ; then
		continue
	fi
	echo "Processing device: $devname"
	exten=`echo "$line" |cut -c3-15 |sed -e 's/^ *//' -e 's/ *$//'`
	if test "$exten" == "+--"; then
		exten=$prevexten
	fi
	label=`echo "$line" |cut -c27-56 |sed -e 's/^ *//' -e 's/ *$//'`
	
	if test -z "${devices[$devname]}"; then
		deviceinfo="`asterisk -rx \"sccp show device $devname\"`"
	
		dev_desc=`echo "$deviceinfo"|grep -e "^Description" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		dev_type=`echo "$deviceinfo"|grep -e "^Skinny Phone Type" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//' -e 's/Cisco \(.*\)(.*)/\1/g'`
		dev_transfer=`echo "$deviceinfo"|grep -e "^Can Transfer" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		#dev_softkeyset=`echo "$deviceinfo"|grep -e "^Softkeyset" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		#dev_dndfeature=`echo "$deviceinfo"|grep -e "^DND Feature enabled" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		#dev_park=`echo "$deviceinfo"|grep -e "^Can Park" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		#dev_nat=`echo "$deviceinfo"|grep -e "^Nat" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		#dev_directrtp=`echo "$deviceinfo"|grep -e "^Direct RTP" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		dev_pickupcontext=`echo "$deviceinfo"|grep -e "^Pickup Context" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//' -e 's/\(.*\)(.*/\1/g'`
		
	        echo "INSERT INTO sccpdevice (type,description,tzoffset,transfer,deny,permit,earlyrtp,mwioncall,pickupcontext,nat,disallow,allow,setvar,name) VALUES ('$dev_type','$dev_desc','0','$dev_transfer',NULL,NULL,'progress','off','$dev_pickupcontext','off','all','alaw;ulaw;g729',NULL,'$devname');" >> $OUTPUT
		devices[$devname]=0	        
	fi
	if test -z "${lines[$exten]}"; then
		lineinfo="`asterisk -rx \"sccp show line $exten\"`"
		line_desc=`echo "$lineinfo"|grep -e "^Description" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_pin=`echo "$lineinfo"|grep -e "^Pin" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_vm=`echo "$lineinfo"|grep -e "^VoiceMail number" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_trnsfvm=`echo "$lineinfo"|grep -e "^Transfer to Voicemail" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_cid_name=`echo "$lineinfo"|grep -e "^Caller ID name" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_cid_num=`echo "$lineinfo"|grep -e "^Caller ID number" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_context=`echo "$lineinfo"|grep -e "^Context" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//' -e 's/\(.*\)(.*/\1/g'`
		line_callgroup=`echo "$lineinfo"|grep -e "^Call Group" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
		line_pickupgroup=`echo "$lineinfo"|grep -e "^Pickup Group" | cut -c50- |sed -e 's/^ *//' -e 's/ *$//'`
	        echo "INSERT INTO sccpline (id,pin,label,description,context,incominglimit,transfer,mailbox,vmnum,cid_name,cid_num,trnsfvm,secondary_dialtone_digits,secondary_dialtone_tone,musicclass,language,accountcode,echocancel,silencesuppression,callgroup,pickupgroup,amaflags,dnd,regexten,setvar,name) VALUES ('$exten','0000','$label','$line_desc','$line_context','4','on','$cid_name','$line_vm','$line_cid_name','$line_cid_num','$line_trnsfvm','9','0x22','default','us','$line_cid_num','on','off','$line_callgroup','$line_pickupgroup',NULL,'on',NULL,'','$exten');" >> $OUTPUT
	        
	        lines[$exten]=1					# add line to array
	fi
	prevexten=$exten
		
	echo "INSERT INTO buttonconfig VALUES ('$devname',${devices[$devname]},'line','$exten','$label');" >> $OUTPUT
	devices[$devname]=$((${devices[$devname]} + 1))
done
