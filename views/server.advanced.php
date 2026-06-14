<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
?>
<form autocomplete="off" name="frm_advanced" id="frm_ntp" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value="advancedform">
    <input type="hidden" name="Submit" value="Submit">

<?php

        // originally, this was populated by sccpgeneral.xml but that should be static
        // now will populate from the db defaults.
        $defaultVals = $this->getTableDefaults('sccpdevice', true);

        echo $this->showGroup('sccp_srst', 1);
        echo $this->showGroup('sccp_dev_vendor_conf', 1, 'sccpdevice', $defaultVals);

?>
</form>
