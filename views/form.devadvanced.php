<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
?>    
<form autocomplete="off" name="frm_advanced" id="frm_adddevice" class="fpbx-submit" action="" method="post" data-id="hw_edit">
    <input type="hidden" name="category" value="deviceadvanced_form">
    <input type="hidden" name="Submit" value="Submit">
    
    <?php
    echo $this->showGroup('sccp_hw_addv_device', 1, 'sccp_hw', $def_val);
    
    ?>    
</form>
