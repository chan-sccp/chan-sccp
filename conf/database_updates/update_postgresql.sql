
DROP sccpdeviceconfig;

ALTER TABLE sccpdevice alter earlyrtp type varchar(10);
ALTER TABLE sccpline alter dnd type varchar(7) default "reject";
ALTER TABLE `sccpdevice` DROP COLUMN `dtmfmode`;

CREATE OR REPLACE VIEW sccpdeviceconfig AS
        SELECT 
              (SELECT textcat_column(bc.type || ',' || bc.name || COALESCE(',' || bc.options, '') || ';') FROM (SELECT * FROM buttonconfig WHERE device=sccpdevice.name ORDER BY instance) bc ) as button,
              sccpdevice.*
        FROM sccpdevice
;
                                                