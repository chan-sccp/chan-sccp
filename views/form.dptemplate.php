<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

//$list_data = $this->getDialPlan('dialplan');
//print_r($list_data);
//$dialFelds = array('match','timeout','line','rewrite','tone');
//$dialFelds = array('match','timeout','User','rewrite','tone');
$dialFelds = array('match','timeout','rewrite','tone');
$dev_id = '*new*';
if (!empty($_REQUEST['extdisplay'])) {
    $dev_id = $_REQUEST['extdisplay'];
}
if ($dev_id != '*new*') {
    $list_data= $this->getDialPlan($dev_id);
    $data_s= '';
    foreach ($list_data['template'] as $key => $value) {
        foreach ($dialFelds as $fld) {
            if (isset($value[$fld])) {
                $data_s .=(string)$value[$fld];
            }
            $data_s .= '/';
        }
        $data_s = substr($data_s, 0, -1);
        $data_s .= ';';
    }
    $data_s = substr($data_s, 0, -1);
    $def_val['dialtemplate'] =  array("keyword" => 'dialtemplate', "data" => $data_s, "seq" => "99");
}

?>


<form autocomplete="off" name="frm_editdialtemplate" id="frm_editbuttons" class="fpbx-submit" action="" method="post" data-id="dial_template">
    
    <input type="hidden" name="idtemplate" value="<?php echo str_replace('dial', '', $dev_id);?>">
    <input type="hidden" name="Submit" value="Submit">
    <?php
    if ($dev_id == '*new*') {
        echo $this->showGroup('sccp_dp_new_template', 0, 'sccp_dial', $def_val);
    }
    ?>    
    
    <div class="panel panel-default">
        <div class="panel-heading"><?php echo _("Dial Plan Help");?>
            <a data-toggle="collapse" href="#pathelp"><i class="fa fa-plus pull-right"></i></a>
        </div>
        <div class="panel-body collapse" id="pathelp">
        <p> <?php echo _("Specifies a pattern to match dialed digits against. Note: TEMPLATE must be in uppercase.");?> </p>
            <h4><?php echo _("Rules:");?></h4>
            <table class="table">
                <tr><td><strong><?php echo _("match:");?> </strong></td><td><?php echo _("Pattern to match, consists of one or more elements");?></td></tr>
                <tr><td><strong>0 1 2 3 4 5 6 7 8 9</strong></td><td><?php echo _("Match digit");?></td></tr>
                <tr><td><strong>.</strong></td><td><?php echo _("Match one digit, # or *");?></td></tr>
                <tr><td><strong>*</strong></td><td><?php echo _("Match zero or more digits, # or *");?></td></tr>
                <tr><td><strong>\*</strong></td><td><?php echo _("Match a literal *");?></td></tr>
                <tr><td><strong>,</strong></td><td><?php echo _("Play secondary dial-tone specified by tone");?></td></tr>
                <tr><td><strong><?php echo _("timeout:");?></strong></td><td><?php echo _("Number of seconds to wait for more digits if this pattern matches");?></td></tr>
                <tr><td><strong><?php echo _("line:");?></strong></td><td><?php echo _("Only apply template to the specified line (optional)");?></td></tr>
                <tr><td><strong><?php echo _("rewrite:");?></strong></td><td><?php echo _("Rewrite the matched digits before dialing, consists of one or more elements (optional)");?></td></tr>
                <tr><td><strong>0 1 2 3 4 5 6 7 8 9</strong></td><td><?php echo _("Replace with digit");?></td></tr>
                <tr><td><strong>%0</strong></td><td><?php echo _("The entire match");?></td></tr>
                <tr><td><strong>%1 %2 %3 %4 %5</strong></td><td><?php echo _("Replace with group of digits matched, grouping is done by consecutive literal digit or . elements");?></td></tr>
                <tr><td><strong>%%</strong></td><td><?php echo _("A literal %");?></td></tr>
                <tr><td><strong>.</strong></td><td><?php echo _("Each . is replaced by the digit that was matched by the corresponding . in the pattern");?></td></tr>
                <tr><td><strong><?php echo  _("tone:");?></strong></td><td><?php echo _("Secondary dial-tone to play when a , is matched, up to 3 can be specified (optional)");?></td></tr>
            </table>
        </div>
    </div>
    
    
<?php
//    echo $this->showGroup('sccp_dp_new_template',0,'sccp_dial',$def_val);
    echo $this->showGroup('sccp_dp_template', 0, 'sccp_dial', $def_val);
?>    
</form>
