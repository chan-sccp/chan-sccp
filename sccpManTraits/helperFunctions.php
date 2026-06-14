<?php

namespace FreePBX\modules\sccp_manager\sccpManTraits;

trait helperfunctions {
    private function convertCsvToArray($stringToConvert = "") {
        // Take a csv string form of net/mask or ip/port and convert to an array
        // sub arrays are separated by ";"
        $outputArr = array();
        if (empty($stringToConvert)) {
            return $outputArr;
        }
        foreach (explode(";", $stringToConvert) as $value) {
            //internal is always the first setting if present
            if ($value == 'internal') {
                $outputArr[] = array('internal' => 'on');
                break;
            }
            // Now handle rest of value types
            $subArr = explode("/", $value);
            if (count($subArr) === 2) {
                // Have net/mask
                $outputArr[] = array('net' => $subArr[0], 'mask' => $subArr[1]);
            } else {
                // have ip:port
                $subArr = explode(":", $value);
                $outputArr[] = array('ip' => $subArr[0], 'port' => $subArr[1]);
            }
        }
        return $outputArr;
    }

    private function convertArrayToCsv(array $arrayToConvert) {
        // About to save to db so need to convert to string
        // Take an array form of net mask or ip port and convert to a csv
        // sub arrays are separated by ";"
        if (empty($arrayToConvert)) {
            return '';
        }
        $output = array();
        // Internal is always element 0, nets and ips start at element 1.
        if ((isset($arrayToConvert[1]['net'])) || (isset($arrayToConvert[0]['internal']))) {
            // Have net masks
            foreach ($arrayToConvert as $netValue) {
                if (isset($netValue['internal'])) {
                    $output[] = 'internal';
                    break;
                }
                if (empty($netValue['net'])) {
                    // If network not set, user error, has added empty row so delete
                    break;
                }
                // If the mask has not been set, set to this subnet
                $netValue['mask'] = (empty($netValue['mask'])) ? "255.255.255.0" : $netValue['mask'];
                $output[] = implode('/', $netValue);
            }
        } else {
            // Have ip addresses
            foreach ($arrayToConvert as $ipArr) {
                if (isset($ipArr['internal'])) {
                    // should not be set for an ip address
                    break;
                }
                if (empty($ipArr['ip'])) {
                    // If ip not set, user error, has added empty row so delete
                    break;
                }
                $ipArr['port'] = (empty($ipArr['port'])) ? "2000" : $ipArr['port'];
                $output[] = implode(':', $ipArr);
            }
        }
        return implode(';', $output);
    }

    private function getIpInformation($type = '') {
        $interfaces = array();
        switch ($type) {
            case 'ip4':
                exec("/sbin/ip -4 -o addr", $result, $ret);
                break;
            case 'ip6':
                exec("/sbin/ip -6 -o addr", $result, $ret);
                break;

            default:
                exec("/sbin/ip -o addr", $result, $ret);
                break;
        }
        foreach ($result as $line) {
            $vals = preg_split("/\s+/", $line);
            if ($vals[3] == "mtu") {
                break;
            }
            if ($vals[2] != "inet" && $vals[2] != "inet6") {
                break;
            }
            if (preg_match("/(.+?)(?:@.+)?:$/", $vals[1], $res)) {
                break;
            }
            $ret = preg_match("/(\d*+.\d*+.\d*+.\d*+)[\/(\d*+)]*/", $vals[3], $ip);

            $interfaces[$vals[1] . ':' . $vals[2]] = array('name' => $vals[1], 'type' => $vals[2], 'ip' => ((empty($ip[1]) ? '' : $ip[1])));
        }
        return $interfaces;
    }

    private function before($thing, $inthat) {
        return substr($inthat, 0, strpos($inthat, $thing));
    }

    private function array_key_exists_recursive($key, $arr) {
        if (array_key_exists($key, $arr)) {
            return true;
        }
        foreach ($arr as $currentKey => $value) {
            if (is_array($value)) {
                return $this->array_key_exists_recursive($key, $value);
            }
        }
        return false;
    }

    private function strpos_array($haystack, $needles) {
        if (is_array($needles)) {
            foreach ($needles as $str) {
                if (is_array($str)) {
                    $pos = $this->strpos_array($haystack, $str);
                } else {
                    $pos = strpos($haystack, $str);
                }
                if ($pos !== FALSE) {
                    return $pos;
                }
            }
        } else {
            return strpos($haystack, $needles);
        }
        return FALSE;
    }
    private function getTableDefaults($table, $trim_underscore = true) {
        $def_val = array();
        // TODO: This is ugly and overkill - needs to be cleaned up in dbinterface
        if ($table == 'sccpsettings') {
            // sccpsettings has a different structure and already have values in $sccpvalues
            return $this->sccpvalues;
        }
        $sccpTableDesc = $this->dbinterface->getSccpDeviceTableData("get_columns_{$table}");
        foreach ($sccpTableDesc as $key => $data) {
            // function has 2 roles: return actual table keys (trim_underscore = false)
            // return sanitised keys to add defaults (trim_underscore = true)
            if ($trim_underscore) {
                // Remove any leading (or trailing but should be none) underscore
                // These are only used to hide fields from chan-sccp for compatibility
                $key = trim($key,'_');
            }
            $def_val[$key] = array("keyword" => $key, "data" => $data['Default'], "seq" => "99");
        }
        return $def_val;
    }

    private function getTableEnums($table, $trim_underscore = true) {
        $enumFields = array();
        $sccpTableDesc = $this->dbinterface->getSccpDeviceTableData("get_columns_{$table}");
        foreach ($sccpTableDesc as $key => $data) {
            // function has 2 roles: return actual table keys (trim_underscore = false)
            // return sanitised keys to add defaults (trim_underscore = true)
            if ($trim_underscore) {
                // Remove any leading (or trailing but should be none) underscore
                // These are only used to hide fields from chan-sccp for compatibility
                $key = trim($key,'_');
            }
            $typeArray = explode('(', $data['Type']);
            if ($typeArray[0] == 'enum') {
                $enumOptions = explode(',', trim($typeArray[1],')'));
                $enumFields[$key] = $enumOptions;
            }
        }
        return $enumFields;
    }

    private function findAllFiles($searchDir, $file_mask = array(), $mode = 'full') {
        $result = array();
        if (!is_dir($searchDir)) {
            return $result;
        }
        foreach (array_diff(scandir($searchDir),array('.', '..')) as $value) {
            if (is_file("$searchDir/$value")) {
                $extFound = '';
                $foundFile = true;
                if (!empty($file_mask)) {
                    $foundFile = false;
                    foreach ($file_mask as $k) {
                        if (strpos($value, $k) !== false) {
                            $foundFile = true;
                            $extFound = $k;
                            break;
                        }
                    }
                }
                if ($foundFile) {
                    switch ($mode) {
                        case 'fileonly':
                            $result[] = $value;
                            break;
                        case 'fileBaseName':
                            $result[] = basename("/$value", $extFound);
                            break;
                        case 'dirFileBaseName':
                            $result[] = $searchDir . "/" . basename("/$value", $extFound);
                            break;
                        default:
                            $result[] = "$searchDir/$value";
                            break;
                    }
                }
                break;
            }
            // Now iterate over sub directories
            $sub_find = $this->findAllFiles("$searchDir/$value", $file_mask, $mode);
            if (!empty($sub_find)) {
                foreach ($sub_find as $sub_value) {
                    $result[] = $sub_value;
                }
            }
        }
        return $result;
    }

    function is_assoc($array) {
        foreach (array_keys($array) as $k => $v) {
            if ($k !== $v)
              return true;
            }
        return false;
    }

    function tftpReadTestFile($remoteFileName, $host = "127.0.0.1")
    {
        // https://datatracker.ietf.org/doc/html/rfc1350
        $socket = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
        // Set timeout so that do not hang if no data received.
        socket_set_option($socket,SOL_SOCKET, SO_RCVTIMEO, array('sec'=>1, 'usec'=>0));

        if ($socket) {
            $port = 69;   // Initial TFTP port. Changed in received packet.

            // create the RRQ request packet
            $packet = chr(0) . chr(1) . $remoteFileName . chr(0) . 'netascii' . chr(0);
            // UDP is connectionless, so we just send it.
            socket_sendto($socket, $packet, strlen($packet), MSG_EOR, $host, $port);

            $buffer = null;
            $port = "";
            $ret = "";
            // MSG_WAITALL is blocking but socket has timeout set to 1 sec.
            $numbytes = socket_recvfrom($socket, $buffer, 84, MSG_WAITALL, $host, $port);

            if ($numbytes < 2) {
                // socket has timed out before data received.
                return false;
            }
            // unpack the returned buffer and discard the first two bytes.
            $pkt = unpack("nopcode/nblockno/a*data", $buffer);

            // send ack and close socket.
            $packet = chr(4) . chr($pkt["blockno"]);
            socket_sendto($socket, $packet, strlen($packet), MSG_EOR, $host, $port);

            socket_close($socket);

            if ($pkt["opcode"] == 3 && $numbytes) {
                return $pkt["data"];
            }
        }
        return false;
    }

    public function initialiseConfInit(){
        $read_config = \FreePBX::LoadConfig()->getConfig('sccp.conf');
        $sccp_conf_init['general'] = $read_config['general'];
        foreach ($read_config as $key => $value) {
            if (isset($read_config[$key]['type'])) { // copy soft key
                if ($read_config[$key]['type'] == 'softkeyset') {
                    $sccp_conf_init[$key] = $read_config[$key];
                }
            }
        }
        return $sccp_conf_init;
    }


    public function checkTftpMapping(){
        exec('in.tftpd -V', $tftpInfo);
        $info['TFTP Server'] = array('Version' => 'Not Found', 'about' => 'Mapping not available');

        if (isset($tftpInfo[0])) {
            $tftpInfo = explode(',',$tftpInfo[0]);
            $info['TFTP Server'] = array('Version' => $tftpInfo[0], 'about' => 'Mapping not available');
            $tftpInfo[1] = trim($tftpInfo[1]);
            $this->sccpvalues['tftp_rewrite']['data'] = 'off';
            if ($tftpInfo[1] == 'with remap') {
                $info['TFTP Server'] = array('Version' => $tftpInfo[0], 'about' => $tftpInfo[1]);

                $remoteFileName = ".sccp_manager_remap_probe_sentinel_temp".mt_rand(0, 9999999).".tlzz";
                $remoteFileContent = "# This is a test file created by Sccp_Manager. It can be deleted without impact";
                $tftp_base = $this->sccpvalues['tftp_path']['data'] ?? '/tftpboot';
$testFtpDir = "{$tftp_base}/settings";

                // write a sentinel to a tftp subdirectory to see if mapping is working

                if (is_dir($testFtpDir) && is_writable($testFtpDir)) {
                    $tempFile = "{$testFtpDir}/{$remoteFileName}";
                    file_put_contents($tempFile, $remoteFileContent);
                    // try to pull the written file through tftp.
                    // this way we can determine if mapping is active and using sccp_manager maps
                    if ($remoteFileContent == $this->tftpReadTestFile($remoteFileName)) {
                        //found the file and contents are correct
                        $this->sccpvalues['tftp_rewrite']['data'] = 'pro';
                    } else {
                        // Did not find sentinel so mapping not available
                        $this->sccpvalues['tftp_rewrite']['data'] = 'off';
                    }
                    unlink($tempFile);
                }
                return true;
            }
        }
        return false;
    }
    // helper function to save xml with proper indentation
    public function saveXml($xml, $filename) {
       $dom = new \DOMDocument("1.0");
       $dom->preserveWhiteSpace = false;
       $dom->formatOutput = true;
       $dom->loadXML($xml->asXML());
       $dom->save($filename);
    }

    public function getFileListFromProvisioner(string $tftpRootPath) {

        $provisionerUrl = "https://github.com/dkgroot/provision_sccp/raw/master/";
        // Get master tftpboot directory structure
        try {
            file_put_contents("{$tftpRootPath}/masterFilesStructure.xml",file_get_contents("{$provisionerUrl}tools/tftpbootFiles.xml"));
        } catch (\Exception $e) {
            return false;
        }
        return true;
    }

    public function getChanSccpSettings() {
        // This is a utility function for debug only, and is not used by core code
        foreach (array('general','line', 'device') as $section) {
            $sysConfig = $this->aminterface->getSCCPConfigMetaData($section);
            dbug($sysConfig);
        }
        unset($sysConfig);
    }

    public function createDefaultSccpConfig(array $sccpvalues, string $asteriskPath, $conf_init) {
        global $cnf_wr;
        // Make sccp.conf data
        // [general] section
        // TODO: Need to review sccpsettings seq numbering, as will speed this up, and remove the need for $permittedSettings.
        $cnf_wr = \FreePBX::WriteConfig();
        //clear old general settings, and initiate with allow/disallow and permit/deny keys in correct order
        $conf_init['general'] = array();
        $conf_init['general']['disallow'] = 'all';
        $conf_init['general']['allow'] = '';
        $conf_init['general']['deny'] = '0.0.0.0/0.0.0.0';
        $conf_init['general']['permit'] = '0.0.0.0/0.0.0.0';
        // permitted chan-sccp settings array
        $permittedSettings = array(
                          'debug', 'servername', 'keepalive', 'context', 'dateformat', 'bindaddr', 'port', 'secbindaddr', 'secport', 'disallow', 'allow', 'deny', 'permit',
                          'localnet', 'externip', 'externrefresh', 'firstdigittimeout', 'digittimeout', 'digittimeoutchar', 'recorddigittimeoutchar', 'simulate_enbloc',
                          'ringtype', 'autoanswer_ring_time', 'autoanswer_tone', 'remotehangup_tone', 'transfer', 'transfer_tone', 'transfer_on_hangup', 'dnd_tone',
                          'callwaiting_tone', 'callwaiting_interval', 'musicclass', 'language', 'callevents', 'accountcode', 'sccp_tos', 'sccp_cos', 'audio_tos',
                          'audio_cos', 'video_tos', 'video_cos', 'echocancel', 'silencesuppression', 'earlyrtp', 'dndFeature', 'private', 'mwilamp', 'mwioncall',
                          'cfwdall', 'cfwdbusy', 'cfwdnoanswer', 'cfwdnoanswer_timeout', 'nat', 'directrtp', 'allowoverlap', 'pickup_modeanswer',
                          'callhistory_answered_elsewhere', 'amaflags', 'callanswerorder', 'devicetable', 'linetable', 'meetmeopts', 'jbenable', 'jbforce',
                          'jblog', 'jbmaxsize', 'jbresyncthreshold', 'jbimpl', 'hotline_enabled', 'hotline_extension', 'hotline_context', 'hotline_label', 'fallback',
                          'backoff_time', 'server_priority');

        foreach ($sccpvalues as $key => $value) {
            if (!in_array($key, $permittedSettings, true)) {
                break;
            }
            if ($value['seq'] == 0) {
                switch ($key) {
                    case "allow":
                    case "disallow":
                    case "deny":
                    case "localnet":
                    case "permit":
                        $conf_init['general'][$key] = explode(';', $value['data']);
                        break;
                    case "devlang":
                        /*
                        $lang_data = $this->extconfigs->getExtConfig('sccp_lang', $value['data']);
                        if (!empty($lang_data)) {
                            // TODO:  will always get here, but lang_data['codepage'] will be empty as not a valid key
                            $this->sccp_conf_init['general']['phonecodepage'] = $lang_data['codepage'];
                        }
                        break;
                        */
                    case "netlang": // Remove Key
                    case "tftp_path":
                    case "sccp_compatible":    // This is equal to SccpDBmodel
                        break;
                    default:
                        if (!empty($value['data'])) {
                            $conf_init['general'][$key] = $value['data'];
                        }
                }
            }
        }
        //
        // ----- It is a very bad idea to add an external configuration file "sccp_custom.conf" !!!!
        // This will complicate solving problems caused by unexpected solutions from users.
        //
        if (file_exists($asteriskPath . "/sccp_custom.conf")) {
            $conf_init['HEADER'] = array(
                ";                                                                                ;",
                ";  It is a very bad idea to add an external configuration file !!!!              ;",
                ";  This will complicate solving problems caused by unexpected solutions          ;",
                ";  from users.                                                                   ;",
                ";--------------------------------------------------------------------------------;",
                "#include sccp_custom.conf"
            );
        }
        $cnf_wr->WriteConfig('sccp.conf', $conf_init);
    }

    public function initVarfromXml() {
        if ((array) $this->xml_data) {
            foreach ($this->xml_data->xpath('//page_group') as $item) {
                foreach ($item->children() as $child) {
                    $seq = 0;
                    if (!empty($child['seq'])) {
                        $seq = (string) $child['seq'];
                    }
                    if ($seq < 99) {
                        if ($child['type'] == 'IE') {
                            foreach ($child->xpath('input') as $value) {
                                $tp = 0;
                                if (empty($value->value)) {
                                    $datav = (string) $value->default;
                                } else {
                                    $datav = (string) $value->value;
                                }
                                if (strtolower($value->type) == 'number') {
                                    $tp = 1;
                                }
                                if (empty($this->sccpvalues[(string) $value->name])) {
                                    $this->sccpvalues[(string) $value->name] = array('keyword' => (string) $value->name, 'data' => $datav, 'type' => $tp, 'seq' => $seq, 'systemdefault' => '');
                                }
                            }
                        }
                        if ($child['type'] == 'IS' || $child['type'] == 'IED') {
                            if (empty($child->value)) {
                                $datav = (string) $child->default;
                            } else {
                                $datav = (string) $child->value;
                            }
                            if (empty($this->sccpvalues[(string) $child->name])) {
                                $this->sccpvalues[(string) $child->name] = array('keyword' => (string) $child->name, 'data' => $datav, 'type' => '2', 'seq' => $seq, 'systemdefault' => '');
                            }
                        }
                        if (in_array($child['type'], array('SLD', 'SLS', 'SLT', 'SLNA', 'SLDA', 'SL', 'SLM', 'SLZ', 'SLTZN', 'SLA'))) {
                            if (empty($child->value)) {
                                $datav = (string) $child->default;
                            } else {
                                $datav = (string) $child->value;
                            }
                            if (empty($this->sccpvalues[(string) $child->name])) {
                                $this->sccpvalues[(string) $child->name] = array('keyword' => (string) $child->name, 'data' => $datav, 'type' => '2', 'seq' => $seq, 'systemdefault' => '');
                            }
                        }
                    }
                }
            }
        }
    }

    public function getSipConfig() {
        // Only called from sccp_manager class when saving SIP device
        $result = array();

        $tmp_binds = \FreePBX::Sipsettings()->getBinds();
        $if_list = $this->getIpInformation('ip4');
        if (!is_array($tmp_binds)) {
            // FreePBX has no sip bindings.
            die_freepbx(_("SIP server configuration error ! No SIP protocols enabled"));
        }
        foreach ($tmp_binds as $fpbx_protocol => $fpbx_bind) {
            foreach ($fpbx_bind as $protocol_ip => $protocol_port_arr) {
                if (empty($protocol_port_arr)) {
                    break;
                }
                if (($protocol_ip == '0.0.0.0') || ($protocol_ip == '[::]')) {
                    foreach ($if_list as $if_type => $if_data) {
                        if ($if_data['ip'] == "127.0.0.1") {
                            break;
                        }
                        if (empty($result[$fpbx_protocol][$if_data['ip']])) {
                            $result[$fpbx_protocol][$if_data['ip']]= $protocol_port_arr;
                        } else {
                            $result[$fpbx_protocol][$if_data['ip']]= array_merge($result[$fpbx_protocol][$if_data['ip']],$protocol_port_arr);
                        }
                        $result[$fpbx_protocol][$if_data['ip']]['ip']=$if_data['ip'];
                    }
                } else {
                    $result[$fpbx_protocol][$protocol_ip]=$protocol_port_arr;
                    $result[$fpbx_protocol][$protocol_ip]['ip']=$protocol_ip;
                }
            }
        }
        if (empty($result)) {
            die_freepbx(_("SIP server configuration error ! No SIP protocols enabled"));
        }
        return $result;
    }
}
?>
