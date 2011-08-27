ALTER TABLE `sccpdevice` ADD `softkeyset` VARCHAR( 50 ) NULL DEFAULT NULL AFTER `addon`;
ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` ADD `nat` VARCHAR( 5 ) NULL DEFAULT "no" BEFORE `name`;
ALTER TABLE `sccpdevice` CHANGE COLUMN 'earlyrtp' VARCHAR(8) default 'off';

ALTER TABLE `sccpline` CHANGE COLUMN `rtptos` `audio_tos` VARCHAR( 11 ) NULL DEFAULT "0xB8";
ALTER TABLE `sccpline` ADD `audio_cos` VARCHAR( 1 ) NULL DEFAULT "6" AFTER `audio_tos`;
ALTER TABLE `sccpline` ADD `video_tos` VARCHAR( 11 ) NULL DEFAULT "0x88" AFTER `audio_cos`;
ALTER TABLE `sccpline` ADD `video_cos` VARCHAR( 1 ) NULL DEFAULT "5" AFTER `video_tos`;
update sccpline set audio_cos="6",video_tos="0x88",video_cos="5";
update sccpline set audio_tos="0xB8" where audio_tos=NULL or audio_tos="";
