DROP VIEW sccpdeviceconfig;
ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` CHANGE COLUMN `nat` VARCHAR( 7 ) DEFAULT "auto";
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
ALTER TABLE `sccpdevice` ADD COLUMN `privacy` ENUM('full','on','true','yes','off','false','no') default 'full' AFTER `private`;

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
ALTER TABLE `sccpline` ADD COLUMN id MEDIUMINT NOT NULL AUTO_INCREMENT FIRST, ADD INDEX Index2 (id)
ALTER TABLE `sccpline` ADD UNIQUE(`id`);
ALTER TABLE `sccpline` CHANGE COLUMN `pin` varchar(7);
ALTER TABLE `sccpdevice` CHANGE COLUMN `type` varchar(15);
ALTER TABLE `sccpdevice` CHANGE COLUMN `imageversion` varchar(31);

ALTER TABLE `sccpdevice` CHANGE COLUMN `pickupexten` `directed_pickup` VARCHAR(5) NOT NULL default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `pickupcontext` `directed_pickup_context` VARCHAR(100) default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `pickupmodeanswer` `directed_pickup_modeanswer` VARCHAR(5) NOT NULL default 'on';

ALTER TABLE `sccpdevice` CHANGE COLUMN `earlyrtp` ENUM('immediate','offhook','dialing','ringout','progress','none') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `mwilamp` ENUM('on','true','yes','off','false','no','wink','flash','blink') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `mwioncall` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `directed_pickup` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `directed_pickup_modeanswer` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `dndFeature` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `transfer` ENUM('on','true','yes','off','false','no') default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `cfwdall` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `cfwdbusy` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `privacy` ENUM('full','on','true','yes','off','false','no') default 'full';
ALTER TABLE `sccpdevice` CHANGE COLUMN `nat` ENUM('on','true','yes','off','false','no','auto') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `directrtp` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpdevice` CHANGE COLUMN `conf_allow` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `conf_play_part_announce` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';
ALTER TABLE `sccpdevice` CHANGE COLUMN `conf_mute_on_entry` ENUM('on','true','yes','off','false','no') NOT NULL default 'off';
ALTER TABLE `sccpdevice` CHANGE COLUMN `conf_show_conflist` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';

ALTER TABLE `sccpline` CHANGE COLUMN `transfer` ENUM('on','true','yes','off','false','no') NOT NULL default 'on';
ALTER TABLE `sccpline` CHANGE COLUMN `echocancel` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpline` CHANGE COLUMN `silencesuppression` ENUM('on','true','yes','off','false','no') default NULL;
ALTER TABLE `sccpline` CHANGE COLUMN `dnd` ENUM('off','reject','silent','user') NOT NULL default 'reject';

CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', buttonconfig.type, buttonconfig.name, buttonconfig.options )
	ORDER BY instance ASC
	SEPARATOR ';' ) AS button, sccpdevice.*
	FROM sccpdevice
	LEFT JOIN buttonconfig ON ( buttonconfig.device = sccpdevice.name )
GROUP BY sccpdevice.name;