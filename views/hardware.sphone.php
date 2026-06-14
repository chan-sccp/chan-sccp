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
                <h1><?php echo _("Device SIP Phone") ?></h1>
                <div id="toolbar-sccp-sphone">
                    <a class="btn btn-default" href="config.php?display=sccp_phone&tech_hardware=cisco-sip"><i class="fa fa-plus">&nbsp;</i><?php echo _("Add Device Phone") ?></a>
                    <button id="remove-sccp-sphone" class="btn btn-danger sccp_update btn-tab-select" data-id="delete_hardware" disabled>
                        <i class="glyphicon glyphicon-remove"></i> <span><?php echo _('Delete') ?></span>
                    </button>
                    <button name="cr_sip_phone_xml" class="btn sccp_update btn-default" data-id="create-cnf">
                        <i class="glyphicon glyphicon-ok"></i> <span><?php echo _('Create CNF') ?></span>
                    </button>
                </div>
                <table data-cookie="true" data-cookie-id-table="sccp-sphone" data-url="ajax.php?module=sccp_manager&command=getPhoneGrid&type=cisco-sip"
                            data-cache="false" data-show-refresh="true" data-toolbar="#toolbar-sip" data-maintain-selected="true"
                            data-show-columns="true" data-show-toggle="true" data-toggle="table" data-pagination="true"
                            data-search="true" class="table table-striped ext-list" id="table-sip" data-id="name">
                    <thead>
                        <tr>
                            <th data-checkbox="true"></th>
                            <th data-sortable="true" data-field="name"><?php echo _('Device ID') ?></th>
                            <th data-sortable="true" data-field="description"><?php echo _('Device  Description') ?></th>
                            <th data-sortable="true" data-formatter="DispayTypeFormatter" data-field="type"><?php echo _('Device type') ?></th>
                            <th data-sortable="true" data-field="button" data-formatter="LineFormatter"><?php echo _('Line') ?></th>
                            <th data-sortable="true" data-field="status"><?php echo _('Status') ?></th>
                            <th data-sortable="true" data-field="address"><?php echo _('Address') ?></th>
                            <th data-field="actions" data-formatter="DispayDeviceActionsKeyFormatterS"><?php echo _('Actions') ?></th>
                        </tr>
                    </thead>
                </table>
            </div>
        </div>
    </div>
</div>

<script>



    function DispayTypeFormatter(value, row, index) {
        var exp_model = value;
        if (row['addon'] !== null ) {
            var posd = row['addon'].indexOf(';');
            if (posd >0) {
                exp_model += ' + 2x ' + row['addon'].substring(0, posd);
            } else {
                exp_model += ' + ' + row['addon'];
            }
        }
        return  exp_model;
    }
    function DispayDeviceActionsKeyFormatterS(value, row, index) {
        var exp_model = '';
        if (row['new_hw'] == "Y") {
            exp_model += '<a href="?display=sccp_phone&tech_hardware=cisco-sip&new_id=' + row['name'] + '&type='+ row['type'];
            if (row['addon'] !== null ) {
                exp_model += '&addon='+ row['addon'];
            }
            exp_model += '"><i class="fa fa-pencil"></i></a> &nbsp; &nbsp;\n';

        } else {
            exp_model += '<a href="?display=sccp_phone&tech_hardware=cisco-sip&id=' + row['name'] + '"><i class="fa fa-pencil"></i></a> &nbsp; &nbsp;\n';
            exp_model += '</a> &nbsp;<a class="btn-item-delete" data-for="hardware" data-id="' + row['name'] + '"><i class="fa fa-trash"></i></a>';
        }
        return  exp_model;
    }
    function LineFormatter(value, row, index) {
        if (value === null)  {
            return  '-- EMPTY --';
        }
        var data = value.split(";");
        result = '';
        for (var i = 0; i < data.length; i++) {
            var val = data[i].split(',');
            if (val[0] === 'line') {
              result = result + val[1] + '<br>';
            }
        }
        return  result;
    }
    
</script>
