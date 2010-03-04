ALTER TABLE `sccpdevice` ADD `softkeyset` VARCHAR( 50 ) NULL DEFAULT NULL AFTER `addon`;

--
-- View for merging device and button configuration
--
CREATE OR REPLACE
ALGORITHM =
MERGE
VIEW sccpdeviceconfig AS
	SELECT GROUP_CONCAT( CONCAT_WS( ',', buttonconfig.type, buttonconfig.name, buttonconfig.options )
	ORDER BY instance ASC
	SEPARATOR ';' ) AS button, sccpdevice . *
	FROM sccpdevice
	LEFT JOIN buttonconfig ON ( buttonconfig.device = sccpdevice.name )
GROUP BY sccpdevice.name;