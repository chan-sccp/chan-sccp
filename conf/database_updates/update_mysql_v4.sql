ALTER TABLE `sccpdevice` ADD `softkeyset` VARCHAR( 50 ) NULL DEFAULT NULL AFTER `addon`;
ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` ADD `nat` VARCHAR( 5 ) NULL DEFAULT "no" BEFORE `name`;
ALTER TABLE `sccpdevice` CHANGE COLUMN `earlyrtp` VARCHAR(8);
ALTER TABLE `sccpdevice` ADD `audio_tos` VARCHAR( 11 ) NULL DEFAULT "0xB8" AFTER `softkeyset`;
ALTER TABLE `sccpdevice` ADD `audio_cos` VARCHAR( 1 ) NULL DEFAULT "6" AFTER `audio_tos`;
ALTER TABLE `sccpdevice` ADD `video_tos` VARCHAR( 11 ) NULL DEFAULT "0x88" AFTER `audio_cos`;
ALTER TABLE `sccpdevice` ADD `video_cos` VARCHAR( 1 ) NULL DEFAULT "5" AFTER `video_tos`;
ALTER TABLE `sccpdevice` ADD `conf_allow` varchar(3) NULL default 'on' AFTER `video_cos`;
ALTER TABLE `sccpdevice` ADD `conf_play_general_announce` varchar(3) NULL default 'on' AFTER `conf_allow`;
ALTER TABLE `sccpdevice` ADD `conf_play_part_announce` varchar(3) NULL default 'on' AFTER `conf_play_general_announce`;
ALTER TABLE `sccpdevice` ADD `conf_mute_on_entry` varchar(3) NULL default 'off' AFTER `conf_mute_on_entry`;
ALTER TABLE `sccpdevice` ADD `conf_music_on_hold_class` varchar(80) NULL default 'default' AFTER `conf_mute_on_entry`;
ALTER TABLE `sccpdevice` REMOVE COLUMN `trustphoneip`;

ALTER TABLE `sccpline` CHANGE COLUMN `dnd` `dndFeature`;
ALTER TABLE `sccpline` ADD `dnd` VARCHAR( 5 ) DEFAULT "on" AFTER `amaflags`;
ALTER TABLE `sccpline` DROP COLUMN `rtptos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_tos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_cos`;
ALTER TABLE `sccpline` DROP COLUMN `video_tos`;
ALTER TABLE `sccpline` DROP COLUMN `video_cos`;
update sccpdevice set audio_tos="0xB8",audio_cos="6",video_tos="0x88",video_cos="5" where audio_tos=NULL or audio_tos="";

ALTER TABLE `sccpdevice` ADD `backgroundImage` varchar(255) DEFAULT NULL;
ALTER TABLE `sccpdevice` ADD `ringtone` varchar(255) DEFAULT NULL;