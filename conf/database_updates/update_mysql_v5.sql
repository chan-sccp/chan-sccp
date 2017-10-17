DROP VIEW sccpdeviceconfig;
ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` CHANGE `nat` VARCHAR( 7 ) DEFAULT "auto";
ALTER TABLE `sccpdevice` CHANGE COLUMN `earlyrtp` VARCHAR(10);
ALTER TABLE `sccpdevice` ADD `audio_tos` VARCHAR( 11 ) NULL DEFAULT "0xB8" AFTER `softkeyset`;
ALTER TABLE `sccpdevice` ADD `audio_cos` VARCHAR( 1 ) NULL DEFAULT "6" AFTER `audio_tos`;
ALTER TABLE `sccpdevice` ADD `video_tos` VARCHAR( 11 ) NULL DEFAULT "0x88" AFTER `audio_cos`;
ALTER TABLE `sccpdevice` ADD `video_cos` VARCHAR( 1 ) NULL DEFAULT "5" AFTER `video_tos`;
ALTER TABLE `sccpdevice` DROP COLUMN `trustphoneip`;
ALTER TABLE `sccpdevice` DROP COLUMN `dtmfmode`;
ALTER TABLE `sccpdevice` CHANGE COLUMN `deny` VARCHAR(100) default '0.0.0.0/0.0.0.0';
ALTER TABLE `sccpdevice` CHANGE COLUMN `permit` VARCHAR(100) default 'internal';
ALTER TABLE `sccpdevice` CHANGE COLUMN `dnd` `dndFeature`;

ALTER TABLE `sccpline` ADD `dnd` VARCHAR( 7 ) DEFAULT "reject" AFTER `amaflags`;
ALTER TABLE `sccpline` ADD `namedcallgroup` VARCHAR(100) DEFAULT "" AFTER `pickupgroup`;
ALTER TABLE `sccpline` ADD `namedpickupgroup` VARCHAR(100) DEFAULT "" AFTER `namedcallgroup`;
ALTER TABLE `sccpline` DROP COLUMN `rtptos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_tos`;
ALTER TABLE `sccpline` DROP COLUMN `audio_cos`;
ALTER TABLE `sccpline` DROP COLUMN `video_tos`;
ALTER TABLE `sccpline` DROP COLUMN `video_cos`;
update sccpdevice set audio_tos="0xB8",audio_cos="6",video_tos="0x88",video_cos="5" where audio_tos=NULL or audio_tos="";

ALTER TABLE `sccpdevice` ADD `backgroundImage` varchar(255) DEFAULT NULL;
ALTER TABLE `sccpdevice` ADD `ringtone` varchar(255) DEFAULT NULL;

ALTER TABLE `sccpline` DROP COLUMN `id`;
ALTER TABLE sccpline ADD COLUMN id MEDIUMINT NOT NULL AUTO_INCREMENT FIRST, ADD INDEX Index2 (id)
ALTER TABLE `sccpline` ADD UNIQUE(`id`);
ALTER TABLE `sccpline` CHANGE COLUMN `pin` varchar(7);
ALTER TABLE `sccpdevice` CHANGE COLUMN `type` varchar(15);
ALTER TABLE `sccpdevice` CHANGE COLUMN `imageversion` varchar(31);

CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', buttonconfig.type, buttonconfig.name, buttonconfig.options )
	ORDER BY instance ASC
	SEPARATOR ';' ) AS button, sccpdevice.*
	FROM sccpdevice
	LEFT JOIN buttonconfig ON ( buttonconfig.device = sccpdevice.name )
GROUP BY sccpdevice.name;