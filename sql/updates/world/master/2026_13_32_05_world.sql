-- Neighborhood Laborer (244850): retail holds item 187518 (fist weapon placeholder shown by the work emotes) in all 10 sniffed creates
-- (sniff 12.1.5.70575 2026-10-23_14-37-06); entry 246336 already has it, 244850 had no equipment rows.
DELETE FROM `creature_equip_template` WHERE `CreatureID`=244850 AND `ID`=1;
INSERT INTO `creature_equip_template` (`CreatureID`,`ID`,`ItemID1`,`AppearanceModID1`,`ItemVisual1`,`ItemID2`,`AppearanceModID2`,`ItemVisual2`,`ItemID3`,`AppearanceModID3`,`ItemVisual3`,`VerifiedBuild`) VALUES
(244850,1,187518,0,0,0,0,0,0,0,0,70575);
UPDATE `creature` SET `equipment_id`=1 WHERE `id`=244850 AND `equipment_id`=0;
