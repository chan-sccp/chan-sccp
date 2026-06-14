<?php

namespace FreePBX\modules\sccp_manager\sccpManTraits;

trait ajaxHelper {

    public function ajaxRequest($req, &$setting) {
        // Called first by BMO. Must return true or request will be aborted.
        // See https://wiki.freepbx.org/display/FOP/BMO+Ajax+Calls
        switch ($req) {
            case 'backupsettings':
            case 'savesettings':
            case 'save_device':
            case 'save_sip_device':
            case 'save_ruser':
            case 'save_dialplan_template':
            case 'delete_hardware':
            case 'getPhoneGrid':
            case 'getExtensionGrid':
            case 'getDeviceModel':
            case 'getUserGrid':
            case 'getSoftKey':
            case 'getDialTemplate':
            case 'get_ext_files':
            case 'create_hw_tftp':
            case 'reset_dev':
            case 'reset_token':
            case 'model_enabled':
            case 'model_disabled':
            case 'model_update':
            case 'model_add':
            case 'model_delete':
            case 'update_button_label':
            case 'updateSoftKey':
            case 'deleteSoftKey':
            case 'delete_dialplan':
                return true;
                break;
            case 'validateMac':
                return true;
                break;
            default:
                return false;
        }
    }

    // ajaxHandler is called after ajaxRequest returns true
    public function ajaxHandler() {
        $request = $_REQUEST;
        $msg = array();
        $cmd_id = $request['command'];
        switch ($cmd_id) {
            case 'savesettings':
                // Consolidate this into a separate method to improve legibility
                $this->handleSubmit($request);

                // TODO: Need to be more specific on reload and only reload if critical settings changed.
                $res = $this->aminterface->core_sccp_reload();
                return array('status' => true, 'message' => 'Data saved', 'reload' => true, 'toastFlag' => 'success', 'search' => '?display=sccpsettings', 'hash' => '');
                break;
            case 'save_sip_device':
            case 'save_device':
                return $this->saveSccpDevice($request);
                break;
            case 'save_ruser':
                //$res = $request;
                $res = $this->handleRoamingUsers($request);
                return array('status' => true, 'search' => '?display=sccp_phone', 'hash' => 'general');
                break;
            case 'save_dialplan_template':
                /* !TODO!: -TODO-: dialplan templates should be removed (only required for very old devices (like ATA) */
                // -------------------------------   Old +  Sip device support - In the development---
                $res = $this->saveDialPlan($request);
                //public
                if (empty($res)) {
                    return array('status' => true, 'search' => '?display=sccp_adv', 'hash' => 'sccpdialplan');
                } else {
                    return array('status' => false, 'message' => print_r($res));
                }
                break;
            case 'delete_dialplan':
                if (!empty($request['dialplan'])) {
                    $get_file = $request['dialplan'];
                    $res = $this->deleteDialPlan($get_file);
                    return array('status' => true, 'message' => 'Dial Template has been deleted ! ', 'table_reload' => true);
                } else {
                    return array('status' => false, 'message' => print_r($res));
                }
                break;
            // -------------------------------   Old device support - In the development---
            case 'delete_hardware':
                if (!empty($request['idn'])) {
                    foreach ($request['idn'] as $idv) {
                        if ($this->strpos_array($idv, array('SEP', 'ATA', 'VG')) !== false) {
                            $this->dbinterface->write('sccpdevice', array('name' => $idv), 'delete', "name");
                            $this->dbinterface->write('sccpbuttons', array(), 'delete', '', $idv);
                            $this->deleteSccpDeviceXML($idv); // Концы в вводу !!
                            $this->aminterface->sccpDeviceReset($idv, 'reset');
                        }
                    }
                    return array('status' => true, 'table_reload' => true, 'message' => 'Hardware device has been deleted! ');
                }
                break;
            case 'create_hw_tftp':
                $ver_id = ' Test !';
                if (!empty($request['idn'])) {
                    $models = array();
                    foreach ($request['idn'] as $idv) {
                        $this->deleteSccpDeviceXML($idv);
                        $models [] = array('name' => $idv);
                    }
                } else {
                    $this->deleteSccpDeviceXML('all');
                    $models = $this->dbinterface->getSccpDeviceTableData("SccpDevice");
                }

                $this->createDefaultSccpXml(); // Default XML
                $ver_id = ' on found active model !';
                foreach ($models as $data) {
                    $ver_id = $this->createSccpDeviceXML($data['name']);
                    if ($ver_id == -1) {
                        return array('status' => false, 'message' => 'Error Create Configuration Divice :' . $data['name']);
                    }
                };

                if ($this->sccpvalues['siptftp']['data'] == 'on') { // Check SIP Support Enabled
                    $this->createSccpXmlSoftkey(); // Create Softkey Sets for SIP
                }
                // !TODO!: -TODO-: Do these returned message strings work with i18n ?
                return array('status' => true, 'message' => 'Create new config files (version:' . $ver_id . ')');

                break;
            case 'reset_token':
            case 'reset_dev':
                $msg = '';
                $msgr = array();
                $msgr[] = "Reset command sent to device(s) ";
                if (!empty($request['name'])) {
                    foreach ($request['name'] as $idv) {
                        $msg = strpos($idv, 'SEP-');
                        if (!(strpos($idv, 'SEP') === false)) {
                            if ($cmd_id == 'reset_token') {
                                $res = $this->aminterface->sccpDeviceReset($idv, 'tokenack');
                                $msgr[] = $msg . ' ' . $res['Response'] . ' ' . $res['data'];
                            } else {
                                $res = $this->aminterface->sccpDeviceReset($idv, 'reset');
                                $msgr[] = $msg . ' ' . $res['Response'] . ' ' . $res['data'];
                            }
                        }

                        if ($idv == 'all') {
                            $dev_list = $this->aminterface->sccp_get_active_device();
                            foreach ($dev_list as $key => $data) {
                                if ($cmd_id == 'reset_token') {
                                    if (($data['token'] == 'Rej') || ($data['status'] == 'Token ')) {
                                        $res = $this->aminterface->sccpDeviceReset($idv, 'tokenack');
                                        $msgr[] = 'Sent Token reset to :' . $key;
                                    }
                                } else {
                                    $res = $this->aminterface->sccpDeviceReset($idv, 'reset');
                                    $msgr[] = $res['Response'] . ' ' . $res['data'];
                                }
                            }
                        }
                    }
                }
                return array('status' => (($res['Response'] == 'Error')? false : true ), 'message' => $msgr, 'reload' => false, 'table_reload' => true);
                break;
            case 'update_button_label':
                $msg = '';
                $hw_list = array();
                if (!empty($request['name'])) {
                    foreach ($request['name'] as $idv) {
                        if (!(strpos($idv, 'SEP') === false)) {
                            $hw_list[] = array('name' => $idv);
                        }
                        if ($idv == 'all') {

                        }
                    }
                }
                $res = $this->updateSccpButtons($hw_list);
                $msg .= $res['Response'] . (empty($res['data']) ? '' : ' raw data: ' . $res['data'] . ' ');
                return array('status' => true, 'message' => 'Update Buttons Labels Complete: ' . $msg, 'reload' => false, 'table_reload' => true);
            case 'model_add':
                $save_settings = array();
                $key_name = array('model', 'vendor', 'dns', 'buttons', 'loadimage', 'loadinformationid', 'nametemplate');
                $upd_mode = 'replace';
            case 'model_update':
                if ($request['command'] == 'model_update') {
                    $key_name = array('model','vendor','dns', 'buttons', 'loadimage', 'loadinformationid', 'nametemplate');
                    $upd_mode = 'update';
                }
                if (!empty($request['model'])) {
                    foreach ($key_name as $key => $value) {
                        if (!empty($request[$value])) {
                            $save_settings[$value] = $request[$value];
                        } else {
                            $save_settings[$value] = $this->val_null; // null
                        }
                    }
                    $this->dbinterface->write('sccpdevmodel', $save_settings, $upd_mode, "model");
                    return array('status' => true, 'table_reload' => true);
                }
                return $save_settings;
                break;
            case 'model_enabled':
                $model_set = '1';     // fall through intentionally
            case 'model_disabled':
                if ($request['command'] == 'model_disabled') {
                    $model_set = '0';
                }
                $msg = '';
                $save_settings = array();
                if (!empty($request['model'])) {
                    foreach ($request['model'] as $idv) {
                        $this->dbinterface->write('sccpdevmodel', array('model' => $idv, 'enabled' => $model_set), 'update', "model");
                    }
                }
                return array('status' => true, 'table_reload' => true);
                break;
            case 'model_delete':
                if (!empty($request['model'])) {
                    $this->dbinterface->write('sccpdevmodel', array('model' => $request['model']), 'delete', "model");
                    return array('status' => true, 'table_reload' => true);
                }
                break;
            case 'getDeviceModel':
                switch ($request['type']) {
                    case 'all':
                    case 'extension':
                    case 'enabled':
                        $devices = $this->getSccpModelInformation($request['type'], $validate = true);
                        break;
                }
                if (empty($devices)) {
                    return array();
                }
                return $devices;
                break;

            case 'deleteSoftKey':
                if (!empty($request['softkey'])) {
                    $id_name = $request['softkey'];
                    unset($this->sccp_conf_init[$id_name]);
                    $this->createDefaultSccpConfig($this->sccpvalues, $this->sccppath["asterisk"], $this->sccp_conf_init);
                    $msg = print_r($this->aminterface->core_sccp_reload(), 1);
                    return array('status' => true, 'table_reload' => true);
                }
                break;
            case 'updateSoftKey':
                if (!empty($request['id'])) {
                    $id_name = preg_replace('/[^A-Za-z0-9]/', '', $request['id']);
                    $this->sccp_conf_init[$id_name]['type'] = "softkeyset";
                    foreach ($this->extconfigs->getExtConfig('keyset') as $keyl => $vall) {
                        if (!empty($request[$keyl])) {
                            $this->sccp_conf_init[$id_name][$keyl] = $request[$keyl];
                        }
                    }
                    $this->createDefaultSccpConfig($this->sccpvalues, $this->sccppath["asterisk"], $this->sccp_conf_init);

                    // !TODO!: -TODO-:  Check SIP Support Enabled
                    $this->createSccpXmlSoftkey();
                    $msg = print_r($this->aminterface->core_sccp_reload(), 1);
                    return array('status' => true, 'table_reload' => true);
                }
                break;
            case 'getSoftKey':
                $result = array();
                $i = 0;
                $keyl = 'default';
                foreach ($this->aminterface->sccp_list_keysets() as $keyl => $vall) {
                    $result[$i]['softkeys'] = $keyl;
                    if ($keyl == 'default') {
                        foreach ($this->extconfigs->getExtConfig('keyset') as $key => $value) {
                            $result[$i][$key] = str_replace(',', '<br>', $value);
                        }
                    } else {
                        foreach ($this->getMyConfig('softkeyset', $keyl) as $key => $value) {
                            $result[$i][$key] = str_replace(',', '<br>', $value);
                        }
                    }

                    $i++;
                }
                return $result;
                break;
            case 'getExtensionGrid':
                $lineList = $this->dbinterface->getSccpDeviceTableData($request['type']);
                if (empty($lineList)) {
                    return array();
                }
                $activeDevices = $this->aminterface->sccp_get_active_device();
                $uniqueLineList = array();
                foreach ($lineList as $key => &$lineArr) {
                    if (array_key_exists($lineArr['mac'], $activeDevices)) {
                        $lineArr['line_status'] = "{$activeDevices[$lineArr['mac']]['status']} | {$activeDevices[$lineArr['mac']]['act']}";
                    }
                    if (array_key_exists($lineArr['name'], $uniqueLineList)) {
                        $lineList[$uniqueLineList[$lineArr['name']]]['mac'] .= '<br>' . $lineArr['mac'];
                        $lineList[$uniqueLineList[$lineArr['name']]]['line_status'] .= '<br>' . $lineArr['line_status'];
                        unset($lineList[$key]);  // Drop this array as no longer used
                        break;
                    }
                    $uniqueLineList[$lineArr['name']] = $key;
                }
                unset($lineArr, $uniqueLineList); // unset reference and temp vars.
                return array_values($lineList);   // Reindex array and return
                break;
            case 'getPhoneGrid':
                $dbDevices = array();
                // Find all devices defined in the database.
                $dbDevices = $this->dbinterface->getSccpDeviceTableData('phoneGrid', array('type' => $request['type']));

                // Return if only interested in SIP devices
                if ($request['type'] == 'cisco-sip') {
                    return $dbDevices;     //this may be empty
                }
                // Find all devices currently connected
                $activeDevices = $this->aminterface->sccp_get_active_device();

                foreach ($dbDevices as &$dev_id) {
                    if (!empty($activeDevices[$dev_id['name']])) {
                        // Device is in db and is connected
                        $dev_id['description'] = $activeDevices[$dev_id['name']]['descr'];
                        $dev_id['status'] = $activeDevices[$dev_id['name']]['status'];
                        $dev_id['address'] = $activeDevices[$dev_id['name']]['address'];
                        $dev_id['new_hw'] = 'N';
                        // No further action required on this active device
                        unset($activeDevices[$dev_id['name']]);
                    }
                }
                unset($dev_id); // unset reference.

                if (!empty($activeDevices)) {
                    // Have a device that is connected but is not currently in the database
                    // This device must have connected via hotline or config in sccp.conf.
                    // Pass parameters to addDevice so that can be added to db.
                    foreach ($activeDevices as $dev_ids) {
                        $id_name = $dev_ids['name'];
                        $dev_data = $this->aminterface->sccp_getdevice_info($id_name);
                        if (!empty($dev_data['SCCP_Vendor']['model_id'])) {
                            $dev_addon = $dev_data['SCCP_Vendor']['model_addon'];
                            if (empty($dev_addon)) {
                                $dev_addon = null;
                            }
                            $dev_schema = $this->getSccpModelInformation('byciscoid', false, "all", array('model' => $dev_data['SCCP_Vendor']['model_id']));
                            if (empty($dev_schema)) {
                                $dev_schema[0]['model'] = "ERROR in Model Schema";
                            }
                            $dbDevices[] = array(
                                'name' => $id_name,
                                'mac' => $id_name,
                                'button' => '---',
                                'type' => $dev_schema[0]['model'],
                                'new_hw' => 'Y',
                                'description' => '*NEW* ' . $dev_ids['descr'],
                                'status' => '*NEW* ' . $dev_ids['status'],
                                'address' => $dev_ids['address'],
                                'addon' => $dev_addon
                            );
                        }
                    }
                }
                return $dbDevices;
                break;
            case 'getDialTemplate':
                // -------------------------------   Old device support - In the development---
                $result = $this->getDialPlanList();
                if (empty($result)) {
                    $result = array();
                }
                return $result;
                break;
            case 'backupsettings':
                // -------------------------------   Old device support - In the development---
                $filename = $this->createSccpBackup();
                $file_name = basename($filename);

                header("Content-Type: application/zip");
                header("Content-Disposition: attachment; filename=$file_name");
                header("Content-Length: " . filesize($filename));

                readfile($filename);
                unlink($filename);

                // return array('status' => false, 'message' => $result);
                return $result;
                break;
            case 'validateMac':
                break;
            case 'get_ext_files':
                return $this->getFilesFromProvisioner($request);
                break;
        }
    }

    function handleSubmit($request, $validateonly = false) {
        $hdr_prefix = 'sccp_';
        $hdr_arprefix = 'sccp-ar_';
        $save_settings = array();
        $save_codec = array();
        $count_mods = 0;
        $dbSaveArray = array();
        $errors = array();

        if (isset($request["{$hdr_prefix}createlangdir"]) && ($request["{$hdr_prefix}createlangdir"] == 'yes')) {
            $this->initializeTFtpLanguagePath();
        }
        // if uncheck all codecs, audiocodecs key is missing so nothing changes in db.
        // Unsetting all codecs will now return to chan-sccp defaults.
        // all codecs are currently treated as audiocodecs. To treat videocodecs separately name in video codec section of
        // server.codec needs to be changed from audiocodecs to videocodecs.
        if (!isset($request['audiocodecs'])) {
            $save_settings['allow'] = $this->sccpvalues['allow'];
            $save_settings['allow']['data'] = $this->sccpvalues['allow']['systemdefault'];
        } else {
            foreach ($request['audiocodecs'] as $keycodeс => $dumVal) {
                $save_codec[] = $keycodeс;
            }
            $save_settings['allow'] = $this->sccpvalues['allow'];
            $save_settings['allow']['data'] = implode(";", $save_codec);
        }
        unset($request['audiocodecs']);

        if (isset($request[$hdr_prefix . 'ntp_timezone'])) {
            $TZdata = $this->extconfigs->getExtConfig('sccp_timezone', $request[$hdr_prefix . 'ntp_timezone']);
            if (!empty($TZdata)) {
                $save_settings['tzoffset'] = array(
                                            'keyword' => 'tzoffset',
                                            'data' => $TZdata['offset']/60,
                                            'seq' => '98',
                                            'type' => '2',
                                            'systemdefault' => ''
                                            );
            }
            unset($request[$hdr_prefix . 'ntp_timezone']);
        }
        // Now handle remaining data. First get table defaults
        $sccpdevice_def = (array)$this->getTableDefaults('sccpdevice', false);
        $sccpline_def = (array)$this->getTableDefaults('sccpline', false);

        // before handling arrays, need to see if deny and permit are set in the request
        // if they have been cleared by the users, will not be present

        foreach (['deny','permit'] as $keyVal) {
            if (!isset($request[$hdr_arprefix.$keyVal])) {
                $tmpArr = $this->convertCsvToArray($this->sccpvalues[$keyVal]['systemdefault']);
                if (isset($tmpArr[0]['internal'])) {
                    $request[$hdr_arprefix.$keyVal][0] = $tmpArr[0];
                } else {
                    $request[$hdr_arprefix.$keyVal][1]['net'] = $tmpArr[0]['net'];
                    $request[$hdr_arprefix.$keyVal][1]['mask'] = $tmpArr[0]['mask'];
                }
            }
        }

        foreach ($request as $key => $value) {
            // First handle any arrays as their prefix is part common with normal data
            //$netvalue = array();
            $key = (str_replace($hdr_arprefix, '', $key, $count_mods));
            if ($count_mods) {
                // Only arrays : network lists or ip lists.
                $save_settings[$key]['keyword'] = $key;
                $save_settings[$key]['type'] = $this->sccpvalues[$key]['type'];
                $save_settings[$key]['seq'] = $this->sccpvalues[$key]['seq'];
                $save_settings[$key]['data'] = $this->convertArrayToCsv($value);
                $save_settings[$key]['systemdefault'] = $this->sccpvalues[$key]['systemdefault'];
                break;
            }
            // Now handle any normal data - arrays will not match as already handled.
            if (strpos($key, $hdr_prefix) === 0) {
                $key = (str_replace($hdr_prefix, '', $key, $count_mods));
                if (($count_mods) && (!empty($this->sccpvalues[$key])) && ($this->sccpvalues[$key]['data'] != $value)) {
                        $save_settings[$key] = $this->sccpvalues[$key];
                        $save_settings[$key]['data'] = $value;
                }
                break;
            }
            // Finally treat values to be saved to sccpdevice and sccpline defaults.
            // TODO: Need to verify the tables defined in showGroup - some options maybe
            // device options, but if set by freePbx extensions, be in sccpline.
			$sccpdevice_def = $this->sccpvalues['device'] ?? array();
$sccpline_def = $this->sccpvalues['line'] ?? array();
            foreach (array('sccpdevice', 'sccpline') as $tableName) {
                $key = (str_replace("{$tableName}_", '', $key, $count_mods));
                if ($count_mods) {
                    // Have default to be saved to db table default
                    $tableName_def = "{$tableName}_def";
                    if ((array_key_exists($key, $$tableName_def)) && ($$tableName_def[$key]['data'] == $value)) {
                        // Value unchanged so ignore
                    } else {
                        $dbSaveArray[$key] = array('table' => $tableName, 'field' => $key, 'Default' => $value);
                    }
                    // If have matched on device, cannot match on line
                    continue 2;
                }
            }

        }
        $extSettings = $this->extconfigs->updateTftpStructure(array_merge($this->sccpvalues, $save_settings));
        $save_settings = array_merge($save_settings, $extSettings);
        //dbug($save_settings);
        if (!empty($save_settings)) {
            $this->saveSccpSettings($save_settings);
            $this->sccpvalues = $this->dbinterface->get_db_SccpSetting();
        }

        // now add the site defaults from sccpsettings to sccpdevice for permit and deny, so that these will override
        foreach (['deny', 'permit'] as $fieldId) {
            $output = array();
            foreach ($this->convertCsvToArray($this->sccpvalues[$fieldId]['data']) as $netValue) {
                if (isset($netValue['internal'])) {
                    $output[] = 'internal';
                    break;
                }
                $output[] = implode('/', $netValue);
            }
            $dbSaveArray[$fieldId] = array('table' => 'sccpdevice', 'field' => $fieldId, 'Default' => implode(';',$output));
        }

        foreach ($dbSaveArray as $key => $rowToSave) {
            $this->dbinterface->updateTableDefaults($rowToSave['table'], $rowToSave['field'], $rowToSave['Default']);
        }
        // rewrite sccp.conf
        $this->createDefaultSccpConfig($this->sccpvalues, $this->sccppath["asterisk"], $this->sccp_conf_init);
        $this->createDefaultSccpXml();
    }

    public function getMyConfig($var = null, $id = "noid") {
        // TODO: this function has little purpose - need to integrate into AjaxHelper
        switch ($var) {
            case "softkeyset":
                $final = array();
                $i = 0;
                if ($id == "noid") {
                    foreach ($this->sccp_conf_init as $key => $value) {
                        if ($this->sccp_conf_init[$key]['type'] == 'softkeyset') {
                            $final[$i] = $value;
                            $i++;
                        }
                    }
                } else {
                    if (!empty($this->sccp_conf_init[$id])) {
                        if ($this->sccp_conf_init[$id]['type'] == 'softkeyset') {
                            $final = $this->sccp_conf_init[$id];
                        }
                    }
                }

                break;
        }
        return $final;
    }

    public function getFilesFromProvisioner($request) {
        $filesToGet = array();
        $totalFiles = 0;
        $provisionerUrl = "https://github.com/dkgroot/provision_sccp/raw/master/";
        // TODO: Maybe should always fetch to ensure have latest, backing up old version
        if (!file_exists("{$this->sccppath['tftp_path']}/masterFilesStructure.xml")) {
            if (!$this->getFileListFromProvisioner($this->sccppath['tftp_path'])) {
                return array('status' => false,
                    'message' => "{$provisionerUrl}tools/tftpbootFiles.xml cannot be found. Check your internet connection, and that this path exists",
                    'reload' => false);
            }
        }
        $tftpBootXml = simplexml_load_file("{$this->sccppath['tftp_path']}/masterFilesStructure.xml");

        switch ($request['type']) {
            case 'firmware':
                $device = $request['device'];
                $firmwareDir = $tftpBootXml->xpath("//Directory[@name='firmware']");
                $result = $firmwareDir[0]->xpath("//Directory[@name='{$device}']");
                $filesToGet['firmware'] = (array)$result[0]->FileName;
                $totalFiles += count($filesToGet['firmware']);
                $srcDir['firmware'] = $provisionerUrl . (string)$result[0]->DirectoryPath;
                $dstDir['firmware'] = "{$this->sccppath['tftp_firmware_path']}/{$device}";

                $msg = "Firmware for {$device} has been successfully downloaded";
                break;
            case 'locale':
                $language = $request['locale'];
                // Get locales
                $localeDir = $tftpBootXml->xpath("//Directory[@name='languages']");
                $result = $localeDir[0]->xpath("//Directory[@name='{$language}']");
                $filesToGet['language'] = (array)$result[0]->FileName;
                $totalFiles += count($filesToGet['language']);
                $srcDir['language'] = $provisionerUrl . (string)$result[0]->DirectoryPath;
                $dstDir['language'] = "{$this->sccppath['tftp_lang_path']}/{$language}";

                // Get countries. Country is a substring of locale with exception of korea
                $country = explode('_', $language);
                array_shift($country);
                $countryName = array_shift($country);
                while (count($country)>=1) {
                    $countryName .= '_' . array_shift($country);
                }
                $msg = "{$language} Locale and Country tones have been successfully downloaded";
                //fall through intentionally to also get country files

            case 'country':
                if ($totalFiles == 0) {
                    //Request is for countries; if >0, have fallen through from locale
                    $countryName = $request['country'];
                    $msg = "{$countryName} country tones have been successfully downloaded";
                }

                $result = $tftpBootXml->xpath("//Directory[@name='{$countryName}']");
                $filesToGet['country'] = (array)$result[0]->FileName;
                $totalFiles += count($filesToGet['country']);
                $srcDir['country'] = $provisionerUrl . (string)$result[0]->DirectoryPath;
                $dstDir['country'] = "{$this->sccppath['tftp_countries_path']}/{$countryName}";
                break;
            default:
                return array('status' => false, 'message' => 'Invalid request', 'reload' => false);
                break;
        }
        // Now get the files
        $filesRetrieved = 0;
        foreach (array('language','country', 'firmware') as $section){
            if (!isset($dstDir[$section])) {
                // No request for this section
                break;
            }
            $srcDir = $srcDir[$section];
            $dstDir = $dstDir[$section];
            if (!is_dir($dstDir)) {
                mkdir($dstDir, 0755);
            }
            foreach ($filesToGet[$section] as $srcFile) {
                try {
                  file_put_contents("{$dstDir}/{$srcFile}",
                      file_get_contents($srcDir. $srcFile));
                } catch (\Exception $e) {
                    return array('status' => false,
                        'message' => "{$countriesSrcDir}{$srcFile} cannot be found. Check your internet connection, and that this path exists",
                        'reload' => false);
                }
                $filesRetrieved ++;
                $percentComplete = $filesRetrieved *100 / $totalFiles;
                $data = "{$percentComplete},";
                echo $data;
                ob_flush();
                flush();
            }
        }

        return array('status' => true, 'message' => $msg, 'reload' => true);
    }

    function saveSccpDevice($get_settings, $validateonly = false) {
        $hdr_prefix = 'sccp_hw_';
        $hdr_arprefix = 'sccp_hw-ar_';
        $hdr_vendPrefix = 'vendorconfig_';
        $save_buttons = array();
        $save_settings = array();
        $save_codec = array();
        $name_dev = '';
        $db_field = array_keys($this->dbinterface->getSccpDeviceTableData("get_columns_sccpdevice"));
        $hw_id = (empty($get_settings['sccp_device_id'])) ? 'new' : $get_settings['sccp_device_id'];
        $hw_type = (empty($get_settings['sccp_device_typeid'])) ? 'sccpdevice' : $get_settings['sccp_device_typeid'];
        $update_hw = ($hw_id == 'new') ? 'add' : 'clear'; // Clear is delete + add. Only used for buttons
        $hw_prefix = 'SEP';
        if (!empty($get_settings[$hdr_prefix . 'type'])) {
            $value = $get_settings[$hdr_prefix . 'type'];
            if (strpos($value, 'ATA') !== false) {
                $hw_prefix = 'ATA';
            }
            if (strpos($value, 'VG') !== false) {
                $hw_prefix = 'VG';
            }
        }
        foreach ($db_field as $key) {
            $value = "";
            switch ($key) {
                case 'name':
                    if (!empty($get_settings[$hdr_prefix . 'mac'])) {
                        $value = $get_settings[$hdr_prefix . 'mac'];
                        $value = strtoupper(str_replace(array('.', '-', ':'), '', $value)); // Delete mac separators from string
                        $value = sprintf("%012s", $value);
                        if ($hw_prefix == 'VG') {
                            $value = $hw_prefix . $value . '0';
                        } else {
                            $value = $hw_prefix . $value;
                        }
                        $name_dev = $value;
                    }
                    break;
                case 'phonecodepage':
                    // phonecodepage depends on 2 variables - language and phone type (uses java or not).
                    // Non java phones use ISO8859-1 or CP1251 (Cyrillic); java phones use UTF-8. See @dkgroot.
                    // Below list is not definitive or necessarily accurate - needs to be maintained.
                    $nonJavaPhones = array(
                        '6901', '6911', '6921', '6945', '7902', '7905', '7910', '7911', '7912', '7914', '7915', '7916', '7920', '7925', '7926', '7931', '7935', '7936', '7937', '7940', '7960'
                        );
                    // TODO: May be other exceptions so use switch. Historically this is the only one handled
                    if (!empty($get_settings["{$hdr_prefix}devlang"])) {
                        switch ($get_settings["{$hdr_prefix}devlang"]) {
                            case 'Russian_Russian_Federation':
                                $value = (in_array($get_settings['sccp_hw_type'], $nonJavaPhones, true)) ? 'CP1251' : 'utf-8';
                                break;
                            default:
                                $value = (in_array($get_settings['sccp_hw_type'], $nonJavaPhones, true)) ? 'ISO8859-1' : 'utf-8';
                                break;
                        }
                    }
                    break;
                default:
                    // handle vendor prefix
                    if (!empty($get_settings["{$hdr_vendPrefix}{$key}"])) {
                        $value = $get_settings["{$hdr_vendPrefix}{$key}"];
                    }
                    // handle array prefix
                    if (!empty($get_settings["{$hdr_arprefix}{$key}"])) {
                        // Only 3 types of array returned permit,deny, setvar
                        $arr_data = '';
                        $arr_clear = false;
                        $output = array();
                        foreach ($get_settings["{$hdr_arprefix}{$key}"] as $netValue) {
                            switch ($key) {
                                case 'permit':
                                case 'deny';
                                    // Now have an array of settings each with keys net and Mask
                                    // TODO: This needs to be optimised
                                    //foreach ($valueArr as $netValue) {
                                    if (isset($netValue['inherit'])) {
                                        $save_settings['deny'] = 'NONE';
                                        continue 2;
                                    }
                                    if (isset($netValue['internal'])) {
                                        $output[] = 'internal';
                                        continue 2;
                                    }
                                    if (empty($netValue['net'])) {
                                        // empty net so ignored
                                        continue 2;
                                    }
                                    $netValue['mask'] = (empty($netValue['mask'])) ? "255.255.255.0" : $netValue['mask'];
                                    $output[]= implode('/', $netValue);
                                    //}
                                    break;
                                case 'setvar':
                                    $output[] = implode(';', $netValue);
                                    break;
                                // No default case
                            }
                    }
                    $save_settings[$key] = implode(';', $output);
                    unset($output);
                }
                // Now only have normal prefix
                if (!empty($get_settings["{$hdr_prefix}{$key}"])) {
                    $value = $get_settings["{$hdr_prefix}{$key}"];
                }
            }
            if (!empty($value)) {
                $save_settings[$key] = $value;
            }
        }
        // Save this device.
        $this->dbinterface->write('sccpdevice', $save_settings, 'replace');
        // Retrieve the phone buttons from $_REQUEST ($get_settings) and write back to
        // update sccpdeviceconfig via Trigger
        $save_buttons = $this->getPhoneButtons($get_settings, $name_dev, $hw_type);
        $this->dbinterface->write('sccpbuttons', $save_buttons, $update_hw, '', $name_dev);
        // Create new XML for this device, and then reset or restart the device
        // so that it loads the file from TFT.
        $msg = "Device Saved";
        $toastFlag = 'success';
        if (!$this->createSccpDeviceXML($name_dev)){
            // will only be false if creating SIP SEP with no line.
            $msg = "Device Saved but SEP config file not created as no SIP line attached to this device";
            $toastFlag = 'warning';
        };
        $hash = '#sipdevice';
        if ($get_settings['sccp_device_typeid'] != 'sipdevice') {
            $hash = '#sccpdevice';
            // cannot restart SIP device via chan-sccp.
            if ($hw_id == 'new') {
                $this->aminterface->sccpDeviceReset($name_dev, 'reset');
            } else {
                $this->aminterface->sccpDeviceReset($name_dev, 'restart');
            }
        }
        $search = '?display=sccp_phone';
        return array('status' => true, 'message' => $msg, 'reload' => true, 'toastFlag' => $toastFlag, 'search' => $search, 'hash' => $hash);
    }
}
?>
