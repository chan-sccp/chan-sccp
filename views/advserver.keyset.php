<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
$keymultiselect = array('AllRight' =>'>>', 'Right' => '>', 'AllLeft' => '<<', 'Left' => '<');

//   ------------------------------------- Key Set Value ---------------------------------------------------------
$keysetarray =  $this->extconfigs->getExtConfig('keyset');

/*$keysetarray1 = array('onhook'    => array('redial','newcall','cfwdall','dnd','pickup','gpickup','private'),
                    'connected'  => array('hold','endcall','park','vidmode','select','cfwdall','cfwdbusy','idivert'),
                    'onhold'     => array('resume','newcall','endcall','transfer','conflist','select','dirtrfr','idivert','meetme'),
                    'ringin'     => array('answer','endcall','transvm','idivert'),
                    'offhook'    => array('redial','endcall','private','cfwdall','cfwdbusy','pickup','gpickup','meetme','barge'),
                    'conntrans'  => array('hold','endcall','transfer','conf','park','select','dirtrfr','vidmode','meetme','cfwdall','cfwdbusy'),
                    'digitsfoll' => array('back','endcall','dial'),
                    'connconf'   => array('conflist','newcall','endcall','hold','vidmode'),
                    'ringout'    => array('empty','endcall','transfer','cfwdall','idivert'),
                    'offhookfeat'=> array('redial','endcall'),
                    'onhint'     => array('redial','newcall','pickup','gpickup','barge'),
                    'onstealable'=> array('redial','newcall','cfwdall','pickup','gpickup','dnd','intrcpt'),
                    'holdconf'   => array('resume','newcall','endcall','join'),
                    'uriaction'  => array('default'));
*/
//   ------------------------------------- Key Set Display information  ---------------------------------------------------------
$keynamearray = array('onhook'    => array('sname' => 'ONHOOK', 'name' =>'Display Onhook', 'help' =>'help.'),
                    'connected'  => array('sname' => 'CONNECTED', 'name' =>'Display Connected', 'help' =>'help.'),
                    'onhold'     => array('sname' => 'ONHOLD', 'name' =>'Display onhold', 'help' =>'help.'),
                    'ringin'     => array('sname' => 'RINGIN', 'name' =>'Display ringin', 'help' =>'help.'),
                    'offhook'    => array('sname' => 'OFFHOOK', 'name' =>'Display offhook', 'help' =>'help.'),
                    'conntrans'  => array('sname' => 'CONNTRANS', 'name' =>'Display conntrans', 'help' =>'help.'),
                    'digitsfoll' => array('sname' => 'DIGITSFOLL', 'name' =>'Display digitsfoll', 'help' =>'help.'),
                    'connconf'   => array('sname' => 'CONNCONF', 'name' =>'Display connconf', 'help' =>'help.'),
                    'ringout'    => array('sname' => 'RINGOUT', 'name' =>'Display ringout', 'help' =>'help.'),
                    'offhookfeat'=> array('sname' => 'OFFHOOKFEAT', 'name' =>'Display offhookfeat', 'help' =>'help.'),
                    'onhint'     => array('sname' => 'ONHINT', 'name' =>'Display onhint', 'help' =>'help.'),
                    'onstealable'=> array('sname' => 'onstealable', 'name' =>'Display onstealable', 'help' =>'help.'),
                    'holdconf'   => array('sname' => 'HOLDCONF', 'name' =>'Display holdconf', 'help' =>'help.'),
                    'uriaction'  => array('sname' => '', 'name' =>'Display uriaction', 'help' =>'help.')
    );

?>
<form autocomplete="off" name="frm_keyset" id="frm_keyset" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value="keysetform">
    <input type="hidden" name="Submit" value="Submit">
    <div class="fpbx-container container-fluid">
        <div class="row">
            <div class="col-sm-12">
                <div class="display no-border">
                    <div id="toolbar-all">
                        <button type="button" class="btn btn-primary btn-lg" data-toggle="modal" onclick="load_oncliсk(this,'*new*')" data-target=".edit_new_keyset"><i class="fa fa-bolt"></i> <?php echo _("Add Keyset"); ?></button>
                    </div>
                    <table data-cookie="true" data-cookie-id-table="sccp_keyset-all" data-url="ajax.php?module=sccp_manager&command=getSoftKey&type=active" data-cache="false" data-show-refresh="true" data-toolbar="#toolbar-all" data-maintain-selected="true" data-show-columns="true" data-show-toggle="true" data-toggle="table" data-pagination="true" data-search="true" class="table table-striped ext-list" id="softkey-all" data-unique-id="softkeys">
                        <thead>
                            <tr>
                                <th data-sortable="true" data-field="softkeys"><?php echo _('KeySetName')?></th>
                                <?php
                                $i = 0;
                                foreach ($keynamearray as $key => $value) {
                                    if ($i < 12) {
                                        echo '<th data-sortable="false" data-field="'.$key.'">'._($value['sname']).'</th>';
                                    }
                                    $i ++;
                                }
                                ?>
                                <th data-field="actions" data-formatter="DispayActionsKeyFormatter"><?php echo _('Actions')?></th>
                            </tr>
                        </thead>
                        <tbody>

                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    </div>
</form>

<!-- Begin Form Input New / Edit  -->
<div class="modal fade edit_new_keyset" tabindex="-1" role="dialog" aria-labelledby="myLargeModalLabel2">
    <div class="modal-dialog modal-lg" role="document">
        <div class="modal-content">
            <div class="modal-header">
                <button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button>
                <h4 class="modal-title" id="gridSystemModalLabel">Add New KeySet</h4>
            </div>
            <div class="modal-body">
                <style type="text/css">
                    input:invalid {border: red solid 3px;}
                </style>
                <div class="element-container">
                    <div class="row">
                        <div class="form-group">
                            <div class="col-md-3">
                                <label class="control-label" for="new_keysetname">Name Keyset</label>
                                <i class="fa fa-question-circle fpbx-help-icon" data-for="new_devmodel"></i>
                            </div>
                            <div class="col-md-9">
                                <input type="text" maxlength="15" pattern="[a-zA-Z0-9]{4,15}" class="form-control" id="new_keySetname" name="new_keySetname" value="SoftKeyset">
                            </div>
                        </div>
                    </div>
                    <div class="row">
                        <div class="col-md-12">
                            <span id="new_devmodel-help" class="help-block fpbx-help-block">Max length 15 letters or numbers without spaces</span>
                        </div>
                    </div>
                </div>
                <ul class="nav nav-tabs" role="tablist">

<?php
                $i = 0;
foreach ($keysetarray as $key => $value) {
    if ($i == 0) {
        echo '<li role="presentation" data-name="'.$key.'" class="active">';
    } else {
        echo '<li role="presentation" data-name="'.$key.'" class="change-tab">';
    }
    echo '<a href="#'.$key.'" aria-controls="'.$key.'" role="tab" data-toggle="tab">'._($key);
    echo '</a></li>';
    $i ++;
}
?>
        </ul>
                <div class="tab-content display">
<?php
                $i = 0;
foreach ($keysetarray as $key => $value) {
    if ($i == 0) {
        echo '<div role="tabpanel" id="'.$key.'" class="tab-pane active">';
    } else {
        echo '<div role="tabpanel" id="'.$key.'" class="tab-pane">';
    }
    echo '<div class="element-container"><div class="row"><div class="form-group"><div class="col-md-3"><label class="control-label" for="'.$key.'">'._($keynamearray[$key]['name']).'</label>';
    echo '<i class="fa fa-question-circle fpbx-help-icon" data-for="'.$key.'"></i></div>';

        echo '<div class="col-md-4"><select multiple class="form-control sccpmultiselect" name="av_'.$key.'" id="source_'.$key.'">';
    $row_dada= explode(',', $value);
    foreach ($row_dada as $data) {
          echo '<option value="'.$data.'">'.$data.'</option>';
    }
    echo '</select></div><div class="col-md-1">';
    foreach ($keymultiselect as $btkey => $btval) {
        echo '<input type="button" class="btnMultiselect" data-id="'.$key.'" data-key="'.$btkey.'" value="'.$btval.'">';
    }
    echo '</div><div class="col-md-4"><select multiple class="form-control" name="sel_'.$key.'" id="destination_'.$key.'">';
    echo '</select></div></div></div><div class="row"><div class="col-md-12">';
    echo '<span id="'.$key.'-help" class="help-block fpbx-help-block">'._($keynamearray[$key]['help']).'</span>';
    echo '</div></div></div></div>';
    $i ++;
}
?>
                </div>

            </div>
            <div class="modal-footer">
                <button type="button" class="btn btn-default" data-dismiss="modal">Close</button>
                <button type="button" class="btn btn-primary sccp_update" data-id="keyset_add" data-mode="new" id="keyset_add" data-dismiss="modal">Save</button>
            </div>
        </div>
    </div>
</div>

<script>
    function DispayActionsKeyFormatter(value, row, index) {
        var exp_model = '';
        if (row['softkeys'] !== 'default') {
            exp_model += '<a href="#edit_softkeys"   onclick="load_oncliсk(this, &quot;'+row['softkeys']+'&quot;)" data-toggle="modal" data-target=".edit_new_keyset"><i class="fa fa-pencil"></i></a>&nbsp;';
            exp_model += '</a> &nbsp;<a class="btn-item-delete" data-for="softkeys" data-id="' + row['softkeys'] + '"><i class="fa fa-trash"></i></a>';
        }
        return  exp_model;
    }
</script>
