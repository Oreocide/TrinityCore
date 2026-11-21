-- Covenant join: Choose your Covenant (choice 644) and the Shadowlands covenant map in the Ring of Fates (retail 12.1.5.70961)
UPDATE `playerchoice` SET `ScriptName` = 'playerchoice_covenant_join' WHERE `ChoiceId` = 644;

UPDATE `playerchoice_response` SET `VerifiedBuild` = 70961 WHERE `ChoiceId` = 644 AND `ResponseId` IN (2686, 2687, 2688, 2689, 2702, 2706, 2707, 2708);

UPDATE `gameobject_template` SET `ScriptName` = 'go_shadowlands_covenant_map' WHERE `entry` = 355352;

DELETE FROM `gameobject` WHERE `guid` = 60825833;
INSERT INTO `gameobject` (`guid`, `id`, `map`, `zoneId`, `areaId`, `spawnDifficulties`, `phaseUseFlags`, `PhaseId`, `PhaseGroup`, `terrainSwapMap`, `position_x`, `position_y`, `position_z`, `orientation`, `rotation0`, `rotation1`, `rotation2`, `rotation3`, `spawntimesecs`, `animprogress`, `state`, `ScriptName`, `StringId`, `VerifiedBuild`) VALUES
(60825833, 355352, 2222, 10565, 13499, '0', 0, 0, 0, -1, -1937.3507080078125, 1385.970458984375, 5269.28125, 5.505388259887695312, 0, 0, -0.37916946411132812, 0.925327241420745849, 120, 255, 1, '', NULL, 70961);

DELETE FROM `gameobject_addon` WHERE `guid` = 60825833;
INSERT INTO `gameobject_addon` (`guid`, `parent_rotation0`, `parent_rotation1`, `parent_rotation2`, `parent_rotation3`, `invisibilityType`, `invisibilityValue`, `WorldEffectID`, `AIAnimKitID`) VALUES
(60825833, 0, 0, 0, 1, 0, 0, 0, 6132);

DELETE FROM `spawn_tracking` WHERE `SpawnTrackingId` = 1998759 AND `SpawnType` = 1 AND `SpawnId` = 60825833;
INSERT INTO `spawn_tracking` (`SpawnTrackingId`, `SpawnType`, `SpawnId`, `QuestObjectiveIds`) VALUES
(1998759, 1, 60825833, '405264');

-- Covenant preview NPCs (quest 57878 objectives 405260-405263) and Iona Skyblade (quest 62790 objective 408640): quest credit on gossip select, as on retail
UPDATE `creature_template` SET `AIName` = 'SmartAI' WHERE `entry` IN (171589, 171787, 171795, 171821, 176100) AND `AIName` = '' AND `ScriptName` = '';

DELETE FROM `smart_scripts` WHERE `source_type` = 0 AND `entryorguid` IN (171589, 171787, 171795, 171821, 176100);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `Difficulties`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `event_param_string`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `action_param7`, `action_param_string`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_param_string`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(171787, 0, 0, 0, '', 62, 0, 100, 0, 26131, 0, 0, 0, 0, '', 33, 171787, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Polemarch Adrestes - On Gossip Option 26131/0 Selected - Quest Credit 171787 (57878)'),
(171795, 0, 0, 0, '', 62, 0, 100, 0, 26132, 0, 0, 0, 0, '', 33, 171795, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Lady Moonberry - On Gossip Option 26132/0 Selected - Quest Credit 171795 (57878)'),
(171589, 0, 0, 0, '', 62, 0, 100, 0, 26041, 0, 0, 0, 0, '', 33, 171589, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'General Draven - On Gossip Option 26041/0 Selected - Quest Credit 171589 (57878)'),
(171821, 0, 0, 0, '', 62, 0, 100, 0, 26133, 0, 0, 0, 0, '', 33, 171821, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Secutor Mevix - On Gossip Option 26133/0 Selected - Quest Credit 171821 (57878)'),
(176100, 0, 0, 0, '', 62, 0, 100, 0, 26775, 0, 0, 0, 0, '', 33, 175046, 0, 0, 0, 0, 0, 0, '', 7, 0, 0, 0, 0, '', 0, 0, 0, 0, 'Iona Skyblade - On Gossip Option 26775/0 Selected - Quest Credit 175046 (62790)');

-- Command-table missions (type 111): tutorial chain 2174 -> 2295 -> 2175 and its follow-up offers, rewards and follower templates as sent by retail
-- Follower health comes from the retail follower payloads; only these four followers are known.
DROP TABLE IF EXISTS `covenant_mission_template`;
CREATE TABLE `covenant_mission_template` (
  `MissionID` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrMission.db2 ID',
  `Scalar` float NOT NULL DEFAULT '1' COMMENT 'GarrisonMission.MissionScalar as sent by retail (DB2 AutoMissionScalar is 1 for all of them)',
  `StartFlags` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrisonMission.Flags sent once the mission is started (retail: 1 on 2174 only)',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`MissionID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `covenant_mission_reward`;
CREATE TABLE `covenant_mission_reward` (
  `MissionID` int unsigned NOT NULL DEFAULT '0',
  `OrderIndex` int unsigned NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `ItemQuantity` int unsigned NOT NULL DEFAULT '0',
  `CurrencyID` int NOT NULL DEFAULT '0',
  `CurrencyQuantity` int unsigned NOT NULL DEFAULT '0',
  `FollowerXP` int unsigned NOT NULL DEFAULT '0',
  `BonusAbilityID` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrMssnBonusAbilityID',
  `ItemFileDataID` int NOT NULL DEFAULT '0' COMMENT 'as sent by retail',
  `ItemContext` int DEFAULT NULL COMMENT 'NULL = the reward ItemInstance carries no ItemBonus block; else its Context byte',
  `ItemBonusListIDs` varchar(100) NOT NULL DEFAULT '' COMMENT 'space separated BonusListIDs of that block',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`MissionID`,`OrderIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `covenant_mission_chain`;
CREATE TABLE `covenant_mission_chain` (
  `PrevMissionID` int unsigned NOT NULL DEFAULT '0' COMMENT '0 = offered when the covenant garrison is first loaded; else offered when this mission is rewarded',
  `NextMissionID` int unsigned NOT NULL DEFAULT '0',
  `OrderIndex` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`PrevMissionID`,`NextMissionID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `covenant_follower_template`;
CREATE TABLE `covenant_follower_template` (
  `GarrFollowerID` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrFollower.db2 ID (type 111)',
  `IsTroop` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1 = auto troop: created per mission start, never stored',
  `Quality` tinyint unsigned NOT NULL DEFAULT '0',
  `ItemLevelWeapon` int unsigned NOT NULL DEFAULT '0',
  `ItemLevelArmor` int unsigned NOT NULL DEFAULT '0',
  `Durability` int unsigned NOT NULL DEFAULT '0',
  `HealthLevel1` int NOT NULL DEFAULT '0' COMMENT 'full health at level 1',
  `HealthPerLevel` int NOT NULL DEFAULT '0' COMMENT 'added per level above 1',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`GarrFollowerID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `covenant_mission_template` (`MissionID`, `Scalar`, `StartFlags`, `VerifiedBuild`) VALUES
(2174, 1, 1, 70961),
(2295, 1, 0, 70961),
(2175, 1, 0, 70961),
(2171, 5.833333, 0, 70961),
(2179, 5.416667, 0, 70961),
(2177, 5, 0, 70961),
(2180, 5.833333, 0, 70961),
(2228, 12, 0, 70961);

INSERT INTO `covenant_mission_reward` (`MissionID`, `OrderIndex`, `ItemID`, `ItemQuantity`, `CurrencyID`, `CurrencyQuantity`, `FollowerXP`, `BonusAbilityID`, `ItemFileDataID`, `ItemContext`, `ItemBonusListIDs`, `VerifiedBuild`) VALUES
(2174, 0, 184373, 1, 0, 0, 0, 0, 236884, NULL, '', 70961),
(2295, 0, 184373, 1, 0, 0, 0, 0, 236884, NULL, '', 70961),
(2175, 0, 184373, 1, 0, 0, 0, 0, 236884, NULL, '', 70961),
(2171, 0, 0, 0, 0, 0, 1200, 0, 0, NULL, '', 70961),
(2179, 0, 181578, 1, 0, 0, 0, 0, 3547158, 25, '13558', 70961),
(2177, 0, 181544, 6, 0, 0, 0, 0, 1500885, NULL, '', 70961),
(2180, 0, 184636, 1, 0, 0, 0, 0, 1542851, 31, '', 70961),
(2228, 0, 181580, 1, 0, 0, 0, 0, 3547162, 25, '13558', 70961),
(2228, 1, 0, 0, 1889, 1, 0, 0, 0, NULL, '', 70961);

INSERT INTO `covenant_mission_chain` (`PrevMissionID`, `NextMissionID`, `OrderIndex`, `VerifiedBuild`) VALUES
(0, 2174, 0, 70961),
(2174, 2295, 0, 70961),
(2295, 2175, 0, 70961),
(2175, 2171, 0, 70961),
(2175, 2179, 1, 70961),
(2175, 2177, 2, 70961),
(2175, 2180, 3, 70961),
(2175, 2228, 4, 70961);

INSERT INTO `covenant_follower_template` (`GarrFollowerID`, `IsTroop`, `Quality`, `ItemLevelWeapon`, `ItemLevelArmor`, `Durability`, `HealthLevel1`, `HealthPerLevel`, `VerifiedBuild`) VALUES
(1259, 0, 5, 800, 800, 1, 330, 30, 70961),
(1272, 0, 4, 800, 800, 1, 220, 20, 70961),
(1319, 1, 0, 800, 800, 0, 165, 15, 70961),
(1320, 1, 0, 800, 800, 0, 385, 35, 70961);
