<?php

if (!defined('FREEPBX_IS_AUTH')) {
    die_freepbx('No direct script access allowed');
}

global $db;
global $amp_conf;
global $version;
global $aminterface;
global $extconfigs;
global $mobile_hw;
global $settingsFromDb;
global $thisInstaller;
global $cnf_int;
global $sccp_compatible;
global $cnf_wr;

$mobile_hw = '0';
$autoincrement = (($amp_conf["AMPDBENGINE"] == "sqlite") || ($amp_conf["AMPDBENGINE"] == "sqlite3")) ? "AUTOINCREMENT" : "AUTO_INCREMENT";
$table_req = array('sccpdevice', 'sccpline', 'sccpsettings');
$sccp_compatible = 0;
$db_config = '';
$sccp_version = array();
$cnf_int = \FreePBX::Config();

// Do not create Sccp_Manager object as not required.
// Only include required classes and create anonymous class for thisInstaller
$thisInstaller = new class{
    use \FreePBX\modules\Sccp_Manager\sccpManTraits\helperFunctions;
	public $xml_data;     // Добавьте эту строку
    public $sccpvalues; 
};

$requiredClasses = array('aminterface', 'extconfigs');
foreach ($requiredClasses as $className) {
    $class = "\\FreePBX\\Modules\\Sccp_manager\\$className";
    if (!class_exists($class, false)) {
        include(__DIR__ . "/sccpManClasses/$className.class.php");
    }
    if (class_exists($class, false)) {
        $$className = new $class();
    }
}

CheckAsteriskVersion();
$sccp_compatible = $aminterface->getSCCPVersion()['vCode'];

outn("<li>" . _("Sccp model Compatible code : ") . $sccp_compatible . "</li>");
if ($sccp_compatible == 0) {
    outn("<br>");
    outn("<font color='red'>chan-sccp not found. Install it before continuing !</font>");
    die();
}
// BackUp Old config
createBackUpConfig();
RenameConfig();

$db_config   = Get_DB_config($sccp_compatible);
InstallDB_updateSchema($db_config);

cleanUpSccpSettings();

InstallDB_createButtonConfigTrigger();
InstallDbCreateViews($sccp_compatible);
installDbPopulateSccpline();
InstallDB_updateDBVer($sccp_compatible);

Setup_RealTime();
addDriver($sccp_compatible);
checkTftpServer();

outn("<br>");
outn("Install Complete !");
outn("<br>");

// Functions follow

function Get_DB_config($sccp_compatible)
{
    global $mobile_hw;
    // Software mobile
    $db_config_v4 = array(
        'sccpdevmodel' => array(
            'enabled' => array('create' => "INT(2) NULL DEFAULT '0'"),
            'nametemplate' => array('create' => 'VARCHAR(50) NULL DEFAULT NULL'),
            'loadinformationid' => array('create' => "VARCHAR(30) NULL DEFAULT NULL")
        ),
        'sccpdevice' => array(
            'pickupexten' => array('drop' => "yes"),
            'directed_pickup' => array('drop' => "yes"),
            'directed_pickup_context' => array('drop' => "yes"),
            'pickupcontext' => array('drop' => "yes"),
            'directed_pickup_modeanswer' => array('drop' => "yes"),
            'pickupmodeanswer' => array('drop' => "yes"),
            'disallow' => array('drop' => "yes"),
            'callhistory_answered_elsewhere' => array('create' => "enum('Ignore','Missed Calls','Received Calls', 'Placed Calls') NOT NULL default 'Ignore'",
                                                      'modify' => "enum('Ignore','Missed Calls','Received Calls','Placed Calls')"),
            'hwlang' => array('rename' => "_hwlang"),
            '_hwlang' => array('create' => 'VARCHAR(12) NULL DEFAULT NULL'),
            '_loginname' => array('create' => 'VARCHAR(20) NULL DEFAULT NULL AFTER `_hwlang`'),
            '_profileid' => array('create' => "INT(11) NOT NULL DEFAULT '0' AFTER `_loginname`"),
            '_dialrules' => array('create' => "VARCHAR(255) NULL DEFAULT NULL AFTER `_profileid`"),
            'useRedialMenu' => array('create' => "enum('yes','no') NOT NULL default 'no'", 'modify' => "enum('yes','no')"),
            'dtmfmode' => array('drop' => "yes"),
            'force_dtmfmode' => array('create' => "ENUM('auto','rfc2833','skinny') NOT NULL default 'auto'",
                          'modify' => "ENUM('auto','rfc2833','skinny')"),
            'deny' => array('create' => 'VARCHAR(100) NULL DEFAULT NULL', 'modify' => "VARCHAR(100)"),
            'permit' => array('create' => 'VARCHAR(100) NULL DEFAULT NULL', 'modify' => "VARCHAR(100)"),
            'backgroundImage' => array('create' => 'VARCHAR(255) NULL DEFAULT NULL', 'modify' => "VARCHAR(255)"),
            'ringtone' => array('create' => 'VARCHAR(255) NULL DEFAULT NULL', 'modify' => "VARCHAR(255)"),
            'transfer' => array('create' => "enum('no','yes') NOT NULL default 'no'", 'modify' => "enum('no','yes')"),
            'cfwdall' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'cfwdbusy' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'cfwdnoanswer' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'park' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
            'directrtp' => array('create' => "enum('no','yes') NOT NULL default 'no'", 'modify' => "enum('no','yes')"),
            'dndFeature' => array('create' => "enum('off','on') NOT NULL default 'off'", 'modify' => "enum('off','on')"),
            'earlyrtp' => array('create' => "ENUM('yes','no') NOT NULL default 'yes'", 'modify' => "ENUM('yes','no')"),
            'monitor' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
            'audio_tos' => array('create' => "VARCHAR(11) NOT NULL default '0xB8'",'modify' => "0xB8"),
            'audio_cos' => array('create' => "VARCHAR(11) NOT NULL default '0x6'",'modify' => "0x6"),
            'video_tos' => array('create' => "VARCHAR(11) NOT NULL default '0x88'",'modify' => "0x88"),
            'video_cos' => array('create' => "VARCHAR(11) NOT NULL default '0x5'",'modify' => "0x5"),
            'trustphoneip' => array('drop' => "yes"),
            'transfer_on_hangup' => array('create' => "enum('yes','no') NOT NULL DEFAULT 'no'", 'modify' => "enum('yes','no')"),
            'phonecodepage' => array('create' => 'VARCHAR(50) NULL DEFAULT NULL', 'modify' => "VARCHAR(50)"),
            'mwilamp' => array('create' => "enum('on','off','wink','flash','blink') NOT NULL  default 'on'",
                              'modify' => "enum('on','off','wink','flash','blink')"),
            'mwioncall' => array('create' => "enum('no','yes') NOT NULL default 'yes'",'modify' => "enum('no','yes')"),
            'private' => array('create' => "enum('yes','no') NOT NULL default 'no'", 'modify' => "enum('yes','no')"),
            'privacy' => array('create' => "enum('full','on','off') NOT NULL default 'full'", 'modify' => "enum('full','on','off')"),
            'nat' => array('create' => "enum('on','off','auto') NOT NULL default 'off'", 'modify' => "enum('on','off','auto')"),
            'conf_allow' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
            'conf_play_part_announce' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
            'conf_mute_on_entry' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
            'conf_show_conflist' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
            'type' => array('create' => 'VARCHAR(15) NULL DEFAULT NULL', 'modify' => "VARCHAR(15)"),
            'imageversion' => array('create' => 'VARCHAR(31) NULL DEFAULT NULL', 'modify' => "VARCHAR(31)"),
            'softkeyset' => array('def_modify' => "softkeyset")
        ),
        'sccpline' => array(
            'directed_pickup' => array('create' => "enum('yes','no') NOT NULL default 'no'", 'modify' => "enum('yes','no')"),
            'directed_pickup_context' => array('create' => "VARCHAR(100) NULL DEFAULT NULL"),
            'pickup_modeanswer' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'namedcallgroup' => array('create' => "VARCHAR(100) NULL DEFAULT NULL AFTER `setvar`", 'modify' => "VARCHAR(100)"),
            'namedpickupgroup' => array('create' => "VARCHAR(100) NULL DEFAULT NULL AFTER `namedcallgroup`", 'modify' => "VARCHAR(100)"),
            'adhocNumber' => array('create' => "VARCHAR(45) NULL DEFAULT NULL AFTER `namedpickupgroup`"),
            'meetme' => array('create' => "VARCHAR(5) NULL DEFAULT NULL AFTER `adhocNumber`"),
            'context' => array('create' => "VARCHAR(45) NULL DEFAULT NULL AFTER `description`", 'def_modify' => 'from-internal'),
            'meetmenum' => array('create' => "VARCHAR(45) NULL DEFAULT NULL AFTER `meetme`"),
            'meetmeopts' => array('create' => "VARCHAR(45) NULL DEFAULT NULL AFTER `meetmenum`"),
            'regexten' => array('create' => "VARCHAR(45) NULL DEFAULT NULL AFTER `meetmeopts`"),
            'rtptos' => array('drop' => "yes"),
            'audio_tos' => array('drop' => "yes"),
            'audio_cos' => array('drop' => "yes"),
            'video_tos' => array('drop' => "yes"),
            'video_cos' => array('drop' => "yes"),
            'videomode' => array('create' => "enum('auto','user','off') NOT NULL default 'auto'", 'modify' => "enum('auto','user','off')"),
            'incominglimit' => array('create' => "INT(11) DEFAULT '6'", 'modify' => 'INT(11)', 'def_modify' => "6"),
            'transfer' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'vmnum' => array('def_modify' => "*97"),
            'musicclass' => array('def_modify' => "default"),
            'disallow' => array('create' => "VARCHAR(255) NULL DEFAULT 'all'", 'modify' => 'VARCHAR(255)'),
            'allow' => array('create' => "VARCHAR(255) NULL DEFAULT NULL"),
            'id' => array('create' => 'MEDIUMINT(9) NOT NULL AUTO_INCREMENT, ADD UNIQUE(id);', 'modify' => "MEDIUMINT(9)", 'index' => 'id'),
            'echocancel' => array('create' => "enum('yes','no') NOT NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'silencesuppression' => array('create' => "enum('no','yes') NOT NULL default 'no'", 'modify' => "enum('no','yes')"),
            'dnd' => array('create' => "enum('reject','off','silent','user') NOT NULL default 'reject'", 'modify' => "enum('reject','off','silent','user')", 'def_modify' => "reject")
        ),
        'sccpuser' => array(
            'name' => array('create' => "VARCHAR(20) NOT NULL", 'modify' => "VARCHAR(20)" ),
            'pin' => array('create' => "VARCHAR(7) NOT NULL", 'modify' => "VARCHAR(7)" ),
            'password' => array('create' => "VARCHAR(7) NOT NULL", 'modify' => "VARCHAR(7)" ),
            'description' => array('create' => "VARCHAR(45) NOT NULL", 'modify' => "VARCHAR(45)" ),
            'roaminglogin' => array('create' => "ENUM('off','on','multi') NOT NULL DEFAULT 'off'", 'modify' => "ENUM('on','off','multi')" ),
            'auto_logout' => array('create' => "ENUM('on','off') NOT NULL DEFAULT 'off'", 'modify' => "ENUM('on','off')" ),
            'homedevice' => array('create' => "VARCHAR(20) NOT NULL", 'modify' => "VARCHAR(20)" ),
            'devicegroup' => array('create' => "VARCHAR(7) NOT NULL", 'modify' => "VARCHAR(7)" ),
        ),
        'sccpbuttonconfig' => array(
            'reftype' => array('create' => "enum('sccpdevice', 'sipdevice', 'sccpuser') NOT NULL default 'sccpdevice'",
                        'modify' => "enum('sccpdevice', 'sipdevice', 'sccpuser')" ),
        )
    );
//  Hardware Mobile.  Can switch Software to Hardware
    $db_config_v4M = array(
        'sccpdevmodel' => array(
            'loadinformationid' => array('create' => "VARCHAR(30) NULL DEFAULT NULL")
        ),
        'sccpdevice' => array(
            'pickupexten' => array('drop' => "yes"),
            'directed_pickup' => array('drop' => "yes"),
            'cfwdnoanswer' => array('create' => "enum('yes','no') NULL default 'yes'", 'modify' => "enum('yes','no')"),
            'park' => array('create' => "enum('on','off') NULL default 'on'", 'modify' => "enum('on','off')"),
            'monitor' => array('create' => "enum('on','off') NULL default NULL", 'modify' => "enum('on','off')"),
            '_description' => array('rename' => "description"),
            '_loginname' => array('drop' => "yes"),
            '_profileid' => array('drop' => "yes"),
            '_dialrules' => array('create' => "VARCHAR(255) NULL DEFAULT NULL AFTER `_profileid`"),
            'transfer_on_hangup' => array('create' => "enum('on','off') NULL DEFAULT NULL", 'modify' => "enum('on','off')"),
        ),
        'sccpline' => array(
            'directed_pickup' => array('create' => "enum('on','off') NULL default NULL", 'modify' => "enum('on','off')"),
            'videomode' => array('create' => "enum('user','auto','off') NULL default 'auto'", 'modify' => "enum('user','auto','off')"),
        ),
        'sccpuser' => array(
            'id' => array('create' => "VARCHAR(20) NOT NULL", 'modify' => "VARCHAR(20)" ),
            'name' => array('create' => "VARCHAR(45) NOT NULL", 'modify' => "VARCHAR(45)" ),
        )
    );
    // Below fields allow configuration of these settings on a per device basis
    // whereas previously they were all global.Some of these are not supported by chan-sccp;
    // The supported fields are listed in the view sccpdeviceconfig.
    $db_config_v5 = array(
        'sccpdevice' => array(
              'logserver' => array('create' => "VARCHAR(100) NULL default null", 'modify' => "VARCHAR(20)"),
              'daysdisplaynotactive' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'displayontime' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'displayonduration' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'displayidletimeout' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'settingsaccess' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'videocapability' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'webaccess' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'webadmin' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'pcport' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
              'spantopcport' => array('create' => "enum('on','off') NOT NULL default 'on'", 'modify' => "enum('on','off')"),
              'voicevlanaccess' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'enablecdpswport' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'enablecdppcport' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'enablelldpswport' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'enablelldppcport' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'firstdigittimeout' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'digittimeout' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'cfwdnoanswer_timeout' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'autoanswer_ring_time' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'autoanswer_tone' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'remotehangup_tone' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'transfer_tone' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'callwaiting_tone' => array('create' => "VARCHAR(20) NULL default null", 'modify' => "VARCHAR(20)"),
              'callanswerorder' => array('create' => "enum('oldestfirst','latestfirst') NOT NULL default 'latestfirst'",
                                    'modify' => "enum('oldestfirst','latestfirst')"),
              'sccp_tos' => array('create' => "VARCHAR(11) NOT NULL default '0x68'", 'modify' => "VARCHAR(11)"),
              'sccp_cos' => array('create' => "VARCHAR(11) NOT NULL default '0x4'", 'modify' => "VARCHAR(11)"),
              'dev_sshPassword' => array('create' => "VARCHAR(25) NOT NULL default 'cisco'"),
              'dev_sshUserId' => array('create' => "VARCHAR(25) NOT NULL default 'cisco'"),
              'phonepersonalization' => array('create' => "VARCHAR(25) NOT NULL default '0'", 'modify' => "VARCHAR(25)"),
              'loginname' => array('create' => 'VARCHAR(20) NULL DEFAULT NULL'),
              'profileid' => array('create' => "INT(11) NOT NULL DEFAULT '0'"),
              'dialrules' => array('create' => "VARCHAR(255) NULL DEFAULT NULL"),
              'description' => array('create' => "VARCHAR(45) NULL DEFAULT NULL"),
              '_hwlang' => array ('drop' => 'yes'),
              'devlang' => array('create' => "VARCHAR(50) NULL default NULL", 'modify' => "VARCHAR(50)"),
              'netlang' => array('create' => "VARCHAR(50) NULL default NULL", 'modify' => "VARCHAR(50)"),
              '_devlang' => array('rename' => "devlang"),
              '_netlang' => array('rename' => "netlang"),
              '_logserver' => array('rename' => 'logserver'),
              '_daysdisplaynotactive' => array('rename' =>  'daysdisplaynotactive'),
              '_displayontime' => array('rename' =>  'displayontime'),
              '_displayonduration' => array('rename' => 'displayonduration'),
              '_displayidletimeout' => array('rename' => 'displayidletimeout'),
              '_settingsaccess' => array('rename' => 'settingsaccess'),
              '_videocapability' => array('rename' =>  'videocapability'),
              '_webaccess' => array('rename' =>  'webaccess'),
              '_webadmin' => array('rename' =>  'webadmin'),
              '_pcport' => array('rename' =>  'pcport'),
              '_spantopcport' => array('rename' => 'spantopcport'),
              '_voicevlanaccess' => array('rename' => 'voicevlanaccess'),
              '_enablecdpswport' => array('rename' => 'enablecdpswport'),
              '_enablecdppcport' => array('rename' => 'enablecdppcport'),
              '_enablelldpswport' => array('rename' => 'enablelldpswport'),
              '_enablelldppcport' => array('rename' =>  'enablelldppcport'),
              '_firstdigittimeout' => array('rename' => 'firstdigittimeout'),
              '_digittimeout' => array('rename' =>  'digittimeout'),
              '_cfwdnoanswer_timeout' => array('rename' => 'cfwdnoanswer_timeout'),
              '_autoanswer_ring_time' => array('rename' => 'autoanswer_ring_time'),
              '_autoanswer_tone' => array('rename' => 'autoanswer_tone'),
              '_remotehangup_tone' => array('rename' => 'remotehangup_tone'),
              '_transfer_tone' => array('rename' => 'transfer_tone'),
              '_callwaiting_tone' => array('rename' => 'callwaiting_tone'),
              '_callanswerorder' => array('rename' => 'callanswerorder'),
              '_sccp_tos' => array('rename' => 'sccp_tos'),
              '_sccp_cos' => array('rename' => 'sccp_cos'),
              '_dev_sshPassword' => array('rename' => 'dev_sshPassword'),
              '_dev_sshUserId' => array('rename' => 'dev_sshUserId'),
              '_phonepersonalization' => array('rename' => '_phonepersonalization'),
              '_loginname' => array('rename' => 'loginname'),
              '_profileid' => array('rename' => 'profileid'),
              '_dialrules' => array('rename' => 'dialrules'),
              '_description' => array('rename' => 'description'),
              'keepalive' => array('create' => "INT(11) DEFAULT '60'", 'modify' => 'INT(11)', 'def_modify' => "60")
            ),
        'sccpline' => array(
              'regcontext' => array('create' => "VARCHAR(20) NULL default 'sccpregistration'", 'modify' => "VARCHAR(20)"),
              'transfer_on_hangup' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'autoselectline_enabled' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'autocall_select' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'backgroundImageAccess' => array('create' => "enum('on','off') NOT NULL default 'off'", 'modify' => "enum('on','off')"),
              'callLogBlfEnabled' => array('create' => "enum('3','2') NOT NULL default '2'", 'modify' => "enum('3','2')"),
              '_regcontext' => array('rename' => 'regcontext'),
              '_transfer_on_hangup' => array('rename' => 'transfer_on_hangup'),
              '_autoselectline_enabled' => array('rename' => 'autoselectline_enabled'),
              '_autocall_select' => array('rename' => 'autocall_select'),
              '_backgroundImageAccess' => array('rename' => 'backgroundImageAccess'),
              '_callLogBlfEnabled' => array('rename' => 'callLogBlfEnabled')
            ),
        'sccpsettings' => array(
              'systemdefault' => array('create' => "VARCHAR(255) NULL default ''")
            ),
        'sccpdevmodel' => array(
                'fwfound' => array('create' => "enum('yes','no') NOT NULL default 'no'", 'modify' => "enum('yes','no')"),
                'templatefound' => array('create' => "enum('yes','no') NOT NULL default 'no'", 'modify' => "enum('yes','no')")
            )
    );

    if ($sccp_compatible >= 433) {
        if ($mobile_hw == '1') {
            return $db_config_v4M;
        }
        // This looks extraneous, but is for future compatibility - do not delete
        // If integrated into chan-sccp, the version number will change
        if ($sccp_compatible >= 433) {
            $db_config_v4['sccpdevice'] = array_merge($db_config_v4['sccpdevice'],$db_config_v5['sccpdevice']);
            $db_config_v4['sccpline'] = array_merge($db_config_v4['sccpline'],$db_config_v5['sccpline']);
            $db_config_v4['sccpsettings'] = $db_config_v5['sccpsettings'];
            $db_config_v4['sccpdevmodel'] = $db_config_v5['sccpdevmodel'];
        }
        return $db_config_v4;
    }
}

function CheckSCCPManagerDBVersion()
{

}

/* notused */

function CheckPermissions()
{
    global $amp_conf;
    outn("<li>" . _("Checking Filesystem Permissions") . "</li>");
    $dst = $amp_conf['AMPWEBROOT'] . '/admin/modules/sccp_manager/views';
    if (fileowner($amp_conf['AMPWEBROOT']) != fileowner($dst)) {
        die_freepbx('Please (re-)check permissions by running "amportal chown. Installation Failed"');
    }
}

function CheckAsteriskVersion()
{
    $version = FreePBX::Config()->get('ASTVERSION');
    outn("<li>" . _("Checking Asterisk Version : ") . $version . "</li>");
    if (!empty($version)) {
        // Woo, we have a version
        if (version_compare($version, "12.2.0", ">=")) {
            $ver_compatible = true;
        } else {
            die_freepbx('Asterisk Version is to old, please upgrade to asterisk-12 or higher. Installation Failed');
        }
    } else {
        // Well. I don't know what version of Asterisk I'm running.
        // Assume less than 12.
        $ver_compatible = false;
        die_freepbx('Asterisk Version could not be verified. Installation Failed');
    }
    return $ver_compatible;
}

function CheckChanSCCPCompatible()
{
    global $aminterface;
    return $aminterface->getSCCPVersion['vCode'];
}

function InstallDB_updateSchema($db_config)
{
    /*
    Initially, referred to schema existing and created db from scratch. This required
    initialising sccp_manager object which in term required that sccpsettings existed
    To overcome this, moved to creation/modification of db via module.xml.
    In early version of 14.3, only used initial fields present in older versions, and then
    added new required fields. This resulted in new fields being dropped at next install.
    Now include all fields in module.xml, and then transfer any data in old fields to new fields
    before then deleting old fields, rather than renaming columns which will no longer work as
    the new column already exists, and may contain data. This affected 36 fields in sccpdevice
    and 5 fields in sccpline which in old db schema used _ prefix to hide from chan-sccp.
    */
    global $db;
    if (!$db_config) {
        die_freepbx("No db_config provided");
    }
    outn("<li>" . _("Saving legacy data into current column") . "</li>");
    $priorSchemaFields = array(
        'sccpdevice' => array(
            '_description', '_loginname', '_profileid', '_dialrules', '_devlang', '_netlang', '_logserver',
            '_daysdisplaynotactive', '_displayontime', '_displayonduration', '_displayidletimeout', '_settingsaccess', '_videocapability',
            '_webaccess', '_webadmin', '_pcport', '_spantopcport', '_voicevlanaccess', '_enablecdpswport', '_enablecdppcport',
            '_enablelldpswport', '_enablelldppcport', '_firstdigittimeout', '_digittimeout', '_cfwdnoanswer_timeout',
            '_autoanswer_ring_time', '_autoanswer_tone', '_remotehangup_tone', '_transfer_tone', '_callwaiting_tone',
            '_callanswerorder', '_sccp_tos', '_sccp_cos', '_dev_sshPassword', '_dev_sshUserId', '_phonepersonalization'),
        'sccpline' => array(
            '_regcontext', '_transfer_on_hangup', '_autoselectline_enabled', '_autocall_select', '_backgroundImageAccess', '_callLogBlfEnabled')
        );
    foreach ($priorSchemaFields as $table => $fieldsArr) {
        // First get any data in columns to be deleted ( _Column)
        $sqlMatch = array_reduce($fieldsArr, function($carry, $column) {
                return "{$carry}  {$column} IS NOT NULL OR";
        });
        unset($column);
        $sqlFields = array_reduce($fieldsArr, function($carry, $column) {
                return "{$carry}  {$column} AS " . ltrim($column,"_") .",";
        });

        $sqlMatch = rtrim($sqlMatch, "OR");
        $sqlFields = rtrim($sqlFields, ",");
        $stmt = $db->prepare("SELECT name, {$sqlFields} FROM {$table} WHERE {$sqlMatch}");
        $stmt->execute();
        $dbResult = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        // Now move any data found from _Column to Column. This is safe as the two should not exist.
        if (!empty($dbResult)) {
            foreach ($dbResult as $name => $columnArr) {
                $sqlVar = array_reduce(array_keys($columnArr), function($carry, $key) use ($columnArr){
                        $carry .= (isset($columnArr[$key])) ? "{$key} = '{$columnArr[$key]}'," : "";
                        return $carry;
                });
                $sqlVar = rtrim($sqlVar, ",");
                $stmt = $db->prepare("UPDATE {$table} SET {$sqlVar} WHERE name = '{$name}'");
                $stmt->execute();
            }
        }
        // Processed all _Column names; now safe to delete them
        $sqlDrop = array_reduce($fieldsArr, function($carry, $column) {
                return "{$carry} DROP COLUMN {$column},";
        });
        $sqlDrop = rtrim($sqlDrop, ", ");
        $stmt = $db->prepare("ALTER TABLE {$table} {$sqlDrop}");
        $stmt->execute();
    }

    // Now process the column updates as per the legacy installer
    $count_modify = 0;
    outn("<li>" . _("Modifying Database schema") . "</li>");
    foreach ($db_config as $tabl_name => $tab_modif) {
        $sql_create = '';
        $sql_modify = '';
        $sql_rename = '';

        $stmt = $db->prepare("DESCRIBE {$tabl_name}");
        $stmt->execute();
        $db_result = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        if (DB::IsError($db_result)) {
            die_freepbx("Can not get information for " . $tabl_name . " table\n");
        }

        // filter modifications based on field existance and prepare sql
        foreach ($db_result as $fld_id => $tabl_data) {
            if (!empty($tab_modif[$fld_id])) {
                // have column in table so potentially something to update
                // if dropping column, prepare sql and continue. The drop case will never
                // occur as columns that are dropped should no longer be in the module.xml schema
                // and so Doctrine will have already dropped them.
                if (!empty($tab_modif[$fld_id]['drop'])) {
                    $sql_create .= "DROP COLUMN {$row_fld}, ";
                    unset($tab_modif[$fld_id]['drop']);
                    continue;
                }

                if (!empty($tab_modif[$fld_id]['modify'])) {
                    // Check if modify type is same as current type
                    if (strtoupper($tab_modif[$fld_id]['modify']) == strtoupper($tabl_data['Type'])) {
                        // Type has not changed so unset
                        unset($tab_modif[$fld_id]['modify']);
                    } else {
                        if (!empty($tab_modif[$fld_id]['def_modify'])) {
                            // if a default has been modified, use it here and unset
                            $sql_modify .= "MODIFY COLUMN {$fld_id} {$tab_modif[$fld_id]['modify']} DEFAULT '{$tab_modif[$fld_id]['def_modify']}', ";
                            // def_modify has been used so unset
                            unset($tab_modif[$fld_id]['def_modify']);
                        } else if (!empty($tab_modif[$fld_id]['create'])) {
                            // use create attributes
                            $sql_modify .= "MODIFY COLUMN {$fld_id} {$tab_modif[$fld_id]['create']}, ";
                        } else {
                            // No default to modify so leave unchanged
                            $sql_modify .= "MODIFY COLUMN {$fld_id}  {$tab_modif[$fld_id]['modify']}, ";
                        }
                        $count_modify ++;
                    }
                }

                if (!empty($tab_modif[$fld_id]['def_modify'])) {
                    // Check if def_modify value is same as current value
if (strtoupper($tab_modif[$fld_id]['def_modify'] ?? '') == strtoupper($tabl_data['Default'] ?? '')) {
                        // Defaults have not changed so unset
                        unset($tab_modif[$fld_id]['def_modify']);
                    } else {
                        $sql_modify .= "ALTER COLUMN {$fld_id} SET DEFAULT '{$tab_modif[$fld_id]['def_modify']}', ";
                        $count_modify ++;
                    }
                }
                // Now handle rename. Need to be carefull that the newname does not exist. If it does then need to
                // ignore or unexpected things may happen.
                if (!empty($tab_modif[$fld_id]['rename'])) {
                    // Field currently exists so need to rename (and keep data). All of legacy _columns have already
                    // been dropped so will not get here as $fld_id cannot be _column.
                    // for backward compatibility use CHANGE - REPLACE is only available in MariaDb > 10.5.
                    $fld_id_newName = $tab_modif[$fld_id]['rename'];

                    // Only execute if the newname column does not already exist.
                    if (empty($db_result[$fld_id_newName])) {
                        // Trying to rename to an existing column. Ignore this case.
                        // Does a create exist for newName
                        if (!empty($tab_modif[$fld_id_newName]['create'])) {
                            //carry the attributes from the new create to the rename
                            $sql_rename .= "CHANGE COLUMN {$fld_id} {$fld_id_newName} {$tab_modif[$fld_id_newName]['create']}, ";
                            // do not create newName as modifying existing
                            unset($tab_modif[$fld_id_newName]['create']);
                        } else {
                            // add current attributes to the new name.
                            $existingAttrs = strtoupper($tabl_data['Type']).(($tabl_data['Null'] == 'NO') ?' NOT NULL': ' NULL') .
                                            ((empty($tabl_data['Default']))?'': ' DEFAULT ' . "'" . $tabl_data['Default']."'");
                            $sql_rename .= "CHANGE COLUMN {$fld_id} {$fld_id_newName} {$existingAttrs}, ";
                        }
                        $count_modify ++;
                    }
                    // Have treated this rename so unset. If the newname already exists, have done nothing.
                    unset($tab_modif[$fld_id]['rename']);
                }
                // is there a create for this field
                if (!empty($tab_modif[$fld_id]['create'])) {
                    // unset as cannot create existing field
                    unset($tab_modif[$fld_id]['create']);
                }
            }
        }
        // only case left to handle is create as all others handled above
        foreach ($tab_modif as $row_fld => $row_data) {
            if (!empty($row_data['create'])) {
                $sql_create .= "ADD COLUMN {$row_fld} {$row_data['create']}, ";
                $count_modify ++;
            }
        }
        //Now execute sql statements
        if (!empty($sql_create)) {
            outn("<li>" . _("Adding/dropping columns ...") . "</li>");
            $sql_create = "ALTER TABLE {$tabl_name} " .substr($sql_create, 0, -2);
            try {
            $check = $db->query($sql_create);
            } catch (\Exception $e) {
                die_freepbx("Can't add/drop column in {$tabl_name}. SQL:  {$sql_create} \n");
            }
        }
        if (!empty($sql_modify)) {
            outn("<li>" . _("Modifying table columns ") . $tabl_name ."</li>");
            $sql_modify = "ALTER TABLE {$tabl_name} " . substr($sql_modify, 0, -2);
            try {
                $check = $db->query($sql_modify);
            } catch (\Exception $e) {
                  die_freepbx("Can not modify {$tabl_name}. SQL:  {$sql_modify} \n");
            }
        }
        if (!empty($sql_rename)) {
            outn("<li>" . _("Renaming table columns ") . $tabl_name ."</li>");
            $sql_rename = "ALTER TABLE {$tabl_name} " . substr($sql_rename, 0, -2);
            try {
                $check = $db->query($sql_rename);
            } catch (\Exception $e) {
                  die_freepbx("Can not modify {$tabl_name}. SQL:  {$sql_rename} \n");
            }
        }
    }
    outn("<li>" . _("Total modify count :") . $count_modify . "</li>");

    // Now see if sccpdevmodel is populated.
$devModelArr = array( 
    "('1000', 'TELEPRESENCE', 1, 1, '', 'loadInform', 0, NULL)",
    "('1100', 'TELEPRESENCE', 1, 1, '', 'loadInforma', 0, NULL)",
    "('12 SP', 'CISCO', 1, 1, '', 'loadInformation3', 0, NULL)",
    "('12 SP+', 'CISCO', 1, 1, '', 'loadInformation2', 0, NULL)",
    "('1300-47', 'TELEPRESENCE', 1, 1, '', 'loadInformation591', 0, NULL)",
    "('1300-65', 'TELEPRESENCE', 1, 1, '', 'loadInformation505', 0, NULL)",
    "('200', 'TELEPRESENCE', 1, 1, '', 'loadInformat', 0, NULL)",
    "('30 SP+', 'CISCO', 1, 1, '', 'loadInformation1', 0, NULL)",
    "('30 VIP', 'CISCO', 1, 1, '', 'loadInformation5', 0, NULL)",
    "('3000', 'TELEPRESENCE', 1, 1, '', 'loadInforma', 0, NULL)",
    "('3200', 'TELEPRESENCE', 1, 1, '', 'loadInforma', 0, NULL)",
    "('3911', 'CISCO', 1, 1, '', 'loadInformation446', 0, NULL)",
    "('3951', 'CISCO', 1, 1, '', 'loadInformation412', 0, '')",
    "('6901', 'CISCO', 1, 1, 'SCCP6901.9-2-1-a', 'loadInformation547', 0, NULL)",
    "('6911', 'CISCO', 1, 1, 'SCCP6911.9-2-1-a', 'loadInformation548', 0, NULL)",
    "('6921', 'CISCO', 1, 1, 'SCCP69xx.9-4-1-3SR3', 'loadInformation496', 0, NULL)",
    "('6941', 'CISCO', 1, 1, 'SCCP69xx.9-3-1-3', 'loadInformation495', 0, NULL)",
    "('6945', 'CISCO', 1, 1, 'SCCP6945.9-3-1-3', 'loadInformation564', 0, NULL)",
    "('6961', 'CISCO', 1, 1, 'SCCP69xx.9-2-1-0', 'loadInformation497', 0, NULL)",
    "('7902', 'CISCO', 1, 1, 'CP7902080002SCCP060817A', 'loadInformation30008', 0, NULL)",
    "('7905', 'CISCO', 1, 1, 'CP7905080003SCCP070409A', 'loadInformation20000', 0, NULL)",
    "('7906', 'CISCO', 1, 1, 'SCCP11.9-4-2SR3-1S', 'loadInformation369', 1, 'SEP0000000000.cnf.xml_791x_template')",
    "('7910', 'CISCO', 1, 1, 'P00405000700', 'loadInformation6', 1, 'SEP0000000000.cnf.xml_791x_template')",
    "('7911', 'CISCO', 1, 1, 'SCCP11.9-4-2SR3-1S', 'loadInformation307', 1, 'SEP0000000000.cnf.xml_791x_template')",
    "('7912', 'CISCO', 1, 1, 'CP7912080004SCCP080108A', 'loadInformation30007', 0, NULL)",
    "('7914', 'CISCO', 0, 14, 'S00105000400', 'loadInformation124', 1, NULL)",
    "('7914;7914', 'CISCO', 0, 28, 'S00105000400', 'loadInformation124', 1, NULL)",
    "('7915', 'CISCO', 0, 24, 'B015-1-0-4-2', 'loadInformation227', 1, NULL)",
    "('7915;7915', 'CISCO', 0, 48, 'B015-1-0-4-2', 'loadInformation228', 1, NULL)",
    "('7916', 'CISCO', 0, 24, 'B016-1-0-4-2', 'loadInformation229', 1, NULL)",
    "('7916;7916', 'CISCO', 0, 48, 'B016-1-0-4-2', 'loadInformation230', 1, NULL)",
    "('7920', 'CISCO', 1, 1, 'cmterm_7920.4.0-03-02', 'loadInformation30002', 0, NULL)",
    "('7921', 'CISCO', 1, 1, 'CP7921G-1.4.6.3', 'loadInformation365', 0, NULL)",
    "('7925', 'CISCO', 1, 6, 'CP7925G-1.4.1SR1', 'loadInformation484', 0, 'SEP0000000000.cnf.xml_7925_template')",
    "('7926', 'CISCO', 1, 1, 'CP7926G-1.4.5.3', 'loadInformation577', 0, '')",
    "('7931', 'CISCO', 1, 34, 'SCCP31.9-2-1S', 'loadInformation348', 0, NULL)",
    "('7935', 'CISCO', 1, 2, 'P00503021900', 'loadInformation9', 0, NULL)",
    "('7936', 'CISCO', 1, 1, 'cmterm_7936.3-3-21-0', 'loadInformation30019', 0, NULL)",
    "('7937', 'CISCO', 1, 1, 'apps37sccp.1-4-5-7', 'loadInformation431', 0, 'SEP0000000000.cnf.xml_7937_template')",
    "('7940', 'CISCO', 1, 2, 'P0030801SR02', 'loadInformation8', 1, 'SEP0000000000.cnf.xml_7940_template')",
    "('7941', 'CISCO', 1, 2, 'SCCP41.9-4-2SR3-1S', 'loadInformation115', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7941G-GE', 'CISCO', 1, 2, 'SCCP41.9-4-2SR3-1S', 'loadInformation309', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7942', 'CISCO', 1, 2, 'SCCP42.9-4-2SR3-1S', 'loadInformation434', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7945', 'CISCO', 1, 2, 'SCCP45.9-3-1SR1-1S', 'loadInformation435', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7960', 'CISCO', 3, 6, 'P0030801SR02', 'loadInformation7', 1, 'SEP0000000000.cnf.xml_7940_template')",
    "('7961', 'CISCO', 3, 6, 'SCCP41.9-4-2SR3-1S', 'loadInformation30018', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7961G-GE', 'CISCO', 3, 6, 'SCCP41.9-4-2SR3-1S', 'loadInformation308', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7962', 'CISCO', 3, 6, 'SCCP42.9-4-2SR3-1S', 'loadInformation404', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7965', 'CISCO', 3, 6, 'SCCP45.9-3-1SR1-1S', 'loadInformation436', 0, 'SEP0000000000.cnf.xml_796x_template')",
    "('7821', 'CISCO', 1, 1, '', 'loadInformation621', 0, '')",
    "('7841', 'CISCO', 1, 1, '', 'loadInformation622', 0, '')",
    "('7861', 'CISCO', 1, 1, '', 'loadInformation623', 0, '')",
    "('7970', 'CISCO', 3, 8, 'SCCP70.9-4-2SR3-1S', 'loadInformation30006', 0, 'SEP0000000000.cnf.xml_797x_template')",
    "('7971', 'CISCO', 1, 2, 'SCCP70.9-4-2SR3-1S', 'loadInformation119', 0, 'SEP0000000000.cnf.xml_797x_template')",
    "('7975', 'CISCO', 3, 8, 'SCCP75.9-4-2SR3-1S', 'loadInformation437', 0, 'SEP0000000000.cnf.xml_7975_template')",
    "('7985', 'CISCO', 3, 8, 'cmterm_7985.4-1-7-0', 'loadInformation302', 0, NULL)",
    "('8831', 'CISCO', 1, 1, '', 'loadInformation659', 0, '')",
    "('8841', 'CISCO', 1, 1, '', 'loadInformation683', 0, '')",
    "('8851', 'CISCO', 1, 1, '', 'loadInformation684', 0, '')",
    "('8861', 'CISCO', 1, 1, '', 'loadInformation685', 0, '')",
    "('8941', 'CISCO', 1, 4, 'SCCP894x.9-4-2SR1-2', 'loadInformation586', 0, 'SEP0000000000.cnf.xml_797x_template')",
    "('8945', 'CISCO', 1, 4, 'SCCP894x.9-4-2SR1-2', 'loadInformation585', 0, 'SEP0000000000.cnf.xml_7975_template')",
    "('ATA 186', 'CISCO', 1, 1, 'ATA030204SCCP090202A', 'loadInformation12', 0, 'SEP0000000000.cnf.xml_ATA_template')",
    "('ATA 187', 'CISCO', 1, 1, 'ATA187.9-2-3-1', 'loadInformation550', 0, 'SEP0000000000.cnf.xml_ATA_template')",
    "('CN622', 'MOTOROLA', 1, 1, '', 'loadInformation335', 0, NULL)",
    "('Digital Access', 'CISCO', 1, 1, 'D001M022', 'loadInformation40', 0, NULL)",
    "('Digital Access+', 'CISCO', 1, 1, 'D00303010033', 'loadInformation42', 0, NULL)",
    "('E-Series', 'NOKIA', 1, 1, '', '', 0, NULL)",
    "('ICC', 'NOKIA', 1, 1, '', '', 0, NULL)",
    "('Analog Access', 'CISCO', 1, 1, 'A001C030', 'loadInformation30', 0, '')",
    "('WS-X6624', 'CISCO', 1, 1, 'A00204000013', 'loadInformation43', 0, '')",
    "('WS-X6608', 'CISCO', 1, 1, 'C00104000003', 'loadInformation51', 0, '')",
    "('H.323 Phone', 'CISCO', 1, 1, '', 'loadInformation61', 0, '')",
    "('Simulator', 'CISCO', 1, 1, '', 'loadInformation100', 0, '')",
    "('MTP', 'CISCO', 1, 1, '', 'loadInformation111', 0, '')",
    "('MGCP Station', 'CISCO', 1, 1, '', 'loadInformation120', 0, '')",
    "('MGCP Trunk', 'CISCO', 1, 1, '', 'loadInformation121', 0, '')",
    "('UPC', 'CISCO', 1, 1, '', 'loadInformation358', 0, '')",
    "('TelePresence', 'TELEPRESENCE', 1, 1, '', 'loadInformation375', 0, '')"
);

    $test = $db->prepare("SELECT count(*) AS modelCount from sccpdevmodel");
    $test->execute();
    if ($test->fetchAll()[0]['modelCount'] == count($devModelArr)) {
        // Appear to have a correctly populated sccpdevmodel table. Do not overwrite
        // as may contain user modifications;
        outn("<li>" . _("sccpdevmodel appears to be populated; not overwriting") . "</li>");
        return;
    };
    // Update sccpdevmodel as counts do not match
    outn("Updating sccpdevmodel...");
    outn("<li>" . _("Fill sccpdevmodel") . "</li>");
    $sql = "REPLACE INTO sccpdevmodel (model, vendor, dns, buttons, loadimage, loadinformationid, enabled, nametemplate) VALUES" . implode(',',$devModelArr);
    $check = $db->query($sql);
    if (DB::IsError($check)) {
        die_freepbx("Can not create sccpdevmodel table, error:$check\n");
    }
    return;
}

function InstallDB_createButtonConfigTrigger()
{
    global $db;
    outn("<li>" . _("(Re)Create buttonconfig trigger") . "</li>");
    $sql = "DROP TRIGGER IF EXISTS sccp_trg_buttonconfig;";

    $sql .= "CREATE TRIGGER `sccp_trg_buttonconfig` BEFORE INSERT ON `sccpbuttonconfig` FOR EACH ROW BEGIN
        IF NEW.`reftype` = 'sccpdevice' THEN
            IF (SELECT COUNT(*) FROM `sccpdevice` WHERE `sccpdevice`.`name` = NEW.`ref` ) = 0 THEN
                UPDATE `Foreign key contraint violated: ref does not exist in sccpdevice` SET x=1;
            END IF;
        END IF;
        IF NEW.`reftype` = 'sccpline' THEN
            IF (SELECT COUNT(*) FROM `sccpline` WHERE `sccpline`.`name` = NEW.`ref`) = 0 THEN
                UPDATE `Foreign key contraint violated: ref does not exist in sccpline` SET x=1;
            END IF;
        END IF;
        IF NEW.`buttontype` = 'line' THEN
            SET @line_x = SUBSTRING_INDEX(NEW.`name`,'!',1);
            SET @line_x = SUBSTRING_INDEX(@line_x,'@',1);
            IF NEW.`reftype` != 'sipdevice' THEN
                IF (SELECT COUNT(*) FROM `sccpline` WHERE `sccpline`.`name` = @line_x ) = 0 THEN
                    UPDATE `Foreign key contraint violated: line does not exist in sccpline` SET x=1;
                END IF;
            END IF;
        END IF;
        END;";
    $check = $db->query($sql);
    if (DB::IsError($check)) {
        die_freepbx("Can not modify sccpdevice table\n");
    }
    outn("<li>" . _("(Re)Create trigger Ok") . "</li>");
    return true;
}
function InstallDB_updateDBVer($sccp_compatible)
{
    global $db;
    outn("<li>" . _("Update DB Ver") . "</li>");
    $sql = "REPLACE INTO `sccpsettings` (`keyword`, `data`, `seq`, `type`) VALUES ('SccpDBmodel', '". $sccp_compatible. "','30','0');";
    $results = $db->query($sql);
    if (DB::IsError($results)) {
        die_freepbx(sprintf(_("Error updating sccpsettings. Command was: %s; error was: %s "), $sql, $results->getMessage()));
    }
    return true;
}

function InstallDbCreateViews($sccp_compatible)
{
    global $db;
    outn("<li>" . _("(Re)Create sccpdeviceconfig view") . "</li>");
    $sql = "DROP VIEW IF EXISTS sccpdeviceconfig;
            DROP VIEW IF EXISTS sccpuserconfig;
            ";
    ///    global $hw_mobil;
    // From logserver to end only applies to db ver > 433

    global $mobile_hw;
    if ($mobile_hw == '1') {
        $sql .= "CREATE OR REPLACE
            ALGORITHM = MERGE
            VIEW sccpdeviceconfig AS
            SELECT GROUP_CONCAT( CONCAT_WS( ',', sccpbuttonconfig.buttontype, sccpbuttonconfig.name, sccpbuttonconfig.options )
            ORDER BY instance ASC SEPARATOR ';' ) AS sccpbutton, sccpdevice.*
            FROM sccpdevice
            LEFT JOIN sccpbuttonconfig ON (sccpbuttonconfig.reftype = 'sccpdevice' AND sccpbuttonconfig.ref = sccpdevice.name )
            GROUP BY sccpdevice.name; ";
            $sql .=  "CREATE OR REPLACE ALGORITHM = MERGE VIEW sccpuserconfig AS
            SELECT GROUP_CONCAT( CONCAT_WS( ',', sccpbuttonconfig.buttontype, sccpbuttonconfig.name, sccpbuttonconfig.options )
            ORDER BY instance ASC SEPARATOR ';' ) AS button, sccpuser.*
            FROM sccpuser
            LEFT JOIN sccpbuttonconfig ON ( sccpbuttonconfig.reftype = 'sccpuser' AND sccpbuttonconfig.ref = sccpuser.id)
            GROUP BY sccpuser.name; ";
    } else {
        $sql .= "CREATE OR REPLACE
            VIEW sccpdeviceconfig AS
            SELECT CASE sccpdevice.profileid
                WHEN 0 THEN
            (SELECT GROUP_CONCAT(CONCAT_WS(',', defbutton.buttontype, defbutton.name, defbutton.options ) SEPARATOR ';') FROM sccpbuttonconfig AS defbutton WHERE defbutton.ref = sccpdevice.name ORDER BY defbutton.instance )
                WHEN 1 THEN
            (SELECT GROUP_CONCAT(CONCAT_WS(',', userbutton.buttontype, userbutton.name, userbutton.options ) SEPARATOR ';') FROM sccpbuttonconfig AS userbutton WHERE userbutton.ref = sccpdevice.loginname ORDER BY userbutton.instance )
                WHEN 2 THEN
            (SELECT GROUP_CONCAT(CONCAT_WS(',', homebutton.buttontype, homebutton.name, homebutton.options ) SEPARATOR ';') FROM sccpbuttonconfig AS homebutton WHERE homebutton.ref = sccpuser.homedevice  ORDER BY homebutton.instance )
                END
                AS button, if(sccpdevice.profileid = 0, sccpdevice.description, sccpuser.description) AS description, sccpdevice.name, sccpdevice.type,
                sccpdevice.addon, sccpdevice.tzoffset, sccpdevice.imageversion, sccpdevice.deny, sccpdevice.permit, sccpdevice.earlyrtp, sccpdevice.mwilamp,
                sccpdevice.mwioncall, sccpdevice.dndFeature, sccpdevice.transfer, sccpdevice.cfwdall, sccpdevice.cfwdbusy, sccpdevice.private, sccpdevice.privacy,
                sccpdevice.nat, sccpdevice.directrtp, sccpdevice.softkeyset, sccpdevice.audio_tos, sccpdevice.audio_cos, sccpdevice.video_tos, sccpdevice.video_cos,
                sccpdevice.conf_allow, sccpdevice.conf_play_general_announce, sccpdevice.conf_play_part_announce, sccpdevice.conf_mute_on_entry,
                sccpdevice.conf_music_on_hold_class, sccpdevice.conf_show_conflist, sccpdevice.force_dtmfmode, sccpdevice.setvar, sccpdevice.backgroundImage,
                sccpdevice.backgroundThumbnail, sccpdevice.ringtone, sccpdevice.callhistory_answered_elsewhere, sccpdevice.useRedialMenu, sccpdevice.cfwdnoanswer,
                sccpdevice.park, sccpdevice.monitor, sccpdevice.phonecodepage, sccpdevice.keepalive
            FROM sccpdevice
            LEFT JOIN sccpuser sccpuser ON ( sccpuser.name = sccpdevice.loginname )
            GROUP BY sccpdevice.name;";
    }
    $stmt = $db->prepare($sql);
    $result = $stmt->execute();
    if (!$result) {
        die_freepbx(sprintf(_("Error updating sccpdeviceconfig view. Command was: %s"), $sql));
    }

    outn("<li>" . _("(Re)Create sccplineconfig view") . "</li>");

    $sql = "DROP VIEW IF EXISTS sccplineconfig;
            ";
    $sql .= "CREATE OR REPLACE
            VIEW sccplineconfig AS
            SELECT sccpline.name, sccpline.id, sccpline.pin ,sccpline.label, sccpline.description, sccpline.context, sccpline.incominglimit, sccpline.transfer, sccpline.mailbox,
                sccpline.vmnum, sccpline.cid_name, sccpline.cid_num, sccpline.disallow, sccpline.allow, sccpline.trnsfvm, sccpline.secondary_dialtone_digits,
                sccpline.secondary_dialtone_tone, sccpline.musicclass, sccpline.language, sccpline.accountcode, sccpline.echocancel, sccpline.silencesuppression,
                sccpline.callgroup, sccpline.pickupgroup, sccpline.adhocNumber, sccpline.meetme, sccpline.meetmenum, sccpline.meetmeopts, sccpline.regexten,
                sccpline.directed_pickup, sccpline.directed_pickup_context, sccpline.pickup_modeanswer, sccpline.amaflags, sccpline.dnd, sccpline.setvar,
                sccpline.namedcallgroup, sccpline.namedpickupgroup, sccpline.phonecodepage, sccpline.videomode
                FROM sccpline";
    $stmt = $db->prepare($sql);
    $result = $stmt->execute();
    if (!$result) {
        die_freepbx(sprintf(_("Error updating sccplineconfig view. Command was: %s"), $sql));
    }
    return true;
}

function installDbPopulateSccpline() {
    // Lines in Sccp_manager are devices in FreePbx. Need to ensure that these two tables are synchronised on install
    global $db;
    $freePbxExts = array ();
    $sccpExts =array();
    $sql = "SELECT id AS name, user AS accountcode, description AS label FROM devices WHERE tech='sccp'";
    $stmt = $db->prepare($sql);
    $stmt->execute();
    $freePbxExts = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);

    $sql = "SELECT name, accountcode, label FROM sccpline";
    $stmt = $db->prepare($sql);
    $stmt->execute();
     $sccpExts = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
    
    // PHP 8.2 Fix: Сравниваем ключи вместо массивов, чтобы избежать Array to string conversion
    $linesToCreateKeys = array_diff(array_keys($freePbxExts), array_keys($sccpExts));
    
    foreach ($linesToCreateKeys as $key) {
        $valArr = $freePbxExts[$key];
        $stmt = $db->prepare("INSERT into sccpline (name, accountcode, description, label) VALUES (:name, :accountcode, :description, :label)");
        $stmt->bindParam(':name',$key,\PDO::PARAM_STR);
        $description = "{$valArr['label']} <{$key}>";
        $stmt->bindParam(':accountcode',$valArr['accountcode'],\PDO::PARAM_STR);
        $stmt->bindParam(':description',$description,\PDO::PARAM_STR);
        $stmt->bindParam(':label',$valArr['label'],\PDO::PARAM_STR);
        $stmt->execute();
        if (DB::IsError($stmt)) {
            $err = $stmt->errorInfo();
die_freepbx(_("Error inserting into sccpline. Error was: ") . ($err[2] ?? 'Unknown Error'));
        }
    }
}

function createBackUpConfig()
{
    global $amp_conf;
    global $cnf_int;
    outn("<li>" . _("Creating Config BackUp") . "</li>");
    $backup_files = array('extensions','extconfig','res_mysql', 'res_config_mysql','sccp','sccp_hardware','sccp_extensions');
    $backup_ext = array('_custom.conf', '_additional.conf','.conf');
    $dir = $cnf_int->get('ASTETCDIR');

    $fsql = $dir.'/sccp_backup_'.date("Ymd").'.sql';
    $result = exec('mysqldump '.$amp_conf['AMPDBNAME'].' --password='.$amp_conf['AMPDBPASS'].' --user='.$amp_conf['AMPDBUSER'].' --single-transaction >'.$fsql);

    try {
        $zip = new \ZipArchive();
    } catch (\Exception $e) {
        outn("<br>");
        outn("<font color='red'>PHPx.x-zip not installed where x.x is the installed PHP version. Install it before continuing !</font>");
        die_freepbx();
    }
    $filename = $dir . "/sccp_install_backup" . date("Ymdhis"). ".zip";
    if ($zip->open($filename, \ZIPARCHIVE::CREATE)) {
        foreach ($backup_files as $file) {
            foreach ($backup_ext as $b_ext) {
                if (file_exists($dir . '/'.$file . $b_ext)) {
                    $zip->addFile($dir . '/'.$file . $b_ext);
                }
            }
        }
        if (file_exists($fsql)) {
            $zip->addFile($fsql);
        }
        $zip->close();
    } else {
        outn("<li>" . _("Error Creating BackUp: ") . $filename ."</li>");
    }
    unlink($fsql);
    outn("<li>" . _("Config backup created: ") . $filename ."</li>");
}

function RenameConfig()
{
    outn("<li>" . _("Move Old Config") . "</li>");
    global $cnf_int;
    $rename_files = array('sccp_hardware','sccp_extensions');
    $rename_ext = array('_custom.conf', '_additional.conf','.conf');
    $dir = $cnf_int->get('ASTETCDIR');
    foreach ($rename_files as $file) {
        foreach ($rename_ext as $b_ext) {
            if (file_exists($dir . '/'.$file . $b_ext)) {
                rename($dir . '/'.$file . $b_ext, $dir . '/'.$file . $b_ext.'.old');
            }
        }
    }
}

function Setup_RealTime()
{
    outn("<li>" . _("Checking realtime configuration ...") . "</li>");
    global $amp_conf;
    global $cnf_int;
    $cnf_wr = \FreePBX::WriteConfig();
    $cnf_read = \FreePBX::LoadConfig();

    // Define required default settings based on FreePBX and system settings
    $dir = $cnf_int->get('ASTETCDIR');
    $sys_mysql_socket = ini_get('pdo_mysql.default_socket');
    $def_bd_config = array('dbhost' => $amp_conf['AMPDBHOST'],
                            'dbname' => $amp_conf['AMPDBNAME'],
                            'dbuser' => $amp_conf['AMPDBUSER'],
                            'dbpass' => $amp_conf['AMPDBPASS'],
                            'dbport' => '3306',
                            'dbsock' => '/var/lib/mysql/mysql.sock',
                            'dbcharset'=>'utf8'
                          );
    if (!empty($sys_mysql_socket)) {
        if (file_exists($sys_mysql_socket)) {
            $def_bd_config['dbsock'] = $sys_mysql_socket;
        }
    }
    $def_bd_section = $amp_conf['AMPDBNAME'];
    $def_ext_config = array('sccpdevice' => "mysql,{$def_bd_section},sccpdeviceconfig",'sccpline' => "mysql,{$def_bd_section},sccplineconfig");

    // Check extconfig file for correct connector values
    $ext_conf = '';
    $ext_conf_file = '';
    $backup_ext = array('_custom.conf', '_additional.conf','.conf');
    foreach ($backup_ext as $value) {
        if (file_exists($dir . '/extconfig' . $value)) {
            // Last possibility is normal file extconfig.conf
            $ext_conf_file = 'extconfig' . $value;
            $ext_conf = $cnf_read->getConfig($ext_conf_file);
            break;
        }
    }
    if (empty($ext_conf_file)) {
        // Have not found a file, so will need to create. $ext_conf must be empty
        $ext_conf_file = 'extconfig.conf';
    }

    if (!empty($ext_conf)) {
        // Have found a file and read a config. Now need to check required settings
        $currentExtSettings = array();
        $writeExtSettings = $ext_conf;
        if (empty($ext_conf['settings']['sccpdevice']) || ($ext_conf['settings']['sccpdevice'] !== $def_ext_config['sccpdevice'])) {
            // Have error in sccpdevice so need to correct
            $writeExtSettings['settings']['sccpdevice'] = $def_ext_config['sccpdevice'];
        }
        if (empty($ext_conf['settings']['sccpline']) || ($ext_conf['settings']['sccpline'] !== $def_ext_config['sccpline'])) {
            // Have error in sccpline so need to correct
            $writeExtSettings['settings']['sccpline'] = $def_ext_config['sccpline'];
        }
        if (!empty($writeExtSettings)) {
            outn("<li>" . _("Updating extconfig file ...  ") . $ext_conf_file . "</li>");
            $cnf_wr->writeConfig($ext_conf_file, $writeExtSettings);
        }
    } else {
        // Either did not find file or file did not contain any config, so create and fill
        outn("<li>" . _("Creating extconfig file ...  ") . $ext_conf_file . "</li>");
        $writeExtSettings['settings']['sccpdevice'] = $def_ext_config['sccpdevice'];
        $writeExtSettings['settings']['sccpline'] = $def_ext_config['sccpline'];
        $cnf_wr->writeConfig($ext_conf_file, $writeExtSettings);
    }

    // Check database settings
    $res_conf = array();
    if (file_exists($dir . '/res_mysql.conf')) {
        $res_conf = $cnf_read->getConfig('res_mysql.conf');
        if (empty($res_conf[$def_bd_section])) {
            $res_conf[$def_bd_section] = $def_bd_config;
            $cnf_wr->writeConfig('res_mysql.conf', $res_conf);
            outn("<li>" . _("Updating res_mysql.conf file ...") . "</li>");
        }
    } elseif (file_exists($dir . '/res_config_mysql.conf')) {
        $res_conf = $cnf_read->getConfig('res_config_mysql.conf');
        if (empty($res_conf[$def_bd_section])) {
            $res_conf[$def_bd_section] = $def_bd_config;
            $cnf_wr->writeConfig('res_config_mysql.conf', $res_conf);
            outn("<li>" . _("Updating res_config_mysql.conf file ...") . "</li>");
        }
    } else {
        // Have not found either res_mysql.conf or res_config_mysql.config
        // So create the latter
        $res_conf[$def_bd_section] = $def_bd_config;
        $cnf_wr->writeConfig('res_config_mysql.conf', $res_conf, false);
    }
}

function addDriver($sccp_compatible) {
    global $amp_conf;
    global $cnf_int;
    outn("<li>" . _("Adding driver ...") . "</li>");
    $file = $amp_conf['AMPWEBROOT'] . '/admin/modules/core/functions.inc/drivers/Sccp.class.php';
    $contents = "<?php include '/var/www/html/admin/modules/sccp_manager/sccpManClasses/Sccp.class.php.v{$sccp_compatible}'; ?>";
    file_put_contents($file, $contents);
}
function checkTftpServer() {
    outn("<li>" . _("Checking TFTP server path and availability ...") . "</li>");
    global $db;
    global $cnf_int;
    global $settingsFromDb;
    global $extconfigs;
    global $thisInstaller;
    global $amp_conf;
    $confDir = $cnf_int->get('ASTETCDIR');
    $tftpRootPath = "";
    // put the rewrite rules into the required location
    if (file_exists("{$confDir}/sccpManagerRewrite.rules")) {
        rename("{$confDir}/sccpManagerRewrite.rules", "{$confDir}/sccpManagerRewrite.rules.bu");
    }
    copy($amp_conf['AMPWEBROOT'] . '/admin/modules/sccp_manager/conf/mappingRulesHeader',"{$confDir}/sccpManagerRewrite.rules");
    file_put_contents("{$confDir}/sccpManagerRewrite.rules", file_get_contents($amp_conf['AMPWEBROOT'] . '/admin/modules/sccp_manager/contrib/rewrite.rules'), FILE_APPEND);
    file_put_contents("{$confDir}/sccpManagerRewrite.rules", "\n# Do not disable this rule - this is required by sccp_manager\nri ^(.+\.tlzz)?$ settings/\\1", FILE_APPEND);
    // TODO: add option to use external server
    $remoteFileName = ".sccp_manager_installer_probe_sentinel_temp".mt_rand(0, 9999999);
    $remoteFileContent = "# This is a test file created by Sccp_Manager. It can be deleted without impact";
    $possibleFtpDirs = array('/srv', '/srv/tftp','/var/lib/tftp', '/tftpboot');

    // write a couple of sentinels to different distro tftp locations in the filesystem
    // TODO: Depending on distro, do we have write permissions
    foreach ($possibleFtpDirs as $dirToTest) {
        if (is_dir($dirToTest) && is_writable($dirToTest)) {
            $tempFile = "{$dirToTest}/{$remoteFileName}";
            file_put_contents($tempFile, $remoteFileContent);

            // try to pull the written file through tftp.
            // this way we can determine if tftp server is active, and what it's
            // source directory is.
            if ($remoteFileContent == $thisInstaller->tftpReadTestFile($remoteFileName)) {
                $tftpRootPath = $dirToTest;
                outn("<li>" . _("Found ftp root dir at {$tftpRootPath}") . "</li>");
                if ($settingsFromDb['tftp_path']['data'] != $tftpRootPath) {
                    $settingsFromDb["tftp_path"] = array( 'keyword' => 'tftp_path', 'seq' => 2, 'type' => 0, 'data' => $tftpRootPath, 'systemdefault' => '');
                }
                // Found sentinel file. Remove it and exit loop
                if (file_exists($tempFile)) {
                    unlink($tempFile);
                }
                break;
            }
            // Did not find sentinel so remove and continue
            if (file_exists($tempFile)) {
                unlink($tempFile);
            }
        }
    }
    if (empty($tftpRootPath)) {
        die_freepbx(_("Either TFTP server is down or TFTP root is non standard. Please fix, refresh, and try again"));
    }

    $settingsFromDb['asterisk_etc_path'] =  array( 'keyword' => 'asterisk_etc_path', 'seq' => 20, 'type' => 0, 'data' => $confDir, 'systemdefault' => '');

    // Get TFTP mapping Status
    $settingsFromDb['tftp_rewrite'] = array('keyword' => 'tftp_rewrite', 'seq' => 20, 'type' => 0, 'data' => 'off', 'systemdefault' => '');
    if ($thisInstaller->checkTftpMapping()) {
        $settingsFromDb['tftp_rewrite']['data'] = 'pro';
    }

    // Populate TFTP paths in SccpSettings
    $settingsFromDb = $extconfigs->updateTftpStructure($settingsFromDb);

    foreach ($settingsFromDb as $settingToSave) {
        $sql = "REPLACE INTO sccpsettings (keyword, data, seq, type, systemdefault) VALUES ('{$settingToSave['keyword']}', '{$settingToSave['data']}', {$settingToSave['seq']}, {$settingToSave['type']}, '{$settingToSave['systemdefault']}')";
        $results = $db->query($sql);
        if (DB::IsError($results)) {
            die_freepbx(_("Error updating sccpsettings. $sql"));
        }
    }
    getMasterFileList($tftpRootPath);
    return;
}

function getMasterFileList(string $tftpRootPath) {
    global $thisInstaller;
    global $amp_conf;
    outn("<li>" . _("Checking TFTP server path and availability ...") . "</li>");
    if (file_exists("{$tftpRootPath}/masterFilesStructure.xml")) {
        outn("<li>" . _("Backing up existing masterFilesList ...") . "</li>");
        rename("{$tftpRootPath}/masterFilesStructure.xml","{$tftpRootPath}/masterFilesStructure.xml.old");
    }
    outn("<li>" . _("Getting latest master file list from provisioner ...") . "</li>");
    if (!$thisInstaller->getFileListFromProvisioner($tftpRootPath)) {
        outn("<li>" . _("Unable to fetch master file list from provisioner, installing local copy ...") . "</li>");
        // Cannot get file from internet, so use copy with this dist which may be older.
        if (!copy($amp_conf['AMPWEBROOT'] . '/admin/modules/sccp_manager/contrib/masterFilesStructure.xml',"{$tftpRootPath}/masterFilesStructure.xml")) {
            return false;
        };
        return true;
    };
}

function cleanUpSccpSettings() {
    global $thisInstaller;
    global $settingsFromDb;
    global $db;
    global $aminterface;
    global $sccp_compatible;
    global $amp_conf;
    global $cnf_int;

    // Get current default settings from db
    $stmt = $db->prepare("SELECT keyword, sccpsettings.* FROM sccpsettings");
    $stmt->execute();
    $settingsFromDb = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);

    // See if a previous version was installed
    outn("<li>" . _("Checking for previous version of Sccp_manager.") . "</li>");
    if (!isset($settingsFromDb['sccp_compatible']['data'])) {
        outn(_("No previous version found "));
    } else {
    outn(_("Found DB Schema : {$settingsFromDb['sccp_compatible']['data']}"));
    }
    // Check that required settings are initialised and update db and $settingsFromDb if not
    // Clean up sccpsettings to remove legacy values.
    $xml_vars = $amp_conf['AMPWEBROOT'] . "/admin/modules/sccp_manager/conf/sccpgeneral.xml.v{$sccp_compatible}";
    $thisInstaller->xml_data = simplexml_load_file($xml_vars);
    $thisInstaller->initVarfromXml();
    foreach ( array_diff_key($settingsFromDb,$thisInstaller->sccpvalues) as $key => $valueArray) {
        // Remove legacy values
        unset($settingsFromDb[$key]);
    }
    foreach ($settingsFromDb as $key => $valueArray) {
        $settingsFromDb[$key]['seq'] = $thisInstaller->sccpvalues[$key]['seq'];
        $settingsFromDb[$key]['type'] = $thisInstaller->sccpvalues[$key]['type'];
    }
    $settingsFromDb = array_merge($settingsFromDb, array_diff_key($thisInstaller->sccpvalues, $settingsFromDb));
    unset($thisInstaller->sccpvalues);

    // get chan-sccp defaults

    foreach (array('general','device','line') as $section) {
        $sysConfig = $aminterface->getSCCPConfigMetaData($section);
        foreach ($sysConfig['Options'] as $valueArray) {
            if (isset($valueArray['Flags'])) {
                if ($valueArray['Flags'][0] == 'Obsolete' || $valueArray['Flags'][0] == 'Deprecated') {
                    continue;
                }
            }
            if (isset($sysConfiguration[$valueArray['Name']])) {
                continue;
            }
            $sysConfiguration[$valueArray['Name']] = $valueArray;
        }
    }
    unset($sysConfig);
    foreach ($sysConfiguration as $key => $valueArray) {

        // 2 special cases deny|permit & disallow|allow where need to parse on |.
        $newKeyword = explode("|", $key, 2);
        if (isset($newKeyword[1])) {
            // chan-sccp sets sysdef as comma separated list for sccp.conf, but expects ; separated list
            // when returned from db
            $newSysDef = explode("|", $valueArray['DefaultValue'], 2);
            $newSysDef = str_replace(',',';', $newSysDef);
            $i = 0;
            foreach ($newKeyword as $dummy) {
                if (array_key_exists($newKeyword[$i],$settingsFromDb)) {
                    if (!empty($newSysDef[$i])) {
                        $settingsFromDb[$newKeyword[$i]]['systemdefault'] = $newSysDef[$i];
                    }
                } else {
                    $settingsFromDb[$newKeyword[$i]] = array('keyword' => $newKeyword[$i], 'seq' => 0, 'type' => 0, 'data' => '', 'systemdefault' => $newSysDef[$i]);
                }
                $i++;
            }
            if (array_key_exists($key, $settingsFromDb)){
                unset($settingsFromDb[$key]);
            }
        } else {
            $sysConfiguration[$key]['DefaultValue'] = ($sysConfiguration[$key]['DefaultValue'] == '(null)') ? '' : $sysConfiguration[$key]['DefaultValue'];
            if (array_key_exists($key,$settingsFromDb)) {
                // Preserve sequence and type
                $settingsFromDb[$key]['systemdefault'] = $sysConfiguration[$key]['DefaultValue'];
            } else {
                $settingsFromDb[$key] = array('keyword' => $key, 'seq' => 0, 'type' => 0, 'data' => '', 'systemdefault' => $sysConfiguration[$key]['DefaultValue']);
            }
        }
        // Override certain chan-sccp defaults as they are based on a non-FreePbx system
        $settingsFromDb['context']['systemdefault'] = 'from-internal';
        $settingsFromDb['directed_pickup']['systemdefault'] = 'no';
        // Override this chan-sccp default as it is a potential security risk. See Issue 29
        $settingsFromDb['hotline_enabled']['systemdefault'] = 'no';

        unset($sysConfiguration[$key]);
    }
    unset($sysConfiguration);

    // Update enums in sccpsettings - values have changed over versions so need to update values in db that are not compliant
    outn("<li>" . _("Updating invalid enums in sccpsettings") . "</li>");
    $rowsToTest = array();
    $tablesToDescribe = array('sccpline', 'sccpdevice');
    foreach ($tablesToDescribe as $theTable) {
        $stmt = $db->prepare("DESCRIBE {$theTable}");
        $stmt->execute();
        $tableDesc = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        foreach ($tableDesc as $key => $valArr) {
            if (strpos($valArr['Type'], 'enum') !== 0) {
                continue;
            }
            $rowsToTest[$key] = explode(',',rtrim(str_replace('enum(', '',$valArr['Type']), ')'));
        }
    }
    $count = 0;
    foreach ($rowsToTest as $key => $valArr) {
        if (empty($settingsFromDb[$key]['data'])) {
            continue;
        }
        if (in_array("'{$settingsFromDb[$key]['data']}'", $valArr, true)) {
            continue;
        }
        // clear site setting so that will return to system defaults.
        // Try to convert based on change from on/off to yes/no.
        if (in_array($settingsFromDb[$key]['data'], array('on','off'), true)) {
            if (in_array("'yes'", $valArr, true)) {
                $settingsFromDb[$key]['data'] = ($settingsFromDb[$key]['data'] = 'on') ? 'yes' : 'no';
                continue;
            }
        }
        // Test for case
        if (in_array("'" . strtolower($settingsFromDb[$key]['data']) . "'", $valArr, true)) {
            $settingsFromDb[$key]['data'] = strtolower($settingsFromDb[$key]['data']);
            continue;
        }
        // No easy choices so reset to system default
        $settingsFromDb[$key]['data'] = '';
        $count++;
    }

    // Write settings back to db
    $sql = "TRUNCATE sccpsettings";
    $results = $db->query($sql);
    foreach ( $settingsFromDb as $key =>$valueArray ) {
        $sql = "REPLACE INTO sccpsettings
                (keyword, seq, type, data, systemdefault)
                    VALUES
                ( '{$settingsFromDb[$key]['keyword']}',
                  {$settingsFromDb[$key]['seq']},
                  {$settingsFromDb[$key]['type']},
                  '{$settingsFromDb[$key]['data']}',
                  '{$settingsFromDb[$key]['systemdefault']}'
                )";
        $results = $db->query($sql);
    }
    // Need to load any existing sccp.conf so that retain softkeys section if exists.
    $sccp_conf_init = $thisInstaller->initialiseConfInit();
    // Now correct sccp.conf to replace any illegal settings passing $sccp_conf_init
    $thisInstaller->createDefaultSccpConfig($settingsFromDb, $cnf_int->get('ASTETCDIR'), $sccp_conf_init);

    // have to correct prior verion sccpline lists for allow/disallow and deny permit. Prior
    // versions used csl, but chan-sccp expects ; separated lists when returned by db.

    outn("<li>" . _("Replacing invalid values in sccpline") . "</li>");
    $db->query("UPDATE sccpline SET allow = REPLACE(allow, ',',';') WHERE allow like '%,%'");
    $db->query("UPDATE sccpline SET disallow = REPLACE(disallow, ',',';') WHERE disallow like '%,%'");

    // Ensure that disallow is set to all if unset (and not NULL)
    $db->query("UPDATE sccpline SET disallow = 'all' WHERE disallow like ''");

}
?>
