#!/usr/bin/php
<?php

if (file_exists('test.xml')) {
    $xml = simplexml_load_file('test.xml',null,LIBXML_NOCDATA);
//    print_r($xml);
    $version = $xml->version;
    $revision = $xml->revision;
    print "version: $version, revison: $revision\n";

    print "<html>";
    print "<style media='screen' type='text/css'>"; 
    print "label span {display: none;}";
    print "label:hover span {display: block; relative: absolute; top: 10px; left: 10px; width: 400px; padding: 2px; margin: 2px; z-index: 100; color: #AAA; background: yellow; font: 10px Verdana, sans-serif; text-align: left; filter:alpha(opacity=95); opacity:0.95; display:inline;}";
    print "</style>";
    print "<form><table>\n";
    foreach ($xml->section as $value){ 
      $sectionname = $value['name'];
      print "<tr><th>$sectionname</th><tr>";
      foreach ($value->params->param as $value1){ 
        $name = $value1['name'];
        $size = $value1->size;
        $required = $value1->required;
        $description = trim($value1->description);

        print "<tr><td nowrap><label for='$name'>";
        print "<div id='name'>$name</div>";
        print "<span wrap>$description</span>";
        
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
                if(isset( $value1->generic_parser)) {
                    $parser = explode("=",$value1->generic_parser);
                    if (isset($parser[0])) {
                        $type_parser = $parser[0];
                        if (isset($parser[1])) {
                            $type_values = explode(",",$parser[1]);
                            if(isset( $value1->multiple)) {
                              print "<select name='$name' class='$type_parser' multiple>";
                            } else { 
                              print "<select name='$name' class='$type_parser'>";
                            }
                            foreach($type_values as $value) {
                              print "<option value='$value'>$value</option>";
                            }
                            print "</select>";
                        } else {
                            print "<input type='text' class='$type_parser' name='$name' size='$size'/>";
                        }
                    }
                }
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
    print "</html>";
} else {
    exit('Failed to open test.xml.');
} 
?>
