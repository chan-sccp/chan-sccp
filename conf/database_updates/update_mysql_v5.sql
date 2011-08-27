ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` ADD `nat` VARCHAR( 5 ) NULL DEFAULT "no" BEFORE `name`;
ALTER TABLE `sccpdevice` CHANGE COLUMN 'earlyrtp' VARCHAR(8) default 'off';
