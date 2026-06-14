<?php

//namespace FreePBX\modules;
//  License for all code of this FreePBX module can be found in the license file inside the module directory
//  Copyright 2015 Sangoma Technologies.
// https://github.com/chan-sccp/chan-sccp/wiki/Setup-FreePBX
// http://chan-sccp-b.sourceforge.net/doc/setup_sccp.xhtml
// https://github.com/chan-sccp/chan-sccp/wiki/Conferencing
// https://github.com/chan-sccp/chan-sccp/wiki/Frequently-Asked-Questions
// http://chan-sccp-b.sourceforge.net/doc/_howto.xhtml#nf_adhoc_plar
// https://www.cisco.com/c/en/us/td/docs/voice_ip_comm/cuipph/all_models/xsi/9-1-1/CUIP_BK_P82B3B16_00_phones-services-application-development-notes/CUIP_BK_P82B3B16_00_phones-services-application-development-notes_chapter_011.html
// https://www.cisco.com/c/en/us/td/docs/voice_ip_comm/cuipph/7960g_7940g/sip/4_4/english/administration/guide/ver4_4/sipins44.html
// http://usecallmanager.nz/
/* !TODO!:
 *  + Cisco Format Mac
 *  + Model Information
 *  + Device Right Menu
 *  - Dial Templates are not really needed for skinny, skinny get's direct feed back from asterisk per digit -->
 *  - If your dialplan is finite (completely fixed length (depends on your country dialplan) dialplan, then dial templates are not required) -->
 *  - As far as i know FreePBX does also attempt to build a finite dialplan -->
 *  - Having to maintain both an asterisk dialplan and these skinny dial templates is annoying -->
 *  + Dial Templates + Configuration
 *  + Dial Templates in Global Configuration ( Enabled / Disabled ; default template )
 *  ? Dial Templates - Howto IT Include in XML.Config ???????
 *  + Dial Templates - SIP Device
 *  - Dial Templates in device Configuration ( Enabled / inheret / Disabled ; template )
 *  - WiFi Config (Bulk Deployment Utility for Cisco 7921, 7925, 7926)?????
 *  + Change internal use Field to _Field (new feature in chan_sccp (added for Sccp_manager))
 *  + Delete phone XML
 *  + Change Installer  ?? (test )
 *  + Installer  Realtime config update
 *  + Installer  Adaptive DB reconfig.
 *  + Add system info page
 *  + Change Cisco Language data
 *  + Make DB Acces from separate class
 *  + Make System Acces from separate class
 *  + Make Var elements from separate class
 *  + To make creating XML files in a separate class
 *  + Add Switch to select XML schema (display)
 *  + SRST Config
 *  + secondary_dialtone_digits = ""     line config
 *  + secondary_dialtone_tone = 0x22     line config
 *  - deviceSecurityMode http://usecallmanager.nz//itl-file-tlv.html
 *  - transportLayerProtocol http://usecallmanager.nz//itl-file-tlv.html
 *  - Check Time zone ....
 *  - Failover config
 *  + Auto Addons!
 *  + DND Mode
 *  - support kv-store ?????
 *  + Shared Line
 *  - bug Soft key set (empty keysets )
 *  - bug Fix ...(K no w bug? no fix)
 *  - restore default Value on page
 *  - restore default Value on sccp.class
 *  -  'Device SEP ID.[XXXXXXXXXXXX]=MAC'
 *  +  ATA's start with       ATAXXXXXXXXXXXX.
 *  + Create ATADefault.cnf.xml
 *  - Create Second line Use MAC AABBCCDDEEFF rotation MAC BBCCDDEEFF01 (ATA 187 )
 *  +  Add SEP, ATA, VG prefix.
 *  +  Add Cisco SIP device Tftp config.
 *  -  VG248 ports start with VGXXXXXXXXXXXX0.
 *  * I think this file should be split in 3 parts (as in Model-View-Controller(MVC))
 *    * XML/Database Parts -> Model directory
 *    * Processing parts -> Controller directory
 *    * Ajax Handler Parts -> Controller directory
 *    * Result parts -> View directory
 *  + Support TFTP rewrite :
 *     + dir "settings"
 *     + dir "templates"
 *     + dir "firmware"
 *     + dir "locales"
 *  + Add error information on the server information page (critical display error - the system can not work correctly)
 *  - Add Warning Information on Server Info Page
 *  - ADD Reload Line
 *  - Add Call Map (show Current call Information)
 * ---TODO ---
 * <vendorConfig>
 *  <autoSelectLineEnable>0</autoSelectLineEnable>
 * <autoCallSelect>0</autoCallSelect>
 * </vendorConfig>
 */

namespace FreePBX\modules;

class Sccp_manager extends \FreePBX_Helpers implements \BMO {
    /* Field Values for type  seq */
    private $pagedata = null;
    private $sccp_driver_ver = '11.4';             // Ver fore SCCP.CLASS.PHP
    public $sccp_branch = 'm';                       // Ver fore SCCP.CLASS.PHP
    private $installedLangs = array();

    private $hint_context = array('default' => '@ext-local'); /// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Get it from Config !!!
    private $val_null = 'NONE'; /// REPLACE to null Field
    public $sccp_model_list = array();
    private $cnf_wr = null;
    public $sccppath = array();
    public $sccpvalues = array();
    public $sccp_conf_init = array();
    public $xml_data;
    public $class_error; //error construct
    public $info_warning;
    public $sccpHelpInfo = array();

    // Move all non sccp_manager specific functions to traits
    use \FreePBX\modules\sccp_manager\sccpManTraits\helperFunctions;
use \FreePBX\modules\sccp_manager\sccpManTraits\ajaxHelper;
use \FreePBX\modules\sccp_manager\sccpManTraits\bmoFunctions;

    public function __construct($freepbx = null) {
        if ($freepbx == null) {
            throw new \Exception("Not given a FreePBX Object");
        }
        $this->class_error = array();
        $this->FreePBX = $freepbx;
        $this->db = $freepbx->Database;
        $this->cnf_wr = \FreePBX::WriteConfig();
        $this->cnf_read = \FreePBX::LoadConfig();
        $driverNamespace = "\\FreePBX\\Modules\\Sccp_manager";
        if (class_exists($driverNamespace, false)) {
            foreach (glob(__DIR__ . "/sccpManClasses/*.class.php") as $driver) {
                if (preg_match("/\/([a-z1-9]*)\.class\.php$/i", $driver, $matches)) {
                    $name = $matches[1];
                    $class = $driverNamespace . "\\" . $name;
                    if (!class_exists($class, false)) {
                        include($driver);
                    }
                    if (class_exists($class, false)) {
                        $this->$name = new $class($this);
                    } else {
                        throw new \Exception("Invalid Class inside in the include folder" . print_r($freepbx));
                    }
                }
            }
        } else {
            return;
        }

        $this->sccpvalues = $this->dbinterface->get_db_SccpSetting(); //Initialise core settings
        $this->initializeSccpPath();  //Set required Paths
        $this->updateTimeZone();   // Get timezone from FreePBX
        //$this->findInstLangs();
        $this->saveSccpSettings();
    }

    /*
     *   Generate Input elements in Html Code from sccpgeneral.xml
     */

    public function showGroup($group_name, $show_Header, $form_prefix = 'sccp', $form_values = array()) {

        // load xml data - moved from Construct to simplify Construct.
        // TODO: This is static data so only load first time. Left as is for dbug.
        $xml_vars = __DIR__ . '/conf/sccpgeneral.xml.v433';
              $this->xml_data = simplexml_load_file($xml_vars);
        // load metainfo from chan-sccp - help information if not in xml. Only load first time as static data.
        if (empty($this->sccpHelpInfo)) {
            $sysConfiguration = $this->aminterface->getSCCPConfigMetaData('general');

            foreach ($sysConfiguration['Options'] as $key => $valueArray) {
                foreach ($valueArray['Description'] as $descKey => $descValue) {
    $optName = (string)$valueArray['Name'];
    if (!isset($this->sccpHelpInfo[$optName])) {
        $this->sccpHelpInfo[$optName] = '';
    }
    $this->sccpHelpInfo[$optName] .= $descValue . '<br>';
}
            }
            unset($sysConfiguration);
        }

        if ((array) $this->xml_data) {
            foreach ($this->xml_data->xpath('//page_group[@name="' . $group_name . '"]') as $item) {
                $htmlret = load_view(__DIR__ . '/views/formShowSysDefs.php', array(
                    'itm' => $item,
                    'h_show' => $show_Header,
                    'form_prefix' => $form_prefix,
                    'fvalues' => $form_values,
                    'installedLangs' => $this->findInstLangs(),
                    'chanSccpHelp' => $this->sccpHelpInfo,
                    'sccp_defaults' => $this->sccpvalues
                    )
                  );
            }
        } else {
            $htmlret = load_view(__DIR__ . '/views/formShowError.php');
        }
        return $htmlret;
    }

    /*
     *    Load config vars from base array
     */

    public function updateTimeZone() {
        // Get latest FreePBX time $timeZoneOffsetList
        $freepbxTZ = \date_default_timezone_get();
        $this->sccpvalues['ntp_timezone'] = array('keyword' => 'ntp_timezone', 'seq'=>95, 'type' => 2, 'data' => $freepbxTZ);
        $TZdata = $this->extconfigs->getExtConfig('sccp_timezone', $freepbxTZ);
        if (!empty($TZdata)) {
            $value = $TZdata['offset']/60;   // TODO: Is this correct (storing in hours not minutes)
            $this->sccpvalues['tzoffset'] = array('keyword' => 'tzoffset', 'seq'=>98, 'type' => 2, 'data' => $value);
        }
    }

    /*
     *  Show form information - General
     */

    public function settingsShowPage() {
        $this->checkTftpMapping();
        $request = $_REQUEST;
        $action = !empty($request['action']) ? $request['action'] : '';

        $this->pagedata = array(
            "general" => array(
                "name" => _("Site Default Settings"),
                "page" => 'views/server.setting.php'
              ),
            "sccpdevice" => array(
                "name" => _("SCCP Device"),
                "page" => 'views/server.device.php'
              ),
            "sccpurl" => array(
                "name" => _("SCCP Device URL"),
                "page" => 'views/server.url.php'
              ),
            "sccpntp" => array(
                "name" => _("SCCP Time"),
                "page" => 'views/server.datetime.php'
              ),
            "sccpcodec" => array(
                "name" => _("SCCP Codec"),
                "page" => 'views/server.codec.php'
              ),
            "sccpadv" => array(
                "name" => _("Advanced SCCP Settings"),
                "page" => 'views/server.advanced.php'
              ),
            "sccpinfo" => array(
                "name" => _("SCCP info"),
                "page" => 'views/server.info.php'
              )
            );
        // If view is set to simple, remove the ntp, codec and advanced tabs
        if (isset($this->sccpvalues['displayconfig']['data']) && ($this->sccpvalues['displayconfig']['data'] == 'sccpsimple')) {
            unset($this->pagedata['sccpntp'], $this->pagedata['sccpcodec'], $this->pagedata['sccpadv'] );
        }
        $this->processPageData();
        return $this->pagedata;
    }

    public function infoServerShowPage() {
        $request = $_REQUEST;
        $action = !empty($request['action']) ? $request['action'] : '';
        $this->pagedata = array(
            "general" => array(
                "name" => _("General SCCP Settings"),
                "page" => 'views/server.info.php'
            ),
        );
        $this->processPageData();
        return $this->pagedata;
    }

    public function advServerShowPage() {
        $request = $_REQUEST;
        $action = !empty($request['action']) ? $request['action'] : '';
        $inputform = !empty($request['tech_hardware']) ? $request['tech_hardware'] : '';
        switch ($inputform) {
            case 'dialplan':
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("SCCP Dial Plan information"),
                        "page" => 'views/form.dptemplate.php'
                    )
                );
                break;
            default:
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("SCCP Model information"),
                        "page" => 'views/advserver.model.php'
                    ),
                    "sccpkeyset" => array(
                        "name" => _("SCCP Device Keyset"),
                        "page" => 'views/advserver.keyset.php'
                    )
                );
                if ($this->sccpvalues['siptftp']['data'] == 'on') {
                    $this->pagedata["sccpdialplan"] = array(
                        "name" => _("SIP Dial Plan information"),
                        "page" => 'views/advserver.dialtemplate.php'
                    );
                }
                break;
        }

        $this->processPageData();
        return $this->pagedata;
    }

    public function phoneShowPage() {
        $request = $_REQUEST;
        $action = !empty($request['action']) ? $request['action'] : '';
        $inputform = !empty($request['tech_hardware']) ? $request['tech_hardware'] : '';
        switch ($inputform) {
            case "cisco":
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("Device configuration"),
                        "page" => 'views/form.adddevice.php'
                    ),
                    "buttons" => array(
                        "name" => _("Device Buttons"),
                        "page" => 'views/form.buttons.php'
                    ),
                    "advanced" => array(
                        "name" => _("Device SCCP Advanced"),
                        "page" => 'views/form.devadvanced.php'
                    )
                );
                break;
            case "cisco-sip":
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("Sip device configuration"),
                        "page" => 'views/form.adddevice.php'
                    ),
                    "buttons" => array(
                        "name" => _("Sip device Buttons"),
                        "page" => 'views/form.buttons.php'
                    )
                );
                break;

            case "r_user":
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("Roaming User configuration"),
                        "page" => 'views/form.addruser.php'
                    ),
                    "buttons" => array(
                        "name" => _("Device Buttons"),
                        "page" => 'views/form.buttons.php'
                    ),
                );
                break;

            default:
                $this->pagedata = array(
                    "general" => array(
                        "name" => _("SCCP Extension"),
                        "page" => 'views/hardware.extension.php'
                    ),
                    "sccpdevice" => array(
                        "name" => _("SCCP Phone"),
                        "page" => 'views/hardware.phone.php'
                    )
                );
                if ($this->sccpvalues['siptftp']['data'] == 'on') {
                    $this->pagedata["sipdevice"] = array(
                        "name" => _("SIP CISCO Phone"),
                        "page" => 'views/hardware.sphone.php'
                    );
                }
                break;
        }
        $this->processPageData();
        return $this->pagedata;
    }

    public function processPageData() {
        foreach ($this->pagedata as &$page) {
            ob_start();
            include($page['page']);
            $page['content'] = ob_get_contents();
            ob_end_clean();
        }
    }

    /*
     *
     * *  Save Hardware Device Information to Db + ???? Create / update XML Profile
     *
     */

    function getPhoneButtons($get_settings, $ref_id = '', $ref_type = 'sccpdevice') {
        // get Model Buttons info
        $res = array();
        $def_feature = array('parkinglot' => array('name' => 'P.slot', 'value' => 'default'),
            'devstate' => array('name' => 'Coffee', 'value' => 'coffee'),
            'monitor' => array('name' => 'Record Calls', 'value' => '')
        );

        // $lines_list = $this->dbinterface->getSccpDeviceTableData('SccpExtension');
        $max_btn = (!empty($get_settings['buttonscount']) ? $get_settings['buttonscount'] : 60);

        for ($it = 0; $it < $max_btn; $it++) {
            if (!empty($get_settings["button{$it}_type"])) {
                $btn_t = $get_settings["button{$it}_type"];
                $btn_n = '';
                $btn_opt = '';
                if ($it == 0) {
                    $btn_opt = 'default';
					$default_context = isset($this->hint_context['default']) ? $this->hint_context['default'] : '';
                }
                switch ($btn_t) {
                    case 'feature':
                        $btn_f = $get_settings["button{$it}_feature"];
                        // $btn_opt = (empty($get_settings['button' . $it . '_fvalue'])) ? '' : $get_settings['button' . $it . '_fvalue'];
                        $btn_n = (empty($get_settings["button{$it}_flabel"])) ? $def_feature[$btn_f]['name'] : $get_settings["button{$it}_flabel"];
                        $btn_opt = $btn_f;
                        if (!empty($def_feature[$btn_f]['value'])) {
                            if (empty($get_settings['button' . $it . '_fvalue'])) {
                                $btn_opt .= ',' . $def_feature[$btn_f]['value'];
                            } else {
                                $btn_opt .= ',' . $get_settings['button' . $it . '_fvalue'];
                            }
                            if ($btn_f == 'parkinglot') {
                                if (!empty($get_settings['button' . $it . '_retrieve'])) {
                                    $btn_opt .= ',RetrieveSingle';
                                }
                            }
                        }

                        break;
                    case 'monitor':
                        $btn_t = 'speeddial';
                        $btn_opt = (string) $get_settings["button{$it}_line"];
                        $db_res = $this->dbinterface->getSccpDeviceTableData('SccpExtension', array('name' => $btn_opt));
                        $btn_n = $db_res[0]['label'];
                        $btn_opt .= ',' . $btn_opt . $default_context;
                        break;
                    case 'speeddial':
                        if (!empty($get_settings["button{$it}_input"])) {
                            $btn_n = $get_settings["button{$it}_input"];
                        }
                        if (!empty($get_settings["button{$it}_phone"])) {
                            $btn_opt = $get_settings["button{$it}_phone"];
                            if (empty($btn_n)) {
                                $btn_n = $btn_opt;
                            }
                        }

                        if (!empty($get_settings["button{$it}_hint"])) {
                            if ($get_settings["button{$it}_hint"] == "hint") {
                                if (empty($btn_n)) {
                                    $btn_t = 'line';
                                    $btn_n = $get_settings["button{$it}_hline"] . '!silent';
                                    $btn_opt = '';
                                } else {
                                    // $btn_opt .= ',' . $get_settings['button' . $it . '_hline'] . $this->hint_context['default'];
                                    $btn_opt .= ',' . $get_settings["button{$it}_hline"];
                                }
                            }
                        }
                        break;
                    case 'adv.line':
                        $btn_t = 'line';
                        $btn_n = (string) $get_settings["button{$it}_line"];
                        $btn_n .= '@' . (string) $get_settings["button{$it}_advline"];
                        $btn_opt = (string) $get_settings["button{$it}_advopt"];

                        break;
                    case 'line':
                    case 'silent':
                        if (isset($get_settings["button{$it}_line"])) {
                            $btn_n = (string) $get_settings["button{$it}_line"];
                            if ($it > 0) {
                                if ($btn_t == 'silent') {
                                    $btn_n .= '!silent';
                                    $btn_t = 'line';
                                }
                            }
                        } else {
                            $btn_t = 'empty';
                            $btn_n = '';
                        }
                        break;
                    case 'empty':
                        $btn_t = 'empty';
                        break;
                }
                if (!empty($btn_t)) {
                    $res[] = array('ref' => $ref_id, 'reftype' => $ref_type, 'instance' => (string) ($it + 1), 'buttontype' => $btn_t, 'name' => $btn_n, 'options' => $btn_opt);
                }
            }
        }
        return $res;
    }

    function handleRoamingUsers($get_settings, $validateonly = false) {
        $hdr_prefix = 'sccp_ru_';
        $hdr_arprefix = 'sccp_ru-ar_';

        $save_buttons = array();
        $save_settings = array();
        $name_dev = '';
        $db_field = $this->dbinterface->getSccpDeviceTableData("get_columns_sccpuser");
        $hw_prefix = 'SEP';
        $name_dev = $get_settings[$hdr_prefix . 'id'];
        $save_buttons = $this->getPhoneButtons($get_settings, $name_dev, 'sccpline');

        foreach ($db_field as $data) {
            $key = (string) $data['Field'];
            $value = "";
            switch ($key) {
                case 'name':
                    $value = $name_dev;
                    break;
                default:
                    if (!empty($get_settings[$hdr_prefix . $key])) {
                        $value = $get_settings[$hdr_prefix . $key];
                    }
                    if (!empty($get_settings[$hdr_arprefix . $key])) {
                        $arr_data = '';
                        $arr_clear = false;
                        foreach ($get_settings[$hdr_arprefix . $key] as $vkey => $vval) {
                            $tmp_data = '';
                            foreach ($vval as $vkey => $vval) {
                                switch ($vkey) {
                                    case 'inherit':
                                        if ($vval == 'on') {
                                            $arr_clear = true;
                                            // Злобный ХАК ?!TODO!?
                                            if ($key == 'permit') {
                                                $save_settings['deny'] = 'NONE';
                                            }
                                        }
                                        break;
                                    case 'internal':
                                        if ($vval == 'on') {
                                            $tmp_data .= 'internal;';
                                        }
                                        break;
                                    default:
                                        $tmp_data .= $vval . '/';
                                        break;
                                }
                            }
                            if (strlen($tmp_data) > 2) {
                                while (substr($tmp_data, -1) == '/') {
                                    $tmp_data = substr($tmp_data, 0, -1);
                                }
                                $arr_data .= $tmp_data . ';';
                            }
                        }
                        while (substr($arr_data, -1) == ';') {
                            $arr_data = substr($arr_data, 0, -1);
                        }
                        if ($arr_clear) {
                            $value = 'NONE';
                        } else {
                            $value = $arr_data;
                        }
                    }
            }
            if (!empty($value)) {
                $save_settings[$key] = $value;
            }
        }
        $this->dbinterface->write('sccpuser', $save_settings, 'replace', 'name');
        $this->dbinterface->write('sccpbuttons', $save_buttons, 'delete', '', $name_dev); //standardise to delete
        return $save_buttons;
    }

    public function getCodecs($type, $showDefaults = false) {
        $allSupported = array();
        $sccpCodec = array_fill_keys(array('alaw', 'ulaw', 'g722', 'g723', 'g726', 'g729', 'gsm', 'h264', 'h263', 'h261'),0);
        // First see if have any site defaults
        $val = $this->sccpvalues['allow']['data'];
        if (empty($val)) {
            // No site defaults so return chan-sccp defaults
            $val = $this->sccpvalues['allow']['systemdefault'];
        }
        $siteCodecs = array_fill_keys(explode(';',$val), 1);
        switch ($type) {
            case 'audio':
                $fpbxCodecs = $this->FreePBX->Codecs->getAudio();
                break;
            case 'video':
                $fpbxCodecs = $this->FreePBX->Codecs->getVideo();
                break;
            case 'text':
                $siteCodecs = $this->getConfig('textcodecs');
                $fpbxCodecs = $this->FreePBX->Codecs->getText(true);
                break;
            case 'image':
                $siteCodecs = $this->getConfig('imagecodecs');
                $fpbxCodecs = $this->FreePBX->Codecs->getImage(true);
                break;
        }
        // Below could be squashed to 1 line, but would be unreadable.
        // These have value set to 1
        $enabledCodecs = array_intersect_key($siteCodecs, $sccpCodec, $fpbxCodecs);
        // These have value set to 0
        $allSupported = array_intersect_key($sccpCodec,$fpbxCodecs);
        $disabledCodecs = array_diff_key($allSupported,$enabledCodecs);
        $codecs = array_merge($enabledCodecs, $disabledCodecs);

        return $codecs;
    }

    /**
     * Retrieve Active Codecs
     * return finds Language pack
     */

    private function findInstLangs() {
        //locales and country tones are installed in the tftp_lang_path
        //Available packs from provisioner are in masterFilesStructure.xml in tftpRoot Path

        $searchDir = '/';        //set default for when called by installer on virgin system
        $result = array();

        if (!file_exists("{$this->sccppath['tftp_path']}/masterFilesStructure.xml")) {
            if (!$this->getFileListFromProvisioner($this->sccpvalues['tftp_path']['data'])) {
                // File does not exist and cannot get from internet.
                return $result;
            };
        }
        $tftpBootXml = simplexml_load_file("{$this->sccppath['tftp_path']}/masterFilesStructure.xml");

        foreach (array('languages', 'countries') as $pack) {
            switch ($pack) {
                case 'languages':
                    if (!empty($this->sccppath['tftp_lang_path'])) {
                        $searchDir = $this->sccppath['tftp_lang_path'];
                    }
                    $simpleXmlArr = $tftpBootXml->xpath("//Directory[@name='languages']//DirectoryPath[contains(.,'languages/')]");
                    array_shift($simpleXmlArr); // First element is the parent directory
                    foreach ($simpleXmlArr as $rowIn) {
                        $tmpArr = explode('/',(string)$rowIn);
                        array_pop($tmpArr);   //last element is empty
                        $result[$pack]['available'][] = array_pop($tmpArr);
                    }
                    $fileToFind = 'be-sccp.jar';  // This file should exist if the locale is populated
                    break;
                case 'countries':
                    if (!empty($this->sccppath["tftp_countries_path"])) {
                        $searchDir = $this->sccppath['tftp_countries_path'];
                    }
                    $simpleXmlArr = $tftpBootXml->xpath("//Directory[@name='countries']//DirectoryPath[contains(.,'countries/')]");
                    array_shift($simpleXmlArr); // First element is the parent directory
                    foreach ($simpleXmlArr as $rowIn) {
                        $tmpArr = explode('/',(string)$rowIn);
                        array_pop($tmpArr);   //last element is empty
                        $result[$pack]['available'][] = array_pop($tmpArr);
                    }
                    $fileToFind = 'g3-tones.xml';  // This file should exist if the locale is populated
                    break;
            }

            foreach (array_diff(scandir($searchDir),array('.', '..')) as $subDir) {
                if (is_dir($searchDir . DIRECTORY_SEPARATOR . $subDir)) {
                    $filename = $searchDir . DIRECTORY_SEPARATOR . $subDir . DIRECTORY_SEPARATOR . $fileToFind;
                    if (file_exists($filename)) {
                        $result[$pack]['have'][] = $subDir;
                    }
                }
            }
        }
        $this->installedLangs = $result;
        return $result;
    }

    /*
     *    Check tftp/xml file path and permissions
     */

    private function initializeTFtpLanguagePath() {
        //$dir = $this->sccppath["tftp_lang_path"];
        foreach ($this->extconfigs->getExtConfig('sccp_lang') as $langKey => $langValueArr) {
            $localeDir = $this->sccppath["tftp_lang_path"] . DIRECTORY_SEPARATOR . $langValueArr['locale'];
            if (!is_dir($localeDir)) {
                if (!mkdir($localeDir, 0755, true)) {
                    die("Error creating $localeDir directory");
                }
            }
        }
    }

    /*
     *    Check file paths and permissions
     */

    function initializeSccpPath() {
        $this->sccppath = array(
                    'asterisk' => $this->sccpvalues['asterisk_etc_path']['data'] ?? '/etc/asterisk',
            'tftp_path' => $this->sccpvalues['tftp_path']['data'] ?? '/tftpboot',
            'tftp_templates_path' => $this->sccpvalues['tftp_templates_path']['data'] ?? '/tftpboot/templates',
            'tftp_store_path' => $this->sccpvalues['tftp_store_path']['data'] ?? '/tftpboot',
            'tftp_lang_path' => $this->sccpvalues['tftp_lang_path']['data'] ?? '/tftpboot/languages',
            'tftp_firmware_path' => $this->sccpvalues['tftp_firmware_path']['data'] ?? '/tftpboot',
            'tftp_dialplan_path' => $this->sccpvalues['tftp_dialplan_path']['data'] ?? '/tftpboot',
            'tftp_softkey_path' => $this->sccpvalues['tftp_softkey_path']['data'] ?? '/tftpboot',
            'tftp_countries_path' => $this->sccpvalues['tftp_countries_path']['data'] ?? '/tftpboot/countries'
                  );

        // initialise $sccp_conf_init
        $this->sccp_conf_init = $this->initialiseConfInit();

        $hint = $this->aminterface->core_list_hints();
        foreach ($hint as $key => $value) {
            if ($this->hint_context['default'] != $value) {
                $this->hint_context[$key] = $value;
            }
        }
    }

    /*
     *      Soft Key
     */

    function createSccpXmlSoftkey() {
        foreach ($this->aminterface->sccp_list_keysets() as $keyl => $vall) {
            $this->xmlinterface->create_xmlSoftkeyset($this->sccp_conf_init, $this->sccppath, $keyl);
        }
    }

    /*
     *      DialPlan
     */

    function getDialPlanList() {
        $dir = $this->sccppath["tftp_dialplan_path"] . '/dial*.xml';
        $base_len = strlen($this->sccppath["tftp_dialplan_path"]) + 1;
        $res = glob($dir);
        foreach ($res as $key => $value) {
            $res[$key] = array('id' => substr($value, $base_len, -4), 'file' => substr($value, $base_len));
        }
        return $res;
    }

    function getDialPlan($get_file) {
        $file = $this->sccppath["tftp_dialplan_path"] . '/' . $get_file . '.xml';
        if (file_exists($file)) {

            $fileContents = file_get_contents($file);
            $fileContents = str_replace(array("\n", "\r", "\t"), '', $fileContents);
            $fileContents = trim(str_replace('"', "'", $fileContents));
            $fileContents = strtolower($fileContents);
            $res = (array) simplexml_load_string($fileContents);
        }
        return $res;
    }

    function deleteDialPlan($get_file) {
        $file = $this->sccppath["tftp_dialplan_path"] . '/' . $get_file . '.xml';
        if (file_exists($file)) {
            $res = unlink($file);
        }
        return $res;
    }

    function saveDialPlan($get_settings) {

        $confDir = $this->sccppath["tftp_dialplan_path"];
        return $this->xmlinterface->saveDialPlan($confDir, $get_settings);
    }

    /*
     *      Update buttons Labels on mysql DB
     */

    private function updateSccpButtons($hw_list = array()) {

        $save_buttons = array();
        $button_list = array();
        if (!empty($hw_list)) {
            foreach ($hw_list as $value) {
                $button_tmp = (array)$this->dbinterface->getSccpDeviceTableData("get_sccpdevice_buttons", array('buttontype' => 'speeddial', 'id' => $value['name']));
                $button_list = array_merge($button_list, $button_tmp);
            }
        } else {
            $button_list = $this->dbinterface->getSccpDeviceTableData("get_sccpdevice_buttons", array('buttontype' => 'speeddial'));
        }
        if (empty($button_list)) {
            return array('Response' => ' 0 buttons found ', 'data' => '');
        }
        $copy_fld = array('ref', 'reftype', 'instance', 'buttontype', 'options');
        $extList = $extList = $this->dbinterface->get_db_SccpTableByID("SccpExtension", array(), 'name');
        foreach ($button_list as $value) {
            $btn_opt = explode(',', $value['options']);
            $btn_id = $btn_opt[0];
            if (!empty($extList[$btn_id])) {
                if ($extList[$btn_id]['label'] != $value['name']) {
                    $btn_data['name'] = $extList[$btn_id]['label'];
                    foreach ($copy_fld as $ckey) {
                        $btn_data[$ckey] = $value[$ckey];
                    }
                    $save_buttons[] = $btn_data;
                }
            }
        }
        if (empty($save_buttons)) {
            return array('Response' => 'No update required', 'data' => ' 0 - records ');
        }
        $res = $this->dbinterface->write('sccpbuttons', $save_buttons, 'replace', '', '');
        return array('Response' => 'Update records :' . count($save_buttons), 'data' => $res);
    }

    /*
     *      Save Config Value to mysql DB
     */

    private function saveSccpSettings($save_value = array()) {

        if (empty($save_value)) {
            $this->dbinterface->write('sccpsettings', $this->sccpvalues, 'replace'); //Change to replace as clearer
        } else {
            $this->dbinterface->write('sccpsettings', $save_value, 'update');
        }
        return true;
    }

    /*
     *          Create XMLDefault.cnf.xml
     */

    function createDefaultSccpXml() {
        $data_value = array();
        foreach ($this->sccpvalues as $key => $value) {
            $data_value[$key] = $value['data'];
        }
        $data_value['server_if_list'] = $this->getIpInformation('ip4');
        $model_information = $this->getSccpModelInformation($get = "enabled", $validate = false); // Get Active

        if (empty($model_information)) {
            $model_information = $this->getSccpModelInformation($get = "all", $validate = false); // Get All
        }

        $lang_data = $this->extconfigs->getExtConfig('sccp_lang');
        $data_value['tftp_path'] = $this->sccppath["tftp_path"];

        $this->xmlinterface->create_default_XML($this->sccppath["tftp_store_path"], $data_value, $model_information, $lang_data);
    }

    /*
     *          Create  (SEP) dev_ID.cnf.xml
     */

    function createSccpDeviceXML(string $dev_id) {

        $sccp_native = true;
        $data_value = array();
        $dev_line_data = null;

        $dev_config = $this->dbinterface->getSccpDeviceTableData("get_sccpdevice_byid", array('id' => $dev_id));
        // Support Cisco Sip Device
        if (!empty($dev_config['type'])) {
            if (strpos($dev_config['type'], 'sip') !== false) {
                $sccp_native = false;
                $tmp_bind = $this->getSipConfig();
                $dev_ext_config = $this->dbinterface->getSccpDeviceTableData("SccpDevice", array('name' => $dev_id, 'fields' => 'sip_ext'));
                if (empty($dev_ext_config)){
                    // TODO: Placeholder. Have no associated sip line so cannot generate SEP Xml for SIP.
                    // Need to return and inform user
                    return false;
                }
                $data_value = array_merge($data_value, $dev_ext_config);
                $data_tmp = explode(';', $dev_ext_config['sip_lines']);
                $data_value['sbind'] = array();
                foreach ($data_tmp as $value) {
                    $tmp_line = explode(',', $value);
                    switch ($tmp_line[0]) {
                        case 'line':
                            $dev_line_data = $this->dbinterface->getSipTableData('DeviceById', $tmp_line[1]);
                            $f_linetype = ($dev_line_data['sipdriver'] == 'chan_sip') ? 'sip' : 'pjsip';
                            $dev_line_data['sbind'] = $tmp_bind[$f_linetype];
                            if ((!$this->array_key_exists_recursive('udp', $tmp_bind[$f_linetype])) && (!$this->array_key_exists_recursive('tcp', $tmp_bind[$f_linetype]))) {
                                die_freepbx(_("SIP server configuration error ! Neither UDP nor TCP protocol enabled"));
                                return false;
                            }
                            if (!empty($dev_line_data)) {
                                $data_value['siplines'][] = $dev_line_data;
                            }
                            if ($tmp_line[2] == 'default') {
                                $data_value['sbind'] = $tmp_bind[$f_linetype];
                            }
                            break;
                        case 'speeddial':
                            $data_value['speeddial'][] = array("name" => $tmp_line[1], "dial" => $tmp_line[2]);
                            break;
                        default:
                            $data_value['sipfunctions'][] = $tmp_line;
                            break;
                    }
                }
            }
        }
        foreach ($this->sccpvalues as $key => $value) {
            $data_value[$key] = $value['data'];
        }
        //Get Cisco Code only Old Device
        $data_value['ntp_timezone_id'] = $this->extconfigs->getExtConfig('sccp_timezone', $data_value['ntp_timezone']); // Old Cisco Device
        // $data_value['ntp_timezone_id'] = $data_value['ntp_timezone']; // New Cisco Device !
        // $data_value['ntp_timezone_id'] = // SPA Cisco Device !
        $data_value['server_if_list'] = $this->getIpInformation('ip4');
        $dev_config = array_merge($dev_config, $this->sccppath);
        $dev_config['tftp_firmware'] = '';
        $dev_config['addon_info'] = array();
        if (!empty($dev_config['addon'])) {
            $hw_addon = explode(',', $dev_config['addon']);
            foreach ($hw_addon as $key) {
                $hw_data = $this->getSccpModelInformation('byid', false, "all", array('model' => $key));
                $dev_config['addon_info'][$key] = $hw_data[0]['loadimage'];
            }
        }

        $lang_data = $this->extconfigs->getExtConfig('sccp_lang');
        if (!$sccp_native) {
            return $this->xmlinterface->create_SEP_SIP_XML($this->sccppath["tftp_store_path"], $data_value, $dev_config, $dev_id, $lang_data);
        }
        return $this->xmlinterface->create_SEP_XML($this->sccppath["tftp_templates_path"], $data_value, $dev_config, $dev_id, $lang_data);
    }

    function deleteSccpDeviceXML($dev_id = '') {
        if (empty($dev_id)) {
            return false;
        }
        if ($dev_id == 'all') {
            $xml_name = $this->sccppath["tftp_store_path"] . '/SEP*.cnf.xml';
            array_map("unlink", glob($xml_name));
            $xml_name = $this->sccppath["tftp_store_path"] . '/ATA*.cnf.xml';
            array_map("unlink", glob($xml_name));
            $xml_name = $this->sccppath["tftp_store_path"] . '/VG*.cnf.xml';
            array_map("unlink", glob($xml_name));
        } else {
            if (!strpos($dev_id, 'SEP')) {
                return false;
            }
            $xml_name = $this->sccppath["tftp_store_path"] . '/' . $dev_id . '.cnf.xml';
            if (file_exists($xml_name)) {
                unlink($xml_name);
            }
        }
    }

    private function createSccpBackup() {
        global $amp_conf;
        $dir_info = array();
        $backup_files = array($amp_conf['ASTETCDIR'] . '/sccp', $amp_conf['ASTETCDIR'] . '/extensions', $amp_conf['ASTETCDIR'] . '/extconfig',
            $amp_conf['ASTETCDIR'] . '/res_config_mysql', $amp_conf['ASTETCDIR'] . '/res_mysql');
        $backup_ext = array('.conf', '_additional.conf', '_custom.conf');
        $backup_info = $this->sccppath["tftp_path"] . '/sccp_dir.info';

        $result = $this->dbinterface->dump_sccp_tables($this->sccppath["tftp_path"], $amp_conf['AMPDBNAME'], $amp_conf['AMPDBUSER'], $amp_conf['AMPDBPASS']);
        $dir_info['asterisk'] = $this->findAllFiles($amp_conf['ASTETCDIR']);
        $dir_info['tftpdir'] = $this->findAllFiles($this->sccppath["tftp_path"]);
        $dir_info['driver'] = $this->FreePBX->Core->getAllDriversInfo();
        $dir_info['core'] = $this->aminterface->getSCCPVersion();
        $dir_info['realtime'] = $this->aminterface->getRealTimeStatus();
        $dir_info['extconfigs'] = $this->extconfigs->info();
        $dir_info['dbinterface'] = $this->dbinterface->info();
        $dir_info['XML'] = $this->xmlinterface->info();

        $fh = fopen($backup_info, 'w');
        $dir_str = "Begin JSON data ------------\r\n";
        fwrite($fh, $dir_str);
        fwrite($fh, json_encode($dir_info));
        $dir_str = "\r\n\r\nBegin TEXT data ------------\r\n";
        foreach ($dir_info['asterisk'] as $data) {
            $dir_str .= $data . "\r\n";
        }
        foreach ($dir_info['tftpdir'] as $data) {
            $dir_str .= $data . "\r\n";
        }
        fputs($fh, $dir_str);
        fclose($fh);

        $zip = new \ZipArchive();
        $filename = $result . "." . gethostname() . ".zip";
        if ($zip->open($filename, \ZIPARCHIVE::CREATE)) {
            $zip->addFile($result);
            $zip->addFile($backup_info);
            foreach ($backup_files as $file) {
                foreach ($backup_ext as $b_ext) {
                    if (file_exists($file . $b_ext)) {
                        $zip->addFile($file . $b_ext);
                    }
                }
            }
            $zip->close();
        }
        unlink($backup_info);
        unlink($result);
        return $filename;
    }

    function getSccpModelInformation($get = "all", $validate = false, $format_list = "all", $filter = array()) {
        $file_ext = array('.loads', '.sbn', '.bin', '.zup', '.sbin', '.SBN', '.LOADS');
        $dir = $this->sccppath['tftp_firmware_path'];

        $search_mode = $this->sccpvalues['tftp_rewrite']['data'];
        switch ($search_mode) {
            case 'pro':
            case 'on':
            case 'internal':
                $dir_list = $this->findAllFiles($dir, $file_ext, 'fileBaseName');
                break;
            case 'off':
            default: // Place in root TFTP dir
                $dir_list = $this->findAllFiles($dir, $file_ext, 'dirFileBaseName');
                break;
        }
        $modelList = $this->dbinterface->getDb_model_info($get, $format_list, $filter);
        if ($validate) {
            foreach ($modelList as &$raw_settings) {
                if (!empty($raw_settings['loadimage'])) {
                    $raw_settings['validate'] = 'no;';
                    switch ($search_mode) {
                        case 'pro':
                        case 'on':
                        case 'internal':
                            if (in_array($raw_settings['loadimage'], $dir_list, true)) {
                                $raw_settings['validate'] = 'yes;';
                            }
                            break;
                        case 'internal2':
                            break;
                        case 'off':
                        default: // Place in root TFTP dir
                            if (in_array("{$dir}/{$raw_settings['loadimage']}", $dir_list, true)) {
                                $raw_settings['validate'] = 'yes;';
                            }
                            break;
                    }
                } else {
                    $raw_settings['validate'] = '-;';
                }
                if (!empty($raw_settings['nametemplate'])) {
                    $file = $this->sccppath['tftp_templates_path'] . '/' . $raw_settings['nametemplate'];
                    if (file_exists($file)) {
                        $raw_settings['validate'] .= 'yes';
                    } else {
                        $raw_settings['validate'] .= 'no';
                    }
                } else {
                    $raw_settings['validate'] .= '-';
                }
            }
        }
        unset($raw_settings);   // passed as reference so must unset.
        return $modelList;
    }

    function getHintInformation($sort = true, $filter = array()) {
        $res = array();
        $default_hint = '@ext-local';

        if (empty($res)) {
            // Old Req get all hints
            $tmp_data = $this->aminterface->core_list_all_hints();
            foreach ($tmp_data as $value) {
                $res[$value] = array('key' => $value, 'exten' => $this->before('@', $value), 'label' => $value);
            }
        }

        // Update info from sccp_db
        $tmp_data = $this->dbinterface->getSccpDeviceTableData('SccpExtension');
        foreach ($tmp_data as $value) {
            $name_l = $value['name'];
            if (isset($res[$name_l . $default_hint]) && !empty($res[$name_l . $default_hint])) {
                $res[$name_l . $default_hint]['exten'] = $name_l;
                $res[$name_l . $default_hint]['label'] = $value['label'];
            } else {
                // if not exist in system hints ..... ???????
                $res[$name_l . $default_hint] = array('key' => $name_l . $default_hint, 'exten' => $name_l, 'label' => $value['label']);
            }
        }
        if (!$sort) {
            return $res;
        }

        foreach ($res as $key => $value) {
            $data_sort[$value['exten']] = $key;
        }
        ksort($data_sort);
        foreach ($data_sort as $key => $value) {
            $res_sort[$value] = $res[$value];
        }

        // Update info from sip DB
        /* !TODO!: Update Hint info from sip DB ??? */
        return $res_sort;
    }
}
?>
