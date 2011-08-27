ALTER TABLE `sccpdevice` ADD `pickupcontext` VARCHAR( 100 ) NULL DEFAULT "" AFTER `pickupexten`;
ALTER TABLE `sccpdevice` ADD `nat` VARCHAR( 5 ) NULL DEFAULT "no" BEFORE `name`;
ALTER TABLE `sccpline` CHANGE COLUMN `dnd` `dndFeature`;
ALTER TABLE `sccpline` ADD `dnd` VARCHAR( 5 ) DEFAULT "on" AFTER `amaflags`;
ALTER TABLE `sccpdevice` CHANGE COLUMN `earlyrtp` VARCHAR(8);
