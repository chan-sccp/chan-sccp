<?php

/**
 *
 * Core Comsnd Interface
 *
 *
 */

namespace FreePBX\modules\Sccp_manager;

class dbinterface
{

    private $val_null = 'NONE'; /// REPLACE to null Field
	public $paren_class;
    public $db;

    public function __construct($parent_class = null)
    {
        $this->paren_class = $parent_class;
        $this->db = \FreePBX::Database();
    }

    public function info()
    {
        $Ver = '14.0.0.1';    // This should be updated
        return array('Version' => $Ver,
            'about' => 'Data access interface ver: ' . $Ver);
    }

    /*
     * Core Access Function
     */
    public function get_db_SccpTableByID($dataid, $data = array(), $indexField = '')
    {
        $result = array();
        $raw = $this->getSccpDeviceTableData($dataid, $data);
        if (empty($raw) || empty($indexField)) {
            return $raw;
        }
        foreach ($raw as $value) {
            $id = $value[$indexField];
            $result[$id] = $value;
        }
        return $result;
    }

    public function getSccpDeviceTableData(string $dataid, $data = array())
    {
        // $stmt is a single row fetch, $stmts is a fetchAll while stmtU is fetchAll UNIQUE
        $stmt = '';
        $stmts = '';
        $stmtU = '';

        switch ($dataid) {
            case 'extGrid':
                // only called by getExtensionGrid from hardware.extension.php view
                $stmts = $this->db->prepare("SELECT sccpline.name, sccpline.label, sccpbuttonconfig.ref AS mac, '-|-' AS line_status
                              FROM sccpline LEFT JOIN sccpbuttonconfig
                              ON sccpline.name = TRIM(TRAILING '!silent' FROM sccpbuttonconfig.name) ORDER BY sccpline.name");
                break;
            case 'SccpExtension':
                if (empty($data['name'])) {
                    $stmtU = $this->db->prepare('SELECT name, sccpline.* FROM sccpline ORDER BY name');
                } else {
                    $stmts = $this->db->prepare('SELECT * FROM sccpline WHERE name = :name');
                    $stmts->bindParam(':name', $data['name'],\PDO::PARAM_STR);
                }
                break;
            case 'phoneGrid':
                switch ($data['type']) {
                    case "cisco-sip":
                        $stmts = $this->db->prepare("SELECT name, type, button, addon, description, 'not connected' AS status, '- -' AS address, 'N' AS new_hw
                            FROM sccpdeviceconfig WHERE type LIKE '%-sip' ORDER BY name");
                        break;
                    case "sccp":      // Fall through to default intentionally
                    default:
                        $stmts = $this->db->prepare("SELECT name, type, button, addon, description, 'not connected' AS status, '- -' AS address, 'N' AS new_hw
                            FROM sccpdeviceconfig WHERE type not LIKE '%-sip' ORDER BY name");
                        break;
                }
                break;
            case 'SccpDevice':
                if (empty($data['fields'])) {
                    $fld = 'name, name as mac, type, button, addon, description';
                } else {
                    switch ($data['fields']) {
                        case "all":
                            $fld ='*';
                            break;
                        case "sip_ext":
                            $fld ='button as sip_lines, description as description, addon';
                            break;
                        default:
                            $fld = $data['fields'];
                            break;
                    }
                }
                if (!empty($data['name'])) {      //either filter by name or by type
                    $stmt = $this->db->prepare('SELECT ' . $fld . ' FROM sccpdeviceconfig WHERE name = :name  ORDER BY name');
                    $stmt->bindParam(':name', $data['name'],\PDO::PARAM_STR);
                } elseif (!empty($data['type'])) {
                    switch ($data['type']) {
                        case "cisco-sip":
                            $stmts = $this->db->prepare("SELECT {$fld} FROM sccpdeviceconfig WHERE TYPE LIKE '%-sip' ORDER BY name");
                            break;
                        case "cisco":      // Fall through to default intentionally
                        default:
                            $stmts = $this->db->prepare("SELECT {$fld} FROM sccpdeviceconfig WHERE TYPE not LIKE '%-sip' ORDER BY name");
                            break;
                    }
                } else {      //no filter and no name provided - return all
                    $stmts = $this->db->prepare("SELECT  {$fld}  FROM sccpdeviceconfig ORDER BY name");
                }
                break;
            case 'get_columns_sccpdevice':
                $stmtU = $this->db->prepare('DESCRIBE sccpdevice');
                break;
            case 'get_columns_sccpuser':
                $stmts = $this->db->prepare('DESCRIBE sccpuser');
                break;
            case 'get_columns_sccpline':
                $stmtU = $this->db->prepare('DESCRIBE sccpline');
                break;
            case 'get_sccpdevice_byid':
                $stmt = $this->db->prepare('SELECT t1.*, types.dns,  types.buttons, types.loadimage, types.nametemplate as nametemplate,
                        addon.buttons as addon_buttons FROM sccpdevice AS t1
                        LEFT JOIN sccpdevmodel as types ON t1.type=types.model
                        LEFT JOIN sccpdevmodel as addon ON t1.addon=addon.model WHERE name = :name');
                $stmt->bindParam(':name', $data['id'],\PDO::PARAM_STR);
                break;
            case 'get_sccpuser':
                $stmt = $this->db->prepare('SELECT * FROM sccpuser WHERE name = :name');
                $stmt->bindParam(':name', $data['id'],\PDO::PARAM_STR);
                break;
            case 'getAssignedExtensions':
                // all extensions that are designed as default lines
                $stmtU = $this->db->prepare("SELECT DISTINCT name, name FROM sccpbuttonconfig WHERE buttontype = 'line' AND instance =1");
                break;
            case 'getDefaultLine':
                $stmt = $this->db->prepare("SELECT name FROM sccpbuttonconfig WHERE ref = '{$data['id']}' and instance =1 and buttontype = 'line'");
                break;
            case 'get_sccpdevice_buttons':
                $sql = '';
                if (!empty($data['buttontype'])) {
                    $sql .= 'buttontype = :buttontype';
                }
                if (!empty($data['id'])) {
                    $sql .= (empty($sql)) ? 'ref = :ref' : ' and ref = :ref';
                }
                if (!empty($sql)) {
                    $stmts = $this->db->prepare("SELECT * FROM sccpbuttonconfig WHERE {$sql} ORDER BY instance");
                    // Now bind labels - only bind label if it exists or bind will create exception.
                    // can only bind once have prepared, so need to test again.
                    if (!empty($data['buttontype'])) {
                        $stmts->bindParam(':buttontype', $data['buttontype'],\PDO::PARAM_STR);
                    }
                    if (!empty($data['id'])) {
                        $stmts->bindParam(':ref', $data['id'],\PDO::PARAM_STR);
                    }
                } else {
                    $raw_settings = array();
                }
                break;
                // No default case so will give exception of $raw_settings undefined if the
                // dataid is not in the switch.
        }
        if (!empty($stmt)) {
            $stmt->execute();
            $raw_settings = $stmt->fetch(\PDO::FETCH_ASSOC);
        } elseif (!empty($stmts)) {
            $stmts->execute();
            $raw_settings = $stmts->fetchAll(\PDO::FETCH_ASSOC);
        } elseif (!empty($stmtU)) {
            //returns an assoc array indexed on first field
          $stmtU->execute();
          $raw_settings = $stmtU->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        }
        return $raw_settings;
    }

    public function get_db_SccpSetting()
    {
        $stmt = $this->db->prepare('SELECT keyword, sccpsettings.* FROM sccpsettings ORDER BY type, seq');
        $stmt->execute();
        $settingsFromDb = $stmt->fetchAll(\PDO::FETCH_ASSOC|\PDO::FETCH_UNIQUE);
        return $settingsFromDb;
    }

    public function get_db_sysvalues()
    {
        $stmt = $this->db->prepare('SHOW VARIABLES LIKE \'%group_concat%\'');
        $stmt->execute();
        return $stmt->fetch(\PDO::FETCH_ASSOC);
    }

    /*
     *      Get Sccp Device Model information
     */

    function getDb_model_info($get = 'all', $format_list = 'all', $filter = array())
    {
        $sel_inf = '*, 0 as validate';
        if ($format_list === 'model') {
            $sel_inf = "model, vendor, dns, buttons, '-;-' as validate";
        }
        switch ($get) {
            case 'byciscoid':
                if (!empty($filter)) {
                    if (!empty($filter['model'])) {
                        if (!strpos($filter['model'], 'loadInformation')) {
                            $filter['model'] = 'loadInformation' . $filter['model'];
                        }
                        $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel WHERE (loadinformationid = :model ) ORDER BY model");
                        $stmt->bindParam(':model', $filter['model'], \PDO::PARAM_STR);
                    } else {
                        $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel ORDER BY model");
                    }
                    break;
                }
                break;
            case 'byid':
                if (!empty($filter)) {
                    if (!empty($filter['model'])) {
                        $stmt = $this->db->prepare("SELECT  {$sel_inf} FROM sccpdevmodel WHERE model = :model ORDER BY model");
                        $stmt->bindParam(':model', $filter['model'],\PDO::PARAM_STR);
                    } else {
                        $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel ORDER BY model");
                    }
                    break;
                }
                break;
            case 'extension':
                $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel WHERE (dns = 0) and (enabled = 1) ORDER BY model");
                break;
            case 'enabled':
                //$stmt = $db->prepare('SELECT ' . {$sel_inf} . ' FROM sccpdevmodel WHERE enabled = 1 ORDER BY model'); //previously this fell through to phones.
                //break;  // above includes expansion modules but was not original behaviour so commented out. Falls through to phones.
            case 'phones':
                $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel WHERE (dns != 0) and (enabled = 1) ORDER BY model");
                break;
            case 'ciscophones':
                $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel WHERE (dns > 0) and (enabled = 1) AND vendor NOT LIKE '%-sip' ORDER BY model");
                break;
            case 'sipphones':
                $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel WHERE (dns > 0) and (enabled = 1) AND vendor LIKE '%-sip' ORDER BY model");
                break;
            case 'all':     // Fall through to default
            default:
                $stmt = $this->db->prepare("SELECT {$sel_inf} FROM sccpdevmodel ORDER BY model");
                break;
        }
        $stmt->execute();
        return $stmt->fetchAll(\PDO::FETCH_ASSOC);
    }

    function write($table_name = "", $save_value = array(), $mode = 'update', $key_fld = "", $hwid = "")
    {
        // mode clear  - Empty table before update
        // mode update - update / replace record
        $result = false;
        switch ($table_name) {
            case 'sccpsettings':
                if ($mode == 'replace') {  // Change mode name to be more transparent
                    $this->db->prepare('TRUNCATE sccpsettings')->execute();
                    $stmt = $this->db->prepare('INSERT INTO sccpsettings (keyword, data, seq, type, systemdefault) VALUES (:keyword,:data,:seq,:type,:systemdefault)');
                } else {
                    $stmt = $this->db->prepare('REPLACE INTO sccpsettings (keyword, seq, type, data, systemdefault) VALUES (:keyword,:seq,:type,:data,:systemdefault)');
                }
                foreach ($save_value as $key => $dataArr) {
                    if (empty($dataArr)) {
                            continue;
                    }
                    $stmt->bindParam(':keyword',$dataArr['keyword'],\PDO::PARAM_STR);
                    $stmt->bindParam(':data',$dataArr['data'],\PDO::PARAM_STR);
                    $stmt->bindParam(':seq',$dataArr['seq'],\PDO::PARAM_INT);
                    $stmt->bindParam(':type',$dataArr['type'],\PDO::PARAM_INT);
                    $stmt->bindParam(':systemdefault',$dataArr['systemdefault'],\PDO::PARAM_STR);
                    $result = $stmt->execute();
                }
                break;
            case 'sccpdevmodel':    // Fall through to next intentionally
            case 'sccpdevice':      // Fall through to next intentionally
            case 'sccpuser':
                $sql_key = "";
                $sql_var = "";
                foreach ($save_value as $key_v => $data) {
                    if (!empty($sql_var)) {
                        $sql_var .= ', ';
                    }
                    if ($data === $this->val_null) {
                        $sql_var .= $key_v . '= NULL';
                    } else {
                        $sql_var .= $key_v . ' = \'' . $data . '\''; //quote data as normally is string
                    }
                    if ($key_v === $key_fld) {
                        $sql_key = $key_v . ' = \'' . $data . '\'';  //quote data as normally is string
                    }
                }
                if (!empty($sql_var)) {
                    switch ($mode) {
                        case 'delete':
                            $stmt = $this->db->prepare("DELETE FROM {$table_name} WHERE {$sql_key}");
                            break;
                        case 'update':
                            $stmt = $this->db->prepare("UPDATE {$table_name} SET {$sql_var} WHERE {$sql_key}");
                            break;
                        case 'replace':
                            $stmt = $this->db->prepare("REPLACE INTO {$table_name} SET {$sql_var}");
                            break;
                        // no default mode - must be explicit.
                    }
                }
                $result = $stmt->execute();
                break;
            case 'sccpbuttons':
                switch ($mode) {
                    case 'delete':
                        $sql = 'DELETE FROM sccpbuttonconfig WHERE ref = :hwid';
                        $stmt = $this->db->prepare($sql);
                        $stmt->bindParam(':hwid', $hwid,\PDO::PARAM_STR);
                        $result = $stmt->execute();
                        break;
                    case 'replace':
                        foreach ($save_value as $button_array) {
                            $stmt = $this->db->prepare('UPDATE sccpbuttonconfig SET name =:name WHERE  ref = :ref AND reftype =:reftype AND instance = :instance  AND buttontype = :buttontype AND options = :options');
                            $stmt->bindParam(':ref', $button_array['ref'],\PDO::PARAM_STR);
                            $stmt->bindParam(':reftype', $button_array['reftype'],\PDO::PARAM_STR);
                            $stmt->bindParam(':instance', $button_array['instance'],\PDO::PARAM_INT);
                            $stmt->bindParam(':buttontype', $button_array['buttontype'],\PDO::PARAM_STR);
                            $stmt->bindParam(':name', $button_array['name'],\PDO::PARAM_STR);
                            $stmt->bindParam(':options', $button_array['options'],\PDO::PARAM_STR);
                            $result= $stmt->execute();
                        }
                        break;
                    case 'add':
                        foreach ($save_value as $button_array) {
                            $stmt = $this->db->prepare("INSERT INTO sccpbuttonconfig SET ref = :ref, reftype = :reftype, instance = :instance, buttontype = :buttontype, name = :name, options = :options");
                            $stmt->bindParam(':ref', $button_array['ref'],\PDO::PARAM_STR);
                            $stmt->bindParam(':reftype', $button_array['reftype'],\PDO::PARAM_STR);
                            $stmt->bindParam(':instance', $button_array['instance'],\PDO::PARAM_INT);
                            $stmt->bindParam(':buttontype', $button_array['buttontype'],\PDO::PARAM_STR);
                            $stmt->bindParam(':name', $button_array['name'],\PDO::PARAM_STR);
                            $stmt->bindParam(':options', $button_array['options'],\PDO::PARAM_STR);
                            $result = $stmt->execute();

                        }
                        break;
                    case 'clear';
                        // Clear is equivalent of delete + insert. Mode is used in order to activate trigger.
                        $this->write('sccpbuttons', '', $mode = 'delete','', $hwid);
                        $this->write('sccpbuttons', $save_value, $mode = 'add','', $hwid);
                        break;
                    // No default case - must be specific in request.
                }
        }
        return $result;
    }
    //******** Get SIP settings *******
    public function getSipTableData(string $dataid, $line='') {
        global $db;
        $tech = array();
        switch ($dataid) {
            case "DeviceById":
                // TODO: This needs to be rewritten
                $stmt = $this->db->prepare("SELECT keyword,data FROM sip WHERE id = '{$line}'");
                $stmt->execute();
                $tech = $stmt->fetchAll(\PDO::FETCH_COLUMN | \PDO::FETCH_GROUP);
                foreach ($tech as &$value) {
                    $value = $value[0];
                }

                return $tech;
            case "extensionList";
                $stmt = $this->db->prepare("SELECT id as name, data as label  FROM sip WHERE keyword = 'callerid' order by name");
                $stmt->execute();
                $result = $stmt->fetchAll(\PDO::FETCH_ASSOC);
                /*
                foreach ($result as $value) {
                    if (empty($tech[$value['id']]['id'])) {
                        $tech[$value['id']]['id']= $value['id'];
                    }
                    $tech[$value['id']][$value['keyword']]=$value['data'];
                }
                */
                return $result;
        }
    }

    /*
     *  Maybe Replace by SccpTables ??!
     *
     */
    public function dump_sccp_tables($data_path, $database, $user, $pass)
    {
        $filename = $data_path.'/sccp_backup_'.date('G_a_m_d_y').'.sql';
        $result = exec('mysqldump '.$database.' --password='.$pass.' --user='.$user.' --single-transaction >'.$filename, $output);
        return $filename;
    }

    public function updateTableDefaults($table, $field, $value) {
        $stmt = $this->db->prepare("ALTER TABLE {$table} ALTER COLUMN {$field} SET DEFAULT '{$value}'");
        $stmt->execute();
    }

/*
 *  Check Table structure
 */
    public function validate()
    {
        $result = 0;
        $check_fields = [
                        '431' => ['private'=> "enum('on','off')"],
                        '433' => ['directed_pickup'=>'']
                        ];
        $stmt = $this->db->prepare('DESCRIBE sccpdevice');
        $stmt->execute();
        foreach ($stmt->fetchAll(\PDO::FETCH_ASSOC) as $value) {
            $id_result[$value['Field']] = $value['Type'];
        }
        foreach ($check_fields as $key => $value) {
            if (!empty(array_intersect_assoc($value, $id_result))) {
                  $result = $key;
            } else {
                // no match but maybe checking against an empty string so just need to check key does not exist
                foreach ($value as $skey => $svalue) {
                    if (empty($svalue) && (!isset($id_result[$skey]))) {
                        $result = $key;
                    }
                }
            }
        }

        return $result;
    }

    public function getNamedGroup($callGroup) {
        $sql = "SELECT {$callGroup} FROM sccpline GROUP BY {$callGroup}";
        $sth = $this->db->prepare($sql);
        $result = array();
        $tech = array();
        try {
            $sth->execute();
            $result = $sth->fetchAll();
            foreach($result as $val) {
               $tech[$callGroup][] = $val[0];
            }
        } catch(\Exception $e) {}
    return $tech;
    }
}
