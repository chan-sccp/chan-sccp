<div id="toolbar-sccp-rnav">
    <a class="btn btn-default" href="config.php?display=sccp_phone#sccpdevice">
        <i class="fa fa-list"></i>&nbsp;
        <?php echo _("List Device")?>
    </a>
    <a class="btn btn-default" href="config.php?display=sccp_phone&tech_hardware=cisco">
        <i class="fa fa-plus">&nbsp;</i>
        <?php echo _("Add Device")?>
    </a>
</div>
<table id="sccpnavgrid"
    data-search="true"
    data-toolbar="#toolbar-sccp-rnav"
    data-url="ajax.php?module=sccp_manager&command=getPhoneGrid&type=sccp"
    data-cache="false"
    data-toggle="table"
    class="table">
    <thead>
        <tr>
              <th data-sortable="true" data-field="name"><?php echo _('SEP ID')?></th>
              <th data-sortable="true" data-field="description"><?php echo _('Descriptions')?></th>
        </tr>
    </thead>
</table>
<script type="text/javascript">
    $("#sccpnavgrid").on('click-row.bs.table',function(e,row,elem){
        if (row['new_hw'] == 'Y' ) {
            window.location = '?display=sccp_phone&tech_hardware=cisco&new_id=' +row['name'] +'&' +row['type'];
        } else {
            window.location = '?display=sccp_phone&tech_hardware=cisco&id='+row['name'];
        };
    })
</script>
