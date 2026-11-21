-- Hotfix tables of the DB2 stores RenownRewards, GarrTalent, GarrTalentRank, GarrTalentResearch, GarrTalentSocketProperties, Soulbind,
-- SoulbindConduit, SoulbindConduitItem, GarrFollowerLevelXP, GarrAutoCombatant, GarrAutoSpell, GarrAutoSpellEffect, GarrEncounter and
-- GarrMissionXEncounter. They stay empty: the data comes from the .db2 files.

CREATE TABLE IF NOT EXISTS `renown_rewards` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ToastDescription` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CovenantID` int unsigned NOT NULL DEFAULT '0',
  `Level` int NOT NULL DEFAULT '0',
  `Icon` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `UiOrder` int NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `SpellID` int NOT NULL DEFAULT '0',
  `MountID` int NOT NULL DEFAULT '0',
  `TransmogID` int NOT NULL DEFAULT '0',
  `TransmogSetID` int NOT NULL DEFAULT '0',
  `CharTitlesID` int NOT NULL DEFAULT '0',
  `GarrFollowerID` int NOT NULL DEFAULT '0',
  `TransmogIllusionID` int NOT NULL DEFAULT '0',
  `RewardType` int NOT NULL DEFAULT '0',
  `QuestID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `renown_rewards_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `ToastDescription_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

CREATE TABLE IF NOT EXISTS `garr_talent` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `GarrTalentTreeID` int unsigned NOT NULL DEFAULT '0',
  `Tier` tinyint NOT NULL DEFAULT '0',
  `UiOrder` tinyint NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int unsigned NOT NULL DEFAULT '0',
  `GarrAbilityID` int unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `TalentType` int NOT NULL DEFAULT '0',
  `PrerequisiteTalentID` int NOT NULL DEFAULT '0',
  `ResearchCostSource` int NOT NULL DEFAULT '0',
  `ActiveDurationSecs` int NOT NULL DEFAULT '0',
  `GarrTalentSocketPropertiesID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_talent_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

CREATE TABLE IF NOT EXISTS `garr_talent_rank` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Rank` int NOT NULL DEFAULT '0',
  `PerkSpellID` int NOT NULL DEFAULT '0',
  `PerkPlayerConditionID` int NOT NULL DEFAULT '0',
  `Points` float NOT NULL DEFAULT '0',
  `ResearchCost` int NOT NULL DEFAULT '0',
  `ResearchCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `ResearchGoldCost` int NOT NULL DEFAULT '0',
  `ResearchDurationSecs` int NOT NULL DEFAULT '0',
  `RespecCost` int NOT NULL DEFAULT '0',
  `RespecCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `RespecGoldCost` int NOT NULL DEFAULT '0',
  `RespecDurationSecs` int NOT NULL DEFAULT '0',
  `AlternateResearchCost` int NOT NULL DEFAULT '0',
  `AlternateResearchCostCurrencyTypesID` int NOT NULL DEFAULT '0',
  `AlternateResearchGoldCost` int NOT NULL DEFAULT '0',
  `AlternateResearchDurationSecs` int NOT NULL DEFAULT '0',
  `GarrTalentID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_talent_research` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GoldCost` int NOT NULL DEFAULT '0',
  `CurrencyTypesID` int NOT NULL DEFAULT '0',
  `CurrencyTypesCost` int NOT NULL DEFAULT '0',
  `DurationSecs` int NOT NULL DEFAULT '0',
  `RespecGoldCost` int NOT NULL DEFAULT '0',
  `RespecCurrencyTypesID` int NOT NULL DEFAULT '0',
  `RespecCurrencyTypesCost` int NOT NULL DEFAULT '0',
  `RespecDurationSecs` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_talent_socket_properties` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrTalentSocketType` int NOT NULL DEFAULT '0',
  `GarrTalentSocketSubtype` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `soulbind` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CovenantID` int NOT NULL DEFAULT '0',
  `GarrTalentTreeID` int NOT NULL DEFAULT '0',
  `CreatureID` int NOT NULL DEFAULT '0',
  `GarrFollowerID` int NOT NULL DEFAULT '0',
  `PlayerConditionID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `soulbind_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

CREATE TABLE IF NOT EXISTS `soulbind_conduit` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ConduitType` tinyint unsigned NOT NULL DEFAULT '0',
  `CovenantID` int NOT NULL DEFAULT '0',
  `SpecSetID` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `soulbind_conduit_item` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `ItemID` int NOT NULL DEFAULT '0',
  `ConduitID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;


CREATE TABLE IF NOT EXISTS `garr_follower_level_xp` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrFollowerTypeID` tinyint unsigned NOT NULL DEFAULT '0',
  `FollowerLevel` tinyint unsigned NOT NULL DEFAULT '0',
  `XpToNextLevel` smallint unsigned NOT NULL DEFAULT '0',
  `ShipmentXP` smallint unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_auto_combatant` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `HealthBase` int NOT NULL DEFAULT '0',
  `HealthGainPerLevel` int NOT NULL DEFAULT '0',
  `AttackBase` int NOT NULL DEFAULT '0',
  `AttackGainPerLevel` int NOT NULL DEFAULT '0',
  `AttackSpellID` int NOT NULL DEFAULT '0',
  `AbilitySpellID` int NOT NULL DEFAULT '0',
  `AbilitySpellID2` int NOT NULL DEFAULT '0',
  `PassiveSpellID` int NOT NULL DEFAULT '0',
  `Role` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_auto_spell` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Cooldown` int NOT NULL DEFAULT '0',
  `Duration` int NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `SchoolMask` int NOT NULL DEFAULT '0',
  `IconFileDataID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_auto_spell_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `Description_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

CREATE TABLE IF NOT EXISTS `garr_auto_spell_effect` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `SpellID` int unsigned NOT NULL DEFAULT '0',
  `EffectIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `Effect` tinyint unsigned NOT NULL DEFAULT '0',
  `Points` float NOT NULL DEFAULT '0',
  `TargetType` tinyint unsigned NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `Period` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_encounter` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `Name` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `CreatureID` int NOT NULL DEFAULT '0',
  `PortraitFileDataID` int NOT NULL DEFAULT '0',
  `UiTextureKitID` int unsigned NOT NULL DEFAULT '0',
  `UiAnimScale` float NOT NULL DEFAULT '0',
  `UiAnimHeight` float NOT NULL DEFAULT '0',
  `Flags` int NOT NULL DEFAULT '0',
  `AutoCombatantID` int NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `garr_encounter_locale` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `locale` varchar(4) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,
  `Name_lang` text CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`locale`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
/*!50500 PARTITION BY LIST  COLUMNS(locale)
(PARTITION deDE VALUES IN ('deDE') ENGINE = InnoDB,
 PARTITION esES VALUES IN ('esES') ENGINE = InnoDB,
 PARTITION esMX VALUES IN ('esMX') ENGINE = InnoDB,
 PARTITION frFR VALUES IN ('frFR') ENGINE = InnoDB,
 PARTITION itIT VALUES IN ('itIT') ENGINE = InnoDB,
 PARTITION koKR VALUES IN ('koKR') ENGINE = InnoDB,
 PARTITION ptBR VALUES IN ('ptBR') ENGINE = InnoDB,
 PARTITION ruRU VALUES IN ('ruRU') ENGINE = InnoDB,
 PARTITION zhCN VALUES IN ('zhCN') ENGINE = InnoDB,
 PARTITION zhTW VALUES IN ('zhTW') ENGINE = InnoDB) */;

CREATE TABLE IF NOT EXISTS `garr_mission_x_encounter` (
  `ID` int unsigned NOT NULL DEFAULT '0',
  `GarrEncounterID` int unsigned NOT NULL DEFAULT '0',
  `GarrEncounterSetID` int unsigned NOT NULL DEFAULT '0',
  `OrderIndex` tinyint unsigned NOT NULL DEFAULT '0',
  `BoardIndex` tinyint NOT NULL DEFAULT '0',
  `GarrMissionID` int unsigned NOT NULL DEFAULT '0',
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`ID`,`VerifiedBuild`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
