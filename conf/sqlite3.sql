/*
 * this is for users how like to sepatet device and button configuration
 * You have to change the table names to:
 *
 * sccpdevices -> sccpdeviceconfig
 * sccplines -> sccpline  
*/

PRAGMA auto_vacuum=2;
PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
--
-- Table with line-configuration
--
CREATE TABLE IF NOT EXISTS sccpdevice (
  type 				varchar(15) 	DEFAULT NULL,
  addon 			varchar(45) 	DEFAULT NULL,
  description	 		varchar(45) 	DEFAULT NULL,
  tzoffset 			varchar(5) 	DEFAULT NULL,
  transfer 			varchar(5) 	DEFAULT 'on',
  cfwdall 			varchar(5) 	DEFAULT 'on',
  cfwdbusy 			varchar(5) 	DEFAULT 'on',
  imageversion			varchar(31) 	DEFAULT NULL,
  deny 				varchar(100) 	DEFAULT '0.0.0.0/0.0.0.0',
  permit 			varchar(100) 	DEFAULT 'internal',
  dndFeature 			varchar(5) 	DEFAULT 'on',
  directrtp 			varchar(3) 	DEFAULT 'off',
  earlyrtp 			varchar(8) 	DEFAULT 'progress',
  mwilamp 			varchar(5) 	DEFAULT 'on',
  mwioncall 			varchar(5) 	DEFAULT 'off',
  directed_pickup		varchar(5) 	DEFAULT 'on',
  directed_pickup_context 	varchar(100) 	DEFAULT NULL,
  directed_pickup_modeanswer 	varchar(5) 	DEFAULT 'on',
  private 			varchar(5) 	DEFAULT NULL,
  privacy 			varchar(100) 	DEFAULT 'full',
  nat 				varchar(7) 	DEFAULT 'auto',
  softkeyset 			varchar(100) 	DEFAULT '',
  audio_tos 			varchar(11) 	DEFAULT NULL,
  audio_cos 			varchar(1) 	DEFAULT NULL,
  video_tos 			varchar(11) 	DEFAULT NULL,
  video_cos 			varchar(1) 	DEFAULT NULL,
  conf_allow			varchar(3)	DEFAULT 'on',
  conf_play_general_announce	varchar(3)	DEFAULT 'on',
  conf_play_part_announce	varchar(3)	DEFAULT 'on',   
  conf_mute_on_entry		varchar(3)	DEFAULT 'off',
  conf_music_on_hold_class      varchar(80)	DEFAULT 'default',
  conf_show_conflist            varchar(3)      DEFAULT 'on',
  force_dtmfmode		varchar(8)	DEFAULT 'auto',
  backgroundImage		varchar(255) 	DEFAULT '',
  backgroundThumbnail		varchar(255) 	DEFAULT '',
  ringtone			varchar(255)	DEFAULT '',
  setvar 			varchar(100) 	DEFAULT NULL,
  name 				varchar(15) 	DEFAULT '' NOT NULL,
  PRIMARY KEY  (name)
);

--
-- Table with device-configuration
--
CREATE TABLE IF NOT EXISTS sccpline (
  id 				INTEGER 	PRIMARY KEY AUTOINCREMENT,
  pin 				varchar(7) 	DEFAULT NULL,
  label 			varchar(45) 	DEFAULT NULL,
  description 			varchar(45) 	DEFAULT NULL,
  context 			varchar(45) 	DEFAULT '' NOT NULL,
  incominglimit			varchar(45) 	DEFAULT NULL,
  transfer 			varchar(45) 	DEFAULT NULL,
  mailbox 			varchar(45) 	DEFAULT NULL,
  vmnum 			varchar(45) 	DEFAULT NULL,
  cid_name 			varchar(45) 	DEFAULT NULL,
  cid_num 			varchar(45) 	DEFAULT '' NOT NULL,
  disallow 			varchar(255) 	DEFAULT NULL,
  allow 			varchar(255) 	DEFAULT NULL,
  trnsfvm 			varchar(45) 	DEFAULT NULL,
  secondary_dialtone_digits 	varchar(45) 	DEFAULT NULL,
  secondary_dialtone_tone 	varchar(45) 	DEFAULT NULL,
  musicclass 			varchar(45) 	DEFAULT NULL,
  language 			varchar(45) 	DEFAULT NULL,
  accountcode 			varchar(45) 	DEFAULT NULL,
  echocancel 			varchar(45) 	DEFAULT NULL,
  silencesuppression 		varchar(45) 	DEFAULT NULL,
  callgroup 			varchar(45) 	DEFAULT NULL,
  pickupgroup 			varchar(45) 	DEFAULT NULL,
  namedcallgroup 		varchar(100) 	DEFAULT NULL,
  namedpickupgroup 		varchar(100) 	DEFAULT NULL,
  dnd 				varchar(7) 	DEFAULT 'reject',
  amaflags 			varchar(45) 	DEFAULT NULL,
  setvar 			varchar(50) 	DEFAULT NULL,
  name 				varchar(45) 	DEFAULT '' NOT NULL,
  UNIQUE (name)
);

CREATE TABLE IF NOT EXISTS buttontype (
  type 				varchar(9) 	DEFAULT NULL,
  PRIMARY KEY (type)
);

INSERT OR REPLACE INTO buttontype (type) VALUES ('line');
INSERT OR REPLACE INTO buttontype (type) VALUES ('speeddial');
INSERT OR REPLACE INTO buttontype (type) VALUES ('service');
INSERT OR REPLACE INTO buttontype (type) VALUES ('feature');
INSERT OR REPLACE INTO buttontype (type) VALUES ('empty');
--
-- Table with button-configuration for device
--
CREATE TABLE IF NOT EXISTS buttonconfig (
  device 			varchar(15) 	DEFAULT '' NOT NULL,
  instance 			tinyint(4) 	DEFAULT '0' NOT NULL,
  type 				varchar(9)	DEFAULT 'line' NOT NULL,
  name 				varchar(36) 	DEFAULT NULL,
  options			varchar(100) 	DEFAULT NULL,
  PRIMARY KEY  (device,instance),
  FOREIGN KEY (device) REFERENCES sccpdevice (device),
  FOREIGN KEY (type) REFERENCES buttontype (type) 
);

--
-- View for merging device and button configuration
--
DROP VIEW IF EXISTS sccpdeviceconfig;
CREATE VIEW sccpdeviceconfig AS 
	SELECT 	sccpdevice.*, 
		group_concat(buttonconfig.type||","||buttonconfig.name||","||buttonconfig.options,";") as button 
	FROM buttonconfig, sccpdevice 
	WHERE buttonconfig.device=sccpdevice.name 
	GROUP BY sccpdevice.name
	ORDER BY sccpdevice.name, buttonconfig.instance;
COMMIT;
PRAGMA foreign_keys=ON;

