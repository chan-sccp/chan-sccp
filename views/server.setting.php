<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
// vim: set ai ts=4 sw=4 ft=phtml:

?>

<form autocomplete="off" name="frm_general" id="frm_general" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value="generalform">
    <input type="hidden" name="Submit" value="Submit">
    <!-- div id="toolbar-all">
        <button type="button" class="btn btn-primary btn-lg" data-toggle="modal" onclick="load_oncliсk(this,'*new*')" data-target=".new_network"><i class="fa fa-bolt"></i> <?php echo _("Add Keyset"); ?></button>
    </div -->
    <div class="fpbx-container container-fluid">
        <div class="row">
            <div class="container">
                <h2 style="border:2px solid Tomato;color:Tomato;" ><?php echo _("Warning : Any changes to the server configuration can cause all phones to restart"); ?></h2>
            </div>
        </div>
    </div>
    <?php
        $def_val_device = $this->getTableDefaults('sccpdevice');

        echo $this->showGroup('sccp_general', 1);
        echo $this->showGroup('sccp_net', 1);
        echo $this->showGroup('sccp_lang', 1);
        echo $this->showGroup('sccp_extpath_config', 1);

    ?>

</form>

<!-- Begin Form Input New / Edit  -->
<div class="modal fade new_network" tabindex="-1" role="dialog" aria-labelledby="myLargeModalLabel_Net">
    <div class="modal-dialog modal-lg" role="document">
        <div class="modal-content">
            <div class="modal-header">
                <button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button>
                <h4 class="modal-title" id="gridSystemModalLabel_Net">Device</h4>
            </div>
            <div class="modal-body">
                <ul class="nav nav-tabs" role="tablist">
                <?php
//                    echo $this->showGroup('add_network_1',0);
                ?>
                </ul>
            </div>

            <div class="modal-footer">
                <button type="button" class="btn btn-default" data-dismiss="modal">Close</button>
                <button type="button" class="btn btn-primary sccp_update" data-id="network_add" data-mode="new" id="network_add" data-dismiss="modal">Save</button>
            </div>
        </div>
    </div>
</div>
