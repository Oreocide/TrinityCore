-- World state scheduler tables (TrinityCore WorldStateScheduler). Columns follow retail's
-- ScheduledInterval / ScheduledWorldStateGroup / ScheduledWorldState DB2 (9.0.1); IndexOffset, Enabled,
-- Confidence, Source and Comment are ours.
CREATE TABLE IF NOT EXISTS `scheduled_interval` (
  `ID` int unsigned NOT NULL,
  `Flags` int NOT NULL DEFAULT '0',
  `RepeatType` int NOT NULL DEFAULT '0',
  `DurationSecs` bigint NOT NULL DEFAULT '0',
  `OffsetSecs` bigint NOT NULL DEFAULT '0' COMMENT 'added to Reset.Epoch',
  `DateAlignmentType` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `scheduled_world_state_group` (
  `ID` int unsigned NOT NULL,
  `Flags` int NOT NULL DEFAULT '0',
  `ScheduledIntervalID` int unsigned NOT NULL DEFAULT '0',
  `SelectionType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 all, 1 sequential, 2 ladder, 3 weighted random, 5 progressive (unsupported)',
  `SelectionCount` int unsigned NOT NULL DEFAULT '0',
  `Priority` int NOT NULL DEFAULT '0',
  `IndexOffset` int NOT NULL DEFAULT '0' COMMENT 'sequential: row = (period index + IndexOffset) mod rows',
  `Enabled` tinyint unsigned NOT NULL DEFAULT '0',
  `Confidence` char(1) NOT NULL DEFAULT 'I' COMMENT 'V verified against sniffs, I inferred, R recall',
  `Source` varchar(64) NOT NULL DEFAULT '',
  `Comment` varchar(255) NOT NULL DEFAULT '',
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `scheduled_world_state` (
  `ID` int unsigned NOT NULL,
  `GroupID` int unsigned NOT NULL DEFAULT '0',
  `WorldStateID` int NOT NULL DEFAULT '0',
  `Value` int NOT NULL DEFAULT '0',
  `DurationSecs` bigint NOT NULL DEFAULT '0' COMMENT '> 0: on only for this long from the period start',
  `Weight` int unsigned NOT NULL DEFAULT '0',
  `UniqueCategory` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `OrderIndex` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`),
  KEY `idx_group` (`GroupID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
