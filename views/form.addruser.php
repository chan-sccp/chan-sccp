<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
$def_val = null;
$dev_id = null;
$dev_new = null;

if (!empty($_REQUEST['ru_id'])) {
    $dev_id = $_REQUEST['ru_id'];
    $def_val['id'] = array("keyword" => 'id', "data" => $dev_id, "seq" => "99");
    $db_res = $this->dbinterface->getSccpDeviceTableData('get_sccpuser', array("id" => $dev_id));
    if (!empty($db_res)) {
        foreach ($db_res as $key => $val) {
            if (!empty($val)) {
                $def_val[$key] = array("keyword" => $key, "data" => $val, "seq" => "99");
            }
        }
    }
}
 
?>

<form autocomplete="off" name="frm_addruser" id="frm_addruser" class="fpbx-submit" action="" method="post" data-id="ruser_edit">
    <input type="hidden" name="category" value="addruser_form">
    <input type="hidden" name="Submit" value="Submit">

    <?php
    echo $this->showGroup('sccp_ruser', 1, 'sccp_ru', $def_val);
    echo $this->showGroup('sccp_ruser_time', 1, 'sccp_ru', $def_val);
    ?>    
</form>
