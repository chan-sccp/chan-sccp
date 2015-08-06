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
  `type` varchar(45) default NULL,
  `addon` varchar(45) default NULL,
  `description` varchar(45) default NULL,
  `tzoffset` varchar(5) default NULL,
  `transfer` varchar(5) default 'on',
  `cfwdall` varchar(5) default 'on',
  `cfwdbusy` varchar(5) default 'on',
  `imageversion` varchar(45) default NULL,
  `deny` varchar(45) default NULL,
  `permit` varchar(45) default NULL,
  `dndFeature` varchar(5) default 'on',
  `directrtp` varchar(3) default 'off',
  `earlyrtp` varchar(10) default 'progress',
  `mwilamp` varchar(5) default 'on',
  `mwioncall` varchar(5) default 'off',
  `pickupexten` varchar(5) default 'on',
  `pickupcontext` varchar(100) default '',
  `pickupmodeanswer` varchar(5) default 'on',
  `private` varchar(5) default 'off',
  `privacy` varchar(100) default 'full',
  `nat` varchar(4) default 'auto',
  `softkeyset` varchar(100) default '',
  `audio_tos` varchar(11) default NULL,
  `audio_cos` varchar(1) default NULL,
  `video_tos` varchar(11) default NULL,
  `video_cos` varchar(1) default NULL,
  `conf_allow` varchar(3) default 'on',
  `conf_play_general_announce` varchar(3) default 'on',
  `conf_play_part_announce` varchar(3) default 'on',   
  `conf_mute_on_entry` varchar(3) default 'off',
  `conf_music_on_hold_class` varchar(80) default 'default',
  `conf_show_conflist`  varchar(3) default 'on',
  `setvar` varchar(100) default NULL,
  `disallow` varchar(255) DEFAULT NULL,
  `allow` varchar(255) DEFAULT NULL,
  `backgroundImage` varchar(255) DEFAULT NULL,
  `ringtone` varchar(255) DEFAULT NULL,
  `name` varchar(15) NOT NULL default '',
  PRIMARY KEY  (`name`)
) ENGINE=INNODB DEFAULT CHARSET=latin1;


--
-- Table with device-configuration
--
CREATE TABLE IF NOT EXISTS `sccpline` (
  `id` varchar(4) default NULL,
  `pin` varchar(45) default NULL,
  `label` varchar(45) default NULL,
  `description` varchar(45) default NULL,
  `context` varchar(45) default NULL,
  `incominglimit` varchar(45) default NULL,
  `transfer` varchar(45) default NULL,
  `mailbox` varchar(45) default NULL,
  `vmnum` varchar(45) default NULL,
  `cid_name` varchar(45) default NULL,
  `cid_num` varchar(45) default NULL,
  `trnsfvm` varchar(45) default NULL,
  `secondary_dialtone_digits` varchar(45) default NULL,
  `secondary_dialtone_tone` varchar(45) default NULL,
  `musicclass` varchar(45) default NULL,
  `language` varchar(45) default NULL,
  `accountcode` varchar(45) default NULL,
  `echocancel` varchar(45) default NULL,
  `silencesuppression` varchar(45) default NULL,
  `callgroup` varchar(45) default NULL,
  `pickupgroup` varchar(45) default NULL,
  `amaflags` varchar(45) default NULL,
  `dnd` varchar(7) default 'reject',
  `setvar` varchar(50) default NULL,
  `name` varchar(45) NOT NULL default '',
  PRIMARY KEY  (`name`)
) ENGINE=INNODB DEFAULT CHARSET=latin1;

--
-- Table with button-configuration for device
--
-- foreign constrainst:
--   device -> sccpdevice.name
--   type -> buttontype enum
--   name -> if type=='line' then sccpline.name
--           else free field
-- unique constraints:
--   device, instance, type
--
CREATE TABLE IF NOT EXISTS `buttonconfig` (
  `device` varchar(15) NOT NULL default '',
  `instance` tinyint(4) NOT NULL default '0',
  `type` enum('line','speeddial','service','feature','empty') NOT NULL default 'line',
  `name` varchar(36) default NULL,
  `options` varchar(100) default NULL,
  PRIMARY KEY  (`device`,`instance`,`type`),
  KEY `device` (`device`),
  FOREIGN KEY (device) REFERENCES sccpdevice(name) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=INNODB DEFAULT CHARSET=latin1;


--
-- trigger to check buttonconfig sccpline foreign key constrainst:
--   if type=='line' then check name against sccpline.name column
--   else free field
--
DROP TRIGGER IF EXISTS trg_buttonconfig;

DELIMITER $$
CREATE TRIGGER trg_buttonconfig BEFORE INSERT ON buttonconfig
FOR EACH ROW
BEGIN
	IF NEW.`type` = 'line' THEN
		IF (SELECT COUNT(*) FROM `sccpline` WHERE `sccpline`.`name` = NEW.`name`) = 0
		THEN
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
CREATE OR REPLACE
ALGORITHM = MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', buttonconfig.type, buttonconfig.name, buttonconfig.options )
	ORDER BY instance ASC
	SEPARATOR ';' ) AS button, sccpdevice.*
	FROM sccpdevice
	LEFT JOIN buttonconfig ON ( buttonconfig.device = sccpdevice.name )
GROUP BY sccpdevice.name;



