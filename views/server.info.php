<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

$test_ami = 0;
$test_any = 0;
global $amp_conf;

$driver = $this->FreePBX->Core->getAllDriversInfo();
$core = $this->aminterface->getSCCPVersion();
$ast_realtime = $this->aminterface->getRealTimeStatus();

//$ast_realm = (empty($ast_realtime['sccp']) ? '' : 'sccp');

// if there are multiple connections, this will only return the first.
foreach ($ast_realtime as $key => $value) {
    if (empty($ast_realm)) {
        if ($value['status'] === 'OK') {
            $ast_realm = $key;
        }
    }
}

$conf_realtime = $this->extconfigs->validate_RealTime($ast_realm);
$db_Schema = $this->dbinterface->validate();
$mysql_info = $this->dbinterface->get_db_sysvalues();
$compatible = $this->aminterface->get_compatible_sccp();
$info = array();

//$info['srvinterface'] = $this->srvinterface->info();
$moduleXml = simplexml_load_file("{$amp_conf['AMPWEBROOT']}/admin/modules/sccp_manager/module.xml");
$info['extconfigs'] = $this->extconfigs->info();
$info['dbinterface'] = $this->dbinterface->info();
$info['aminterface'] = $this->aminterface->info();
$info['XML'] = $this->xmlinterface->info();
$info['sccp_class'] = $driver['sccp'];
$info['Core_sccp'] = array('Version' => $core['Version'],
                            'about' => "Sccp ver: {$core['Version']}   r{$core['vCode']}   Revision: {$core['RevisionNum']}   Hash: {$core['RevisionHash']}");
$capabilityArray = array( "park", "pickup", "realtime", "video", "conference", "dirtrfr", "feature_monitor", "functions", "manager_events",
                          "devicestate", "devstate_feature", "dynamic_speeddial", "dynamic_speeddial_cid", "experimental", "debug");

$info['chan-sccp build info'] = array('Version' => $core['Version'], 'about' => 'Following options NOT built:  ' . implode('; ',array_diff($capabilityArray, $core['buildInfo'])));
$info['Asterisk'] = array('Version' => FreePBX::Config()->get('ASTVERSION'), 'about' => 'Asterisk.');

if (!empty($this->sccpvalues['SccpDBmodel'])) {
    $info['DB Model'] = array('Version' => $this->sccpvalues['SccpDBmodel']['data'], 'about' => 'SCCP DB Configure');
}

exec('in.tftpd -V', $tftpInfo);
$info['TFTP Server'] = array('Version' => 'Not Found', 'about' => 'Mapping not available');

if (isset($tftpInfo[0])) {
    $tftpInfo = explode(',',$tftpInfo[0]);
    $info['TFTP Server'] = array('Version' => $tftpInfo[0], 'about' => 'Mapping not available');
    $tftpInfo[1] = trim($tftpInfo[1]);
    if ($tftpInfo[1] == 'with remap') {
        $info['TFTP Server'] = array('Version' => $tftpInfo[0], 'about' => $tftpInfo[1]);
    }
}

if (!empty($this->sccpvalues['tftp_rewrite']['data'])) {
    switch ($this->sccpvalues['tftp_rewrite']['data']) {
      case 'custom':
      case 'pro':
          $info['Provision_SCCP'] = array('Version' => 'base', 'about' => 'Provision Sccp enabled');
          break;
      default:
          if ($tftpInfo[1] == 'with remap') {
              $info['TFTP_Mapping'] = array('Version' => 'off', 'about' => "TFTP mapping is available but the mapping file is not included in tftpd-hpa default settings.<br>
                                            To enable Provision mode, add option <br>
                                            -m /etc/asterisk/sccpManagerRewrite.rules <br>
                                            to the tftpd defaults, (location dependant on the system), and restart the tftpd server");

          } else {
              $info['TFTP_Mapping'] = array('Version' => 'off', 'about' => "Mapping capability is not built into the TFTP server. To enable Provision, upgrade the TFTP server.");
          }
          break;
    }
}

// Finished testing tftp server options
$info['Сompatible'] = array('Version' => $compatible, 'about' => 'Ok');
if (!empty($this->sccpvalues['SccpDBmodel'])) {
    if ($compatible > $this->sccpvalues['SccpDBmodel']['data']) {
        $info['Сompatible']['about'] = '<div class="alert signature alert-danger"> Reinstall SCCP manager required</div>';
    }
}
if ($db_Schema == 0) {
    $info['DB_Schema'] = array('Version' => 'Error', 'about' => '<div class="alert signature alert-danger"> ERROR DB Version </div>');
} else {
    $info['DB_Schema'] = array('Version' => $db_Schema, 'about' => (($compatible == $db_Schema ) ? 'Ok' : 'Incompatible Version'));
}

if (empty($ast_realtime)) {
    $info['RealTime'] = array('Version' => 'Error', 'about' => '<div class="alert signature alert-danger"> No RealTime connections found</div>');
} else {
    $rt_info = '';
    $rt_sccp = 'Failed';
    foreach ($ast_realtime as $key => $value) {
        if ($key == $ast_realm) {
            if ($value['status'] == 'OK') {
                $rt_sccp = 'TEST OK';
                $rt_info .= '<div> Using SCCP connection found to database: '.$value['realm'] . ' with connector: ['. $key .']</div>';
            } else {
                $rt_sccp = 'SCCP ERROR';
                $rt_info .= '<div class="alert signature alert-danger"> Error : ' . $value['message'] . '</div>';
            }
        } elseif ($value['status'] == 'ERROR') {
            $rt_info .= '<div> No connector found for [' . $key . '] : ' . $value['message'] . '</div>';
        } elseif ($value['status'] == 'OK') {
            $rt_info .= '<div> Alternative connector found to database '.$value['realm'] . ' with connector: ['. $key . '] </div>';
        }
    }
    $info['RealTime'] = array('Version' => $rt_sccp, 'about' => $rt_info);
}
// There are potential issues with string Type Declarations in PHP 5.
$info['PHP'] = array('Version' => phpversion(), 'about' => version_compare(phpversion(), '7.0.0', '>' ) ? 'OK' : 'PHP 7 Preferred - Please upgrade if possible');
$mariaDbInfo = exec('mysql -V');
$info['MariaDb'] = array('Version' => explode(" ",$mariaDbInfo)[3], 'about' => $mariaDbInfo);

if (empty($conf_realtime)) {
    $info['ConfigsRealTime'] = array('Version' => 'Error', 'about' => '<div class="alert signature alert-danger"> Realtime configuration was not found</div>');
} else {
    $rt_info = '';
    foreach ($conf_realtime as $key => $value) {
        if (($value != 'OK') && ($key != 'extconfigfile')) {
            $rt_info .= '<div> Found error in section ' . $key . ' :' . $value . '</div>';
        }
    }
    if (!empty($rt_info)) {
        $info['ConfigsRealTime'] = array('Version' => 'Error', 'about' => $rt_info);
    }
}
// $mysql_info
if ($mysql_info['Value'] <= '2000') {
    $this->info_warning['MySql'] = array('Increase Mysql Group Concat Max. Length', 'Step 1: Go to mysql path <br> nano /etc/my.cnf',
        'Step 2: And add the following line below [mysqld] as shown below <br> [mysqld] <br>group_concat_max_len = 4096 or more',
        'Step 3: Save and restart <br> systemctl restart mariadb.service<br> Or <br> service mysqld restart');
}


// Check Time Zone compatibility
$conf_tz = $this->sccpvalues['ntp_timezone']['data'];
$cisco_tz = $this->extconfigs->getExtConfig('sccp_timezone', $conf_tz);
if ($cisco_tz['offset'] == 0) {
    if (!empty($conf_tz)) {
        $tmp_dt = new DateTime(null, new DateTimeZone($conf_tz));
        $tmp_ofset = $tmp_dt->getOffset();
        if (($cisco_tz['offset'] != ($tmp_ofset / 60) )) {
            $this->info_warning['NTP'] = array('The selected NTP time zone is not supported by cisco devices.', 'We will use the Greenwich Time zone');
        }
    }
}

if (!empty($this->info_warning)) {
    ?>
    <div class="fpbx-container container-fluid">
        <div class="row">
            <div class="container">
                <h2 style="border:2px solid Tomato;color:Tomato;" >Sccp Manager Warning</h2>
                <div class="table-responsive">
                    <br> There are Warning in the SCCP Module:<br><pre>
                        <?php
                        foreach ($this->info_warning as $key => $value) {
                            echo '<h3>' . $key . '</h3>';
                            if (is_array($value)) {
                                echo '<li>' . _(implode('</li><li>', $value)) . '</li>';
                            } else {
                                echo '<li>' . _($value) . '</li>';
                            }
                            echo '<br>';
                        }
                        ?>
                    </pre>
                    <br><h4 style="border:2px solid Tomato;color:Green;" > Check these problems before continuing to work.</h4> <br>
                </div>
            </div>
        </div>
    </div>
    <br>
    <?php
}

if (!empty($this->class_error)) {
    ?>
    <div class="fpbx-container container-fluid">
        <div class="row">
            <div class="container">
                <h2 style="border:2px solid Tomato;color:Tomato;" >Diagnostic information about SCCP Manager errors</h2>
                <div class="table-responsive">
                    <br> There is an error in the :<br><pre>
    <?php print_r($this->class_error); ?>
                    </pre>
                    <br> Correct these problems before continuing to work. <br>
                    <br><h3 style="border:2px solid Tomato;color:Green;" > Open 'SCCP Connectivity' -> Server Config' to change global settings</h3> <br>
                </div>
            </div>
        </div>
    </div>
    <br>
<?php } ?>
<div class="fpbx-container container-fluid">
    <div class="row">
        <div class="container">
            <h2>Sccp Manager v<?php print_r((string) $moduleXml->version); ?> Info </h2>
            <div class="table-responsive">
                <table class="table">
                    <thead>
                        <tr>
                            <th>Module</th>
                            <th>Version</th>
                            <th>Info</th>
                        </tr>
                    </thead>
                    <tbody>
<?php
foreach ($info as $key => $value) {
    echo '<tr><td>' . $key . '</td><td>' . $value['Version'] . '</td><td>' . $value['about'] . '</td></tr>';
}
?>
                    </tbody>
                </table>
            </div>
            <a class="btn btn-default" href="ajax.php?module=sccp_manager&command=backupsettings"><i class="fa fa-plane">&nbsp;</i><?php echo _("BackUp Config") ?></a>
        </div>

    </div>
</div>
<?php echo $this->showGroup('sccp_info', 0); ?>
