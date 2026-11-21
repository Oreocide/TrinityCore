-- Covenant join (character_covenant), soulbinds and conduits, command-table followers and missions

CREATE TABLE IF NOT EXISTS `character_covenant` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `covenantId` int NOT NULL DEFAULT '0' COMMENT 'Covenant.db2 ID',
  `joinTime` bigint NOT NULL DEFAULT '0' COMMENT 'Unix time of the join',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_covenant_soulbind_spec` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `chrSpecializationId` int NOT NULL DEFAULT '0' COMMENT 'ChrSpecialization.db2 ID',
  `soulbindId` int NOT NULL DEFAULT '0' COMMENT 'Soulbind.db2 ID',
  PRIMARY KEY (`guid`,`chrSpecializationId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_garrison_talent` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `garrTypeId` int NOT NULL DEFAULT '0' COMMENT 'GarrType.db2 ID',
  `garrTalentId` int NOT NULL DEFAULT '0' COMMENT 'GarrTalent.db2 ID',
  `rank` int NOT NULL DEFAULT '0',
  `researchStartTime` bigint NOT NULL DEFAULT '0',
  `flags` int NOT NULL DEFAULT '0' COMMENT 'GarrisonTalentFlag',
  `socketConduitId` int NOT NULL DEFAULT '0' COMMENT 'SoulbindConduit.db2 ID, 0 = empty socket',
  `socketConduitRank` int NOT NULL DEFAULT '0' COMMENT 'SoulbindConduitRank.db2 RankIndex',
  PRIMARY KEY (`guid`,`garrTypeId`,`garrTalentId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_garrison_collection` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `garrTypeId` int NOT NULL DEFAULT '0' COMMENT 'GarrType.db2 ID',
  `collectionType` int NOT NULL DEFAULT '0' COMMENT '1 = soulbind conduits',
  `entryId` int NOT NULL DEFAULT '0',
  `rank` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`garrTypeId`,`collectionType`,`entryId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE IF NOT EXISTS `character_covenant_follower` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `dbId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'GarrisonFollower.DbID as sent to the client',
  `garrFollowerId` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrFollower.db2 ID',
  `quality` tinyint unsigned NOT NULL DEFAULT '0',
  `followerLevel` int unsigned NOT NULL DEFAULT '1',
  `itemLevelWeapon` int unsigned NOT NULL DEFAULT '0',
  `itemLevelArmor` int unsigned NOT NULL DEFAULT '0',
  `xp` int unsigned NOT NULL DEFAULT '0',
  `durability` int unsigned NOT NULL DEFAULT '0',
  `health` int NOT NULL DEFAULT '0',
  `healingTimestamp` bigint NOT NULL DEFAULT '0',
  `currentMissionId` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrMission.db2 ID, 0 = free',
  `status` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrisonFollowerStatus',
  PRIMARY KEY (`guid`,`dbId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_covenant_mission` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `dbId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'GarrisonMission.DbID as sent to the client',
  `missionId` int unsigned NOT NULL DEFAULT '0' COMMENT 'GarrMission.db2 ID',
  `offerTime` bigint NOT NULL DEFAULT '0',
  `offerDuration` bigint NOT NULL DEFAULT '0' COMMENT 'seconds, 0 = never expires',
  `startTime` bigint NOT NULL DEFAULT '0',
  `travelDuration` bigint NOT NULL DEFAULT '0',
  `missionDuration` bigint NOT NULL DEFAULT '0',
  `state` int NOT NULL DEFAULT '0' COMMENT '0 offered, 1 in progress, 2 complete (awaiting the bonus roll), 6 rewarded',
  `successChance` int NOT NULL DEFAULT '0',
  `flags` int unsigned NOT NULL DEFAULT '0',
  `scalar` float NOT NULL DEFAULT '1',
  PRIMARY KEY (`guid`,`dbId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_covenant_mission_follower` (
  `guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character Guid',
  `missionDbId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'character_covenant_mission.dbId',
  `followerDbId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'hero: character_covenant_follower.dbId; troop: id made for this start',
  `garrFollowerId` int unsigned NOT NULL DEFAULT '0',
  `isTroop` tinyint unsigned NOT NULL DEFAULT '0',
  `followerLevel` int unsigned NOT NULL DEFAULT '1',
  `boardIndex` tinyint NOT NULL DEFAULT '0',
  `health` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`guid`,`missionDbId`,`followerDbId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
