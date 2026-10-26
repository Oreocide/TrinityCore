-- Warband campsite groups (character select screen): groups of characters placed in a warband scene, per Battle.net account
CREATE TABLE IF NOT EXISTS `battlenet_account_warband_groups` (
  `battlenetAccountId` int unsigned NOT NULL,
  `groupId` bigint unsigned NOT NULL,
  `orderIndex` tinyint unsigned NOT NULL,
  `warbandSceneId` int unsigned NOT NULL,
  `flags` int unsigned NOT NULL DEFAULT '0',
  `contentSetId` int NOT NULL DEFAULT '0',
  `name` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`battlenetAccountId`,`groupId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `battlenet_account_warband_group_members` (
  `battlenetAccountId` int unsigned NOT NULL,
  `groupId` bigint unsigned NOT NULL,
  `warbandScenePlacementId` int unsigned NOT NULL,
  `type` int NOT NULL DEFAULT '0',
  `contentSetId` int NOT NULL DEFAULT '0',
  `guidHigh` bigint unsigned NOT NULL DEFAULT '0',
  `guidLow` bigint unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`battlenetAccountId`,`groupId`,`warbandScenePlacementId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
