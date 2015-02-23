DROP sccpdeviceconfig;
ALTER TABLE `sccpdevice` ADD `softkeyset` VARCHAR( 50 ) NULL DEFAULT NULL AFTER `addon`;
ALTER TABLE `sccpdevice` CHANGE COLUMN `earlyrtp` VARCHAR(10);
ALTER TABLE `sccpdevice` CHANGE COLUMN `nat` VARCHAR(4) DEFAULT 'auto';
ALTER TABLE `sccpdevice` ADD `audio_tos` VARCHAR( 11 ) NULL DEFAULT "0xB8" AFTER `softkeyset`;
ALTER TABLE `sccpdevice` ADD `audio_cos` VARCHAR( 1 ) NULL DEFAULT "6" AFTER `audio_tos`;
ALTER TABLE `sccpdevice` ADD `video_tos` VARCHAR( 11 ) NULL DEFAULT "0x88" AFTER `audio_cos`;
ALTER TABLE `sccpdevice` ADD `video_cos` VARCHAR( 1 ) NULL DEFAULT "5" AFTER `video_tos`;
ALTER TABLE `sccpdevice` ADD `conf_allow` varchar(3) default 'on' AFTER `video_cos`;
ALTER TABLE `sccpdevice` ADD `conf_play_general_announce` varchar(3) default 'on' AFTER `conf_allow`;
ALTER TABLE `sccpdevice` ADD `conf_play_part_announce` varchar(3) default 'on' AFTER `conf_play_general_announce`;
ALTER TABLE `sccpdevice` ADD `conf_mute_on_entry` varchar(3) default 'off' AFTER `conf_mute_on_entry`;
ALTER TABLE `sccpdevice` ADD `conf_music_on_hold_class` varchar(80) default 'default' AFTER `conf_mute_on_entry`;
ALTER TABLE `sccpdevice` ADD `conf_show_conflist` varchar(3) default 'on' AFTER `conf_music_on_hold_class`;
ALTER TABLE `sccpdevice` DROP COLUMN `trustphoneip`;
ALTER TABLE `sccpdevice` DROP COLUMN `dtmfmode`;

ALTER TABLE `sccpline` CHANGE COLUMN `dnd` `dndFeature`;
ALTER TABLE `sccpline` ADD `dnd` VARCHAR( 7 ) DEFAULT "reject" AFTER `amaflags`;
ALTER TABLE `sccpline` DROP COLUMN `rtptos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_tos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_cos`;
ALTER TABLE `sccpline` DROP COLUMN `video_tos`;
ALTER TABLE `sccpline` DROP COLUMN `video_cos`;
update sccpdevice set audio_tos="0xB8",audio_cos="6",video_tos="0x88",video_cos="5" where audio_tos=NULL or audio_tos="";
--
-- View for merging device and button configuration
--
CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', buttonconfig.type, buttonconfig.name, buttonconfig.options )
	ORDER BY instance ASC
	SEPARATOR ';' ) AS button, sccpdevice.*
	FROM sccpdevice
	LEFT JOIN buttonconfig ON ( buttonconfig.device = sccpdevice.name )
GROUP BY sccpdevice.name;
