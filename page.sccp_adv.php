<?php /* $Id:$ */
if (!defined('FREEPBX_IS_AUTH')) {
    die('No direct script access allowed');
}
//  License for all code of this FreePBX module can be found in the license file inside the module directory
//  Copyright 2015 Sangoma Technologies.
//
// vim: set ai ts=4 sw=4 ft=php:

// SccpSettings page. Re-written for usage with chan_sccp
// AGPL v3 Licened

// Note that BEFORE THIS IS CALLED, the Sipsettings configPageinit
// function is called. This is where you do any changes. The page.foo.php
// is only for DISPLAYING things.  MVC is a cool idea, ya know?
//
$spage = FreePBX::create()->Sccp_manager;
if (empty($spage->class_error)) {
    $display_page = $spage->advServerShowPage();
    $display_info = _("SCCP Advance Server Configuration");
} else {
    $display_page = $spage->infoServerShowPage();
    $display_info = _("SCCP Server Configuration");
}
// standardise code to reduce base
include('page.html.php');
?>
