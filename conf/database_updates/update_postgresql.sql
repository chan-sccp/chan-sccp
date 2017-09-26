
DROP VIEW sccpdeviceconfig;

ALTER TABLE sccpdevice alter earlyrtp type varchar(10);
ALTER TABLE sccpdevice alter deny type varchar(100);
ALTER TABLE sccpdevice alter permit type varchar(100);
ALTER TABLE sccpline alter dnd type varchar(7);
ALTER TABLE sccpline alter dnd set default 'reject';
ALTER TABLE sccpdevice alter nat type varchar(7);
ALTER TABLE sccpdevice alter nat set default 'auto';
ALTER TABLE sccpdevice DROP COLUMN dtmfmode;
ALTER TABLE sccpline ADD namedcallgroup VARCHAR(100) DEFAULT "" AFTER pickupgroup;
ALTER TABLE sccpline ADD namedpickupgroup VARCHAR(100) DEFAULT "" AFTER namedcallgroup;

CREATE OR REPLACE VIEW sccpdeviceconfig AS
        SELECT 
                (SELECT textcat_column(bc.type || ',' || bc.name || COALESCE(',' || bc.options, '') || ';') FROM (SELECT * FROM buttonconfig WHERE device=sccpdevice.name ORDER BY instance) bc ) as button,
                sccpdevice.*
        FROM sccpdevice
;
                                                