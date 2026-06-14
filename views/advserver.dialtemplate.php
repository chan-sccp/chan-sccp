<?php
/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
// vim: set ai ts=4 sw=4 ft=phtml:
?>
<div class="fpbx-container container-fluid">
    <div class="row">
        <div class="col-sm-12">
            <div class="display no-border">
                <h1><?php echo _("Cisco Dial Template") ?></h1>
                <div id="toolbar-sccp-dialtemplate">
                    <a class="btn btn-default" href="config.php?display=sccp_adv&tech_hardware=dialplan&extdisplay=*new*"><i class="fa fa-plus">&nbsp;</i><?php echo _("Add Dialplan") ?></a>
                    <button id="remove-sccp-dialtemplate" class="btn btn-danger btn-remove" data-type="dialtemplate" data-section="sccp-dialtemplate" disabled>
                        <i class="glyphicon glyphicon-remove"></i> <span><?php echo _('Delete') ?></span>
                    </button>
                </div>
                <table data-cookie="true" data-cookie-id-table="sccp-dialtemplate-table" data-url="ajax.php?module=sccp_manager&command=getDialTemplate" data-cache="false" data-show-refresh="true" data-toolbar="#toolbar-dialtemplate" data-maintain-selected="true" data-show-columns="true" data-show-toggle="true" data-toggle="table" data-pagination="true" data-search="true" class="table table-striped ext-list-sccp" id="table-sccp-dialtemplate" data-id="id">
                    <thead>
                        <tr>
<!--                            <th data-checkbox="true"></th> -->
                            <th data-sortable="true" data-field="id"><?php echo _('Template name') ?></th>
                            <th data-field="actions" data-formatter="DispayDPActionsKeyFormatter"><?php echo _('Actions') ?></th>
                        </tr>
                    </thead>
                </table>
            </div>
        </div>
    </div>
</div>
<script>
    function DispayDPActionsKeyFormatter(value, row, index) {
        var exp_model = '';
        exp_model += '<a href="?display=sccp_adv&tech_hardware=dialplan&extdisplay=' + row['id'] + '"><i class="fa fa-pencil"></i></a> &nbsp; &nbsp;\n';
        if (row['id'] !== 'dialplan') {
            exp_model += '</a> &nbsp;<a class="btn-item-delete" data-for="dialplan" data-id="' + row['id'] + '"><i class="fa fa-trash"></i></a>';
        }
        return  exp_model;
    }
</script>
