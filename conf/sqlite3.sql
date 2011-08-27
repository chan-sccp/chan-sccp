/*
 * this is for users how like to sepatet device and button configuration
 * You have to change the table names to:
 *
 * sccpdevices -> sccpdeviceconfig
 * sccplines -> sccpline  
*/

PRAGMA auto_vacuum=2;
--
-- Table with line-configuration
--
CREATE TABLE sccpdevice (
  "type" 			varchar(45) 	DEFAULT NULL,
  "addon" 			varchar(45) 	DEFAULT NULL,
  "description"	 		varchar(45) 	DEFAULT NULL,
  "tzoffset" 			varchar(5) 	DEFAULT NULL,
  "transfer" 			varchar(5) 	DEFAULT 'on',
  "cfwdall" 			varchar(5) 	DEFAULT 'on',
  "cfwdbusy" 			varchar(5) 	DEFAULT 'on',
  "dtmfmode" 			varchar(10) 	DEFAULT NULL,
  "imageversion"		varchar(45) 	DEFAULT NULL,
  "deny" 			varchar(45) 	DEFAULT NULL,
  "permit" 			varchar(45) 	DEFAULT NULL,
  "dndFeature" 			varchar(5) 	DEFAULT 'on',
  "trustphoneip" 		varchar(5) 	DEFAULT NULL,
  "directrtp" 			varchar(3) 	DEFAULT 'off',
  "earlyrtp" 			varchar(8) 	DEFAULT 'off',
  "mwilamp" 			varchar(5) 	DEFAULT 'on',
  "mwioncall" 			varchar(5) 	DEFAULT 'off',
  "pickupexten"			varchar(5) 	DEFAULT 'on',
  "pickupcontext" 		varchar(100) 	DEFAULT '',
  "pickupmodeanswer" 		varchar(5) 	DEFAULT 'on',
  "private" 			varchar(5) 	DEFAULT 'off',
  "privacy" 			varchar(100) 	DEFAULT 'full',
  "nat" 			varchar(15) 	DEFAULT 'off',
  "softkeyset" 			varchar(100) 	DEFAULT '',
  "setvar" 			varchar(100) 	DEFAULT NULL,
  "disallow" 			varchar(255) 	DEFAULT NULL,
  "allow" 			varchar(255) 	DEFAULT NULL,
  "name" 			varchar(15) 	NOT NULL DEFAULT '',
  PRIMARY KEY  ("name")
);


--
-- Table with device-configuration
--
CREATE TABLE "sccpline" (
  "id" 				varchar(45) 	DEFAULT NULL,
  "pin" 			varchar(45) 	DEFAULT NULL,
  "label" 			varchar(45) 	DEFAULT NULL,
  "description" 		varchar(45) 	DEFAULT NULL,
  "context" 			varchar(45) 	DEFAULT NULL,
  "incominglimit"		varchar(45) 	DEFAULT NULL,
  "transfer" 			varchar(45) 	DEFAULT NULL,
  "mailbox" 			varchar(45) 	DEFAULT NULL,
  "vmnum" 			varchar(45) 	DEFAULT NULL,
  "cid_name" 			varchar(45) 	DEFAULT NULL,
  "cid_num" 			varchar(45) 	DEFAULT NULL,
  "trnsfvm" 			varchar(45) 	DEFAULT NULL,
  "secondary_dialtone_digits" 	varchar(45) 	DEFAULT NULL,
  "secondary_dialtone_tone" 	varchar(45) 	DEFAULT NULL,
  "musicclass" 			varchar(45) 	DEFAULT NULL,
  "language" 			varchar(45) 	DEFAULT NULL,
  "accountcode" 		varchar(45) 	DEFAULT NULL,
  "audio_tos" 			varchar(11) 	DEFAULT NULL,
  "audio_cos" 			varchar(1) 	DEFAULT NULL,
  "video_tos" 			varchar(11) 	DEFAULT NULL,
  "video_cos" 			varchar(1) 	DEFAULT NULL,
  "echocancel" 			varchar(45) 	DEFAULT NULL,
  "silencesuppression" 		varchar(45) 	DEFAULT NULL,
  "callgroup" 			varchar(45) 	DEFAULT NULL,
  "pickupgroup" 		varchar(45) 	DEFAULT NULL,
  "dnd" 			varchar(5) 	DEFAULT 'on',
  "amaflags" 			varchar(45) 	DEFAULT NULL,
  "defaultSubscriptionId_number" varchar(5)	DEFAULT NULL,
  "setvar" 			varchar(50) 	DEFAULT NULL,
  "name" 			varchar(45) 	NOT NULL DEFAULT '',
  PRIMARY KEY  ("name")
);

CREATE TABLE "buttontype" (
  "type" 			varchar(9) 	DEFAULT NULL,
  PRIMARY KEY ("type")
);

INSERT INTO buttontype (type) VALUES ('line');
INSERT INTO buttontype (type) VALUES ('speeddial');
INSERT INTO buttontype (type) VALUES ('service');
INSERT INTO buttontype (type) VALUES ('feature');
INSERT INTO buttontype (type) VALUES ('empty');
--
-- Table with button-configuration for device
--
CREATE TABLE "buttonconfig" (
  "device" 			varchar(15) 	NOT NULL DEFAULT '',
  "instance" 			tinyint(4) 	NOT NULL DEFAULT '0',
  "type" 			varchar(9),
  "name" 			varchar(36) 	DEFAULT NULL,
  "options"			varchar(100) 	DEFAULT NULL,
  PRIMARY KEY  ("device","instance"),
  FOREIGN KEY (device) REFERENCES sccpdevice (device),
  FOREIGN KEY (type) REFERENCES buttontype (type) 
);

--
-- View for merging device and button configuration
--
CREATE VIEW sccpdeviceconfig AS 
	SELECT 	sccpdevice.*, 
		group_concat(buttonconfig.type||","||buttonconfig.name||","||buttonconfig.options,";") as button 
	FROM buttonconfig, sccpdevice 
	WHERE buttonconfig.device=sccpdevice.name 
	ORDER BY instance;

