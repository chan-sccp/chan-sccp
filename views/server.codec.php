<?php

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

$def_val = null;
$dev_id = null;
$audio_codecs = $this->getCodecs('audio');
$video_codecs = $this->getCodecs('video');
$sccp_disallow_def = $this->sccpvalues['disallow']['data'];
$sys_disallow_def = $this->sccpvalues['disallow']['systemdefault'];

if (empty($sccp_disallow_def)) {
    $sccp_disallow_def = $sys_disallow_def;
}
?>

<!-- Codec selection is at the line level - this page sets site defaults based on chan-sccp defaults -->
<form autocomplete="off" name="frm_codec" id="frm_codec" class="fpbx-submit" action="" method="post">
    <input type="hidden" name="category" value="codecform">
    <input type="hidden" name="Submit" value="Submit">

    <div class="section" data-id="sccp_dcodecs">
        <!--Codec disallow-->
        <div class="element-container">
            <div class="row">
                <div class="col-md-12">
                    <div class="row">
                        <div class="form-group">
                            <div class="col-md-3">
                                <label class="control-label" for="sccp_disallow"><?php echo _("Disallow") ?></label>
                                <i class="fa fa-question-circle fpbx-help-icon" data-for="sccp_disallow"></i>
                            </div>
                            <div class="col-md-9 radioset">
                                <input id="sccp_disallow" type="text" name="sccp_disallow" value="<?php echo $sccp_disallow_def ?>">
                                <label for="sccp_disallow"><?php echo _("Recomended default: all") ?></label>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            <div class="row">
                <div class="col-md-12">
                    <span id="sccp_disallow-help" class="help-block fpbx-help-block"><?php echo _("Default : all. If you wish to change (Not Recommended) please enter a semicolon separated list for example: alaw;ulaw;...") ?></span>
                </div>
            </div>
        </div>
        <!--END Codec disallow-->
    </div>

    <!--SCCP Audio Codecs-->
    <div class="section-title" data-for="sccp_acodecs">
        <h3><i class="fa fa-minus"></i><?php echo _("SCCP Audio Codecs ") ?></h3>
    </div>


    <div class="section" data-id="sccp_acodecs">
        <!--Codecs-->
        <div class="element-container">
            <div class="row">
                <div class="col-md-12">
                    <div class="row">
                        <div class="form-group">
                            <div class="col-md-3">
                                <label class="control-label" for="codecw"><?php echo _("Allow") ?></label>
                            </div>
                            <div class="col-md-9">
                                <div>
                                <?php echo show_help(_("These are the default audio codec settings for this site. Unchecked codecs cannot be assigned to extensions.
                                      <br>Order can be changed by dragging and dropping to indicate priority. This priority applies for all extensions
                                      <br>Higher priority enabled codecs are at the top
                                      <br>Precedence for ulaw and alaw, if used, should be set according to your region
                                      <br>If your region uses alaw, it is important that alaw has the highest priority
                                      <br>To return to chan-sccp defaults, uncheck ALL codecs (both Audio and Video)."),"Helpful information",true) ?>
                                </div>
                                <?php
                                $seq = 1;

                                echo '<ul class="sortable">';
                                foreach ($audio_codecs as $codec => $codec_state) {
                                    $codec_trans = _($codec);
                                    $codec_checked = $codec_state ? 'checked' : '';
                                    echo '<li><a href="#">'
                                    . '<img src="assets/sipsettings/images/arrow_up_down.png" height="16" width="16" border="0" alt="move" style="float:none; margin-left:-6px; margin-bottom:-3px;cursor:move" /> '
                                    . '<input type="checkbox" '
                                    . ($codec_checked ? 'value="' . $seq++ . '" ' : '')
                                    . 'name="audiocodecs[' . $codec . ']" '
                                    . 'id="' . $codec . '" '
                                    . 'class="audio-codecs" '
                                    . $codec_checked
                                    . ' />'
                                    . '&nbsp;&nbsp;<label for="' . $codec . '"> '
                                    . '<small>' . $codec_trans . '</small>'
                                    . " </label></a></li>\n";
                                }
                                echo '</ul>';
                                ?>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        <!--END Codecs-->

    </div>
    <!--END SCCP Audio Codecs-->

    <!--SCCP Video Codecs-->
    <div class="section-title" data-for="sccp_vcodecs">
        <h3><i class="fa fa-minus"></i><?php echo _("SCCP Video Codecs ") ?></h3>
    </div>
    <div class="section" data-id="sccp_vcodecs">
        <!--Codecs-->
        <div class="element-container">
            <div class="row">
                <div class="col-md-12">
                    <div class="row">
                        <div class="form-group">
                            <div class="col-md-3">
                                <label class="control-label" for="codecw"><?php echo _("Allow") ?></label>
                            </div>
                            <div class="col-md-9">
                                <div>
                                <?php echo show_help(_("These are the default video codec settings for this site.")) ?>
                                </div>
                                <?php
                                $seq = 1;

                                echo '<ul class="sortable">';
                                // Although classed as video codecs, all stored under allow so name is audiocodecs.
                                foreach ($video_codecs as $codec => $codec_state) {
                                    $codec_trans = _($codec);
                                    $codec_checked = $codec_state ? 'checked' : '';
                                    echo '<li><a href="#">'
                                    . '<img src="assets/sipsettings/images/arrow_up_down.png" height="16" width="16" border="0" alt="move" style="float:none; margin-left:-6px; margin-bottom:-3px;cursor:move" /> '
                                    . '<input type="checkbox" '
                                    . ($codec_checked ? 'value="' . $seq++ . '" ' : '')
                                    . 'name="audiocodecs[' . $codec . ']" '
                                    . 'id="' . $codec . '" '
                                    . 'class="video-codecs" '
                                    . $codec_checked
                                    . ' />'
                                    . '&nbsp;&nbsp;<label for="' . $codec . '"> '
                                    . '<small>' . $codec_trans . '</small>'
                                    . " </label></a></li>\n";
                                }
                                echo '</ul>';
                                ?>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            <!--END Codecs-->
        </div>
        <!--END SCCP Video Codecs-->
    </div>
</form>
