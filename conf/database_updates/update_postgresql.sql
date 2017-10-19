DROP VIEW sccpdeviceconfig;

ALTER TABLE sccpdevice alter earlyrtp type varchar(10);
ALTER TABLE sccpdevice alter earlyrtp SET DEFAULT 'PROGRESS';
ALTER TABLE sccpdevice alter deny type varchar(100);
ALTER TABLE sccpdevice alter deny SET DEFAULT '0.0.0.0/0.0.0.0';
ALTER TABLE sccpdevice alter permit type varchar(100);
ALTER TABLE sccpdevice alter permit SET DEFAULT 'internal';
ALTER TABLE sccpdevice alter nat type varchar(7);
ALTER TABLE sccpdevice alter nat set default 'auto';
ALTER TABLE sccpdevice DROP COLUMN dtmfmode;
ALTER TABLE sccpline alter dnd type varchar(7);
ALTER TABLE sccpline alter dnd set default 'reject';
ALTER TABLE sccpline ADD COLUMN namedcallgroup VARCHAR(100) DEFAULT NULL;
ALTER TABLE sccpline ADD COLUMN namedpickupgroup VARCHAR(100) DEFAULT NULL;
ALTER TABLE sccpdevice ADD COLUMN privacy VARCHAR(5) DEFAULT 'full';

ALTER TABLE sccpline ALTER COLUMN id DROP DEFAULT;
DROP SEQUENCE sccpline_id_seq;
CREATE SEQUENCE sccpline_id_seq;
SELECT setval('sccpline_id_seq', 1);
ALTER TABLE sccpline DROP CONSTRAINT sccpline_pkey;
ALTER TABLE sccpline DROP CONSTRAINT sccpline_id_key;
ALTER TABLE sccpline ALTER COLUMN id TYPE smallint USING (id::integer);
ALTER TABLE sccpline ALTER COLUMN name TYPE VARCHAR(40);
ALTER TABLE sccpline ADD PRIMARY KEY(name);
ALTER TABLE sccpline ADD UNIQUE(id);
ALTER TABLE sccpline ALTER id SET DEFAULT NEXTVAL('sccpline_id_seq');
UPDATE sccpline SET id = DEFAULT;
ALTER TABLE sccpline ALTER COLUMN pin TYPE varchar(7);
ALTER TABLE sccpdevice ALTER COLUMN type TYPE varchar(15);
ALTER TABLE sccpdevice ALTER COLUMN imageversion TYPE varchar(31);
ALTER TABLE sccpdevice RENAME COLUMN dnd TO dndFeature;
ALTER TABLE sccpdevice RENAME COLUMN pickupexten directed_pickup;
ALTER TABLE sccpdevice RENAME COLUMN pickupcontext directed_pickup_context;
ALTER TABLE sccpdevice RENAME COLUMN pickupmodeanswer directed_pickup_modeanswer;

CREATE OR REPLACE VIEW sccpdeviceconfig AS
        SELECT 
                (SELECT textcat_column(bc.type || ',' || bc.name || COALESCE(',' || bc.options, '') || ';') FROM (SELECT * FROM buttonconfig WHERE device=sccpdevice.name ORDER BY instance) bc ) as button,
                sccpdevice.*
        FROM sccpdevice
;
