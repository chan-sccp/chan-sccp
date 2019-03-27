--
-- this is for users how like to sepatet device and button configuration
-- You have to change the table names to:
--
-- sccpdevices -> sccpdeviceconfig
-- sccplines -> sccpline  
--

--
-- Table with line-configuration
--
CREATE TABLE IF NOT EXISTS `sccpdevice` (
  `type` varchar(15) default NULL,
  `addon` varchar(45) default NULL,
  `description` varchar(45) default NULL,
  `tzoffset` varchar(5) default NULL,
  `imageversion` varchar(31) default NULL,
  `deny` varchar(100) default '0.0.0.0/0.0.0.0',
  `permit` varchar(100) default 'internal',
  `earlyrtp` ENUM('immediate','offhook','dialing','ringout','progress','none') default NULL,
  `mwilamp` ENUM('on','true','yes','off','false','no','wink','flash','blink') default NULL,
  `mwioncall` ENUM('on','true','yes','off','false','no') default NULL,
  `dndFeature` ENUM('on','true','yes','off','false','no') default NULL,
  `transfer` ENUM('on','true','yes','off','false','no') default 'on',
  `cfwdall` ENUM('on','true','yes','off','false','no') default NULL,
  `cfwdbusy` ENUM('on','true','yes','off','false','no') default NULL,
  `private` ENUM('on','true','yes','off','false','no') default NULL,
  `privacy` ENUM('full','on','true','yes','off','false','no') default 'full',
  `nat` ENUM('on','true','yes','off','false','no','auto') default NULL,
  `directrtp` ENUM('on','true','yes','off','false','no') default NULL,
  `softkeyset` varchar(100) default '',
  `audio_tos` varchar(11) default NULL,
  `audio_cos` varchar(1) default NULL,
  `video_tos` varchar(11) default NULL,
  `video_cos` varchar(1) default NULL,
  `conf_allow` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `conf_play_general_announce` varchar(3) default 'on',
  `conf_play_part_announce` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `conf_mute_on_entry` ENUM('on','true','yes','off','false','no') NOT NULL default 'off',
  `conf_music_on_hold_class` varchar(80) default 'default',
  `conf_show_conflist` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `force_dtmfmode` ENUM('auto','rfc2833','skinny') NOT NULL default 'auto',
  `setvar` varchar(100) default NULL,
  `backgroundImage` varchar(255) DEFAULT NULL,
  `ringtone` varchar(255) DEFAULT NULL,
  `name` varchar(15) NOT NULL default '',
  PRIMARY KEY  (`name`)
) ENGINE=INNODB DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

--
-- Table with device-configuration
--
CREATE TABLE IF NOT EXISTS `sccpline` (
  `id` MEDIUMINT NOT NULL AUTO_INCREMENT,
  `pin` varchar(7) default NULL,
  `label` varchar(45) default NULL,
  `description` varchar(45) default NULL,
  `context` varchar(45) default NULL,
  `incominglimit` TINYINT(2) default 6,
  `transfer` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `mailbox` varchar(45) default NULL,
  `vmnum` varchar(45) default NULL,
  `cid_name` varchar(45) default NULL,
  `cid_num` varchar(45) default NULL,
  `disallow` varchar(255) DEFAULT NULL,
  `allow` varchar(255) DEFAULT NULL,
  `trnsfvm` varchar(45) default NULL,
  `secondary_dialtone_digits` varchar(45) default NULL,
  `secondary_dialtone_tone` varchar(45) default NULL,
  `musicclass` varchar(45) default NULL,
  `language` varchar(45) default NULL,
  `accountcode` varchar(45) default NULL,
  `echocancel` ENUM('on','true','yes','off','false','no') default NULL,
  `silencesuppression` ENUM('on','true','yes','off','false','no') default NULL,
  `callgroup` varchar(45) default NULL,
  `pickupgroup` varchar(45) default NULL,
  `namedcallgroup` varchar(100) default NULL,
  `namedpickupgroup` varchar(100) default NULL,
  `directed_pickup` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `directed_pickup_context` varchar(100) default NULL,
  `pickup_modeanswer` ENUM('on','true','yes','off','false','no') NOT NULL default 'on',
  `amaflags` varchar(45) default NULL,
  `dnd` ENUM('off','reject','silent','user') NOT NULL default 'reject',
  `setvar` varchar(50) default NULL,
  `name` varchar(40) NOT NULL default '',
  PRIMARY KEY  (`name`),
  UNIQUE (`id`)
) ENGINE=INNODB DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

--
-- Quick addition of sccpuser
--
CREATE TABLE IF NOT EXISTS `sccpuser` (
  `id` varchar(8) NOT NULL,
  `name` varchar(45) NOT NULL,
  `pin` varchar(8) default NULL,
  `password` varchar(45) default NULL,
  PRIMARY KEY  (`name`),
  UNIQUE (`id`)
) ENGINE=INNODB DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

--
-- Table with button-configuration for device
--
-- foreign constrainst:
--        ref -> sccpdevice.name or sccpline.id
--    reftype -> is this a device-button or a user-button
--   instance -> line number to associate this button with
-- buttontype -> buttontype enum
--       name -> if type=='line' then sccpline.name
--               else free field
-- unique constraints:
--   device, instance, type
--
CREATE TABLE IF NOT EXISTS `sccpbuttonconfig` (
  `ref` varchar(15) NOT NULL default '',
  `reftype` enum('sccpdevice', 'sccpuser') NOT NULL default 'sccpdevice',
  `instance` tinyint(4) NOT NULL default 0,
  `buttontype` enum('line','speeddial','service','feature','empty') NOT NULL default 'line',
  `name` varchar(36) default NULL,
  `options` varchar(100) default NULL,
  PRIMARY KEY  (`ref`,`reftype`,`instance`,`buttontype`),
  KEY `ref` (`ref`,`reftype`)
-- , FOREIGN KEY (device) REFERENCES sccpdevice(name) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=INNODB DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

-- trigger to check buttonconfig sccpline foreign key constrainst:
--   if type=='line' then check name against sccpline.name column
--   else free field
--
DROP TRIGGER IF EXISTS trg_buttonconfig;
DELIMITER $$
CREATE TRIGGER trg_buttonconfig BEFORE INSERT ON sccpbuttonconfig
FOR EACH ROW
BEGIN
	IF NEW.`reftype` = 'sccpdevice' THEN
		IF (SELECT COUNT(*) FROM `sccpdevice` WHERE `sccpdevice`.`name` = NEW.`ref`) = 0
		THEN
			UPDATE `Foreign key contraint violated: ref does not exist in sccpdevice` SET x=1;
		END IF;
        END IF;		
	IF NEW.`reftype` = 'sccpuser' THEN
		IF (SELECT COUNT(*) FROM `sccpuser` WHERE `sccpuser`.`name` = NEW.`ref`) = 0
		THEN
			UPDATE `Foreign key contraint violated: ref does not exist in sccpuser` SET x=1;
		END IF;
	END IF;
	IF NEW.`buttontype` = 'line' THEN
   	IF NEW.`buttontype` = 'line' THEN
        	SET @line_x = SUBSTRING_INDEX(NEW.`name`,'!',1);
        	SET @line_x = SUBSTRING_INDEX(@line_x,'@',1);
        	IF (SELECT COUNT(*) FROM `sccpline` WHERE `sccpline`.`name` = @line_x ) = 0 THEN
            		UPDATE `Foreign key contraint violated: line does not exist in sccpline` SET x=1;
        	END IF;
   	END IF;
END$$
DELIMITER ;

--
-- View for merging device and button configuration
--
-- combines sccpdevice and buttonconfig on buttonconfig.device=sccpdevice.name to 
-- produce a complete chan-sccp-b device entry including multiple buttons seperated by comma's
--
-- When altering sccpdevice or buttonconfig, this view needs to be dropped and recreated afterwards
--
--
-- group_concat_max_len = 2048; in my.cnf
--

CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', sccpbuttonconfig.buttontype, sccpbuttonconfig.name, sccpbuttonconfig.options )
	ORDER BY instance ASC SEPARATOR ';' ) AS sccpbutton, sccpdevice.*
	FROM sccpdevice
	LEFT JOIN sccpbuttonconfig ON ( 
	  sccpbuttonconfig.reftype = 'sccpdevice' AND
	  sccpbuttonconfig.ref = sccpdevice.name )
GROUP BY sccpdevice.name;

CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpuserconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', sccpbuttonconfig.buttontype, sccpbuttonconfig.name, sccpbuttonconfig.options )
	ORDER BY instance ASC SEPARATOR ';' ) AS button, sccpuser.*
	FROM sccpuser
	LEFT JOIN sccpbuttonconfig ON (
	    sccpbuttonconfig.reftype = 'sccpuser' AND
            sccpbuttonconfig.ref = sccpuser.id)
GROUP BY sccpuser.name;


