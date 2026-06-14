<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
?>
<form autocomplete="off" name="frm_url" id="frm_url" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value=" deviceurlform">
    <input type="hidden" name="Submit" value="Submit">
<?php
        echo $this->showGroup('sccp_dev_url', 1,);
?>
</form>
