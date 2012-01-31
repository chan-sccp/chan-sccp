#!/usr/bin/php
<?php

if (file_exists('test.xml')) {
    $xml = simplexml_load_file('test.xml',null,LIBXML_NOCDATA);
//    print_r($xml);
    $version = $xml->version;
    $revision = $xml->revision;
    print "version: $version, revison: $revision\n";


    print "<form><table>\n";
    foreach ($xml->section as $value){ 
      $sectionname = $value['name'];
      print "<tr><th>$sectionname</th><tr>";
      foreach ($value->params->param as $value1){ 
        $name = $value1['name'];
        $size = $value1->size;
        $required = $value1->required;
        $description = trim($value1->description);

        print "<tr><td ><label for='$name'>";
        print "<div id=name>$name</div>";
        
        if ( $value1->required) {
            print "<div id=required>*</div>";
        }
        print "</label></td>";
        print "<td>"; 
        
        $type = $value1->type;
        switch ($type) {
            case "string":
            case "int":
            case "unsigned int":
                print "<input type='text' name='$name' size='$size'/>";
                break;
            case "boolean":
                print "<input type='radio' name='$name' value='On' />On<br />";
                print "<input type='radio' name='$name' value='Off' />Off<br />";
                break;
            case "generic":
                if(isset( $value1->type_spec)) {
                    $type_spec = explode(",",$value1->type_spec);
                } else {
                    $type_spec = "";
                }
                if(isset( $value1->multiple)) {
                  print "<select name='$name' multiple>";
                } else { 
                  print "<select name='$name'>";
                }
                foreach($type_spec as $spec) {
                  print "<option value='$spec'>$spec</option>";
                }
                print "</select>";
                break;
        }
        print "</td>\n";
        if(isset( $value1->multiple) && $type<>"generic") {
            print "<td>"; 
            print "<a href='?name=$name&action=Add'>Add</a>&nbsp;";
            print "<a href='?name=$name&action=Remove'>Remove</a>";
            print "</td>";
        }
        print "</tr>\n";
      }
    }
    print "</table></form>\n";
} else {
    exit('Failed to open test.xml.');
} 
?>
