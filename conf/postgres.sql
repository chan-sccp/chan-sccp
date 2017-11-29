CREATE SCHEMA IF NOT EXISTS sccp;

--
-- sccpdevice
-- 
-- Add/Remove columns if needed, check sccp.conf.annotated for valid column entries
--
DROP TABLE IF EXISTS sccpdevice;
CREATE TABLE sccpdevice (
  type varchar(15) default NULL,
  addon varchar(45) default NULL,
  description varchar(45) default NULL,
  tzoffset varchar(5) default NULL,
  transfer varchar(5) default NULL,
  cfwdall varchar(5) default 'on',
  cfwdbusy varchar(5) default 'on',
  imageversion varchar(31) default NULL,
  deny varchar(100) default '0.0.0.0/0.0.0.0',
  permit varchar(100) default 'internal',
  dndFeature varchar(5) default 'on',
  directrtp varchar(3) default 'off',
  earlyrtp varchar(10) default 'progress',
  mwilamp varchar(5) default 'on',
  mwioncall varchar(5) default 'on',
  directed_pickup varchar(5) default 'on',
  directed_pickup_context varchar(100) default NULL,
  directed_pickup_modeanswer varchar(5) default 'on',
  private varchar(5) default NULL,
  privacy varchar(100) default 'full',
  nat varchar(7) NULL default 'auto',
  softkeyset varchar(100) NULL default NULL,
  audio_tos varchar(11) default NULL,
  audio_cos varchar(1) default NULL,
  video_tos varchar(11) default NULL,
  video_cos varchar(1) default NULL,
  disallow varchar(45) default NULL,
  allow varchar(45) default NULL,
  conf_allow varchar(3) NULL default 'on',
  conf_play_general_announce varchar(3) NULL default 'on',
  conf_play_part_announce varchar(3) NULL default 'on',   
  conf_mute_on_entry varchar(3) NULL default 'off',
  conf_music_on_hold_class varchar(80) NULL default 'default',
  conf_show_conflist varchar(3) NULL default 'on',
  force_dtmfmode varchar(8) NULL default 'auto',
  backgroundImage varchar(255) DEFAULT NULL,
  ringtone varchar(255) DEFAULT NULL,
  setvar varchar(100) default NULL,
  name varchar(15) NOT NULL default '',
  PRIMARY KEY  (name)
);

--
-- sccpline
-- 
-- Add/Remove columns if needed, check sccp.conf.annotated for valid column entries
--
CREATE SEQUENCE sccpline_id_seq;
SELECT setval('sccpline_id_seq', 1);

DROP TABLE IF EXISTS sccpline;
CREATE TABLE sccpline (
  id smallint DEFAULT NEXTVAL('sccpline_id_seq') UNIQUE,
  pin varchar(7) default NULL,
  label varchar(45) default NULL,
  description varchar(45) default NULL,
  context varchar(45) default NULL,
  incominglimit varchar(45) default NULL,
  transfer varchar(45) default NULL,
  mailbox varchar(45) default NULL,
  vmnum varchar(45) default NULL,
  cid_name varchar(45) default NULL,
  cid_num varchar(45) default NULL,
  trnsfvm varchar(45) default NULL,
  secondary_dialtone_digits varchar(45) default NULL,
  secondary_dialtone_tone varchar(45) default NULL,
  musicclass varchar(45) default NULL,
  language varchar(45) default NULL,
  accountcode varchar(45) default NULL,
  echocancel varchar(45) default NULL,
  silencesuppression varchar(45) default NULL,
  callgroup varchar(45) default NULL,
  pickupgroup varchar(45) default NULL,
  namedcallgroup varchar(100) default NULL,
  namedpickupgroup varchar(100) default NULL,
  dnd varchar(7) default 'reject',
  amaflags varchar(45) default NULL,
  regexten character varying(20) DEFAULT NULL,
  setvar varchar(50) default NULL,
  name varchar(40) NOT NULL,
  PRIMARY KEY (name)
);

--
-- Enum buttontype
--
DROP TYPE IF EXISTS buttontype;
CREATE TYPE buttontype AS ENUM ('line','speeddial','service','feature','empty');

--
-- buttonconfig table
-- foreign constrainst:
--   device -> sccpdevice.name
--   type -> buttontype enum
--   name -> if type=='line' then sccpline.name
--           else free field
-- unique constraints:
--   device, instance, type
--
DROP TABLE IF EXISTS buttonconfig;
CREATE TABLE buttonconfig(
  device character varying(15) NOT NULL,
  instance integer NOT NULL DEFAULT 0,
  "type" buttontype NOT NULL DEFAULT 'line'::buttontype,
  "name" character varying(36) DEFAULT NULL::character varying,
  options character varying(100) DEFAULT NULL::character varying,
  CONSTRAINT buttonconfig_pkey PRIMARY KEY (device, instance),
  CONSTRAINT devicename FOREIGN KEY (device)
      REFERENCES sccpdevice ("name") MATCH SIMPLE
      ON UPDATE NO ACTION ON DELETE NO ACTION
  UNIQUE (device, instance)    
);

--
-- concatenation/aggregation function
-- used by sccpdeviceconfig view
--
DROP AGGREGATE IF EXISTS textcat_column("text");
CREATE AGGREGATE textcat_column("text") (
  SFUNC=textcat,
  STYPE=text
);

--
-- sccpdeviceconfig view
--
-- combines sccpdevice and buttonconfig on buttonconfig.device=sccpdevice.name to 
-- produce a complete chan-sccp-b device entry including multiple buttons seperated by comma's
--
-- When altering sccpdevice or buttonconfig, this view needs to be dropped and recreated afterwards
--
CREATE OR REPLACE VIEW sccpdeviceconfig AS
        SELECT 
                (SELECT textcat_column(bc.type || ',' || bc.name || COALESCE(',' || bc.options, '') || ';') FROM (SELECT * FROM buttonconfig WHERE device=sccpdevice.name ORDER BY instance) bc ) as button,
                sccpdevice.*
        FROM sccpdevice
;
