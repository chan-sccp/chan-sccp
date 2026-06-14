<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
?>
<form autocomplete="off" name="frm_ntp" id="frm_ntp" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value="ntpform">
    <input type="hidden" name="Submit" value="Submit">
    
<?php
    
        echo $this->showGroup('sccp_dev_ntp', 1);
        echo $this->showGroup('sccp_dev_time', 1);

?>  
</form>
