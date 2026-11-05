DROP TABLE IF EXISTS `house_level_data`;
CREATE TABLE `house_level_data` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Level` int NOT NULL DEFAULT '0',
  `QuestID` int NOT NULL DEFAULT '0',
  `Field_12_0_7_67808_003` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_map`;
CREATE TABLE `neighborhood_map` (
  `EntryPositionX` float NOT NULL DEFAULT '0',
  `EntryPositionY` float NOT NULL DEFAULT '0',
  `EntryPositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `MapID` int NOT NULL DEFAULT '0',
  `EntryRotation` float NOT NULL DEFAULT '0',
  `UiTextureKitID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

DROP TABLE IF EXISTS `neighborhood_plot`;
CREATE TABLE `neighborhood_plot` (
  `Cost` bigint unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Field_12_0_0_63534_004X` float NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_004Y` float NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_004Z` float NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_005X` float NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_005Y` float NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_005Z` float NOT NULL DEFAULT '0',
  `CornerstonePositionX` float NOT NULL DEFAULT '0',
  `CornerstonePositionY` float NOT NULL DEFAULT '0',
  `CornerstonePositionZ` float NOT NULL DEFAULT '0',
  `CornerstoneRotationX` float NOT NULL DEFAULT '0',
  `CornerstoneRotationY` float NOT NULL DEFAULT '0',
  `CornerstoneRotationZ` float NOT NULL DEFAULT '0',
  `TeleportPositionX` float NOT NULL DEFAULT '0',
  `TeleportPositionY` float NOT NULL DEFAULT '0',
  `TeleportPositionZ` float NOT NULL DEFAULT '0',
  `ID` int unsigned NOT NULL DEFAULT '0',
  `NeighborhoodMapID` int NOT NULL DEFAULT '0',
  `Field_12_0_0_63534_010` int NOT NULL DEFAULT '0',
  `CornerstoneGameObjectID` int NOT NULL DEFAULT '0',
  `PlotIndex` int NOT NULL DEFAULT '0',
  `WorldState` int NOT NULL DEFAULT '0',
  `PlotGameObjectID` int NOT NULL DEFAULT '0',
  `Field_12_0_0_65028_014` float NOT NULL DEFAULT '0',
  `Field_12_0_0_64741_016` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

