-- 12.1.5 signedness for columns not altered by 2026_10_13_00_hotfixes.sql.
-- The ADD COLUMN statements from upstream 2026_10_14_00_hotfixes.sql are already applied there.
ALTER TABLE `spell_item_enchantment`
  MODIFY COLUMN `IconFileDataID` int NOT NULL DEFAULT 0 AFTER `MaxLevel`,
  MODIFY COLUMN `MinItemLevel` int NOT NULL DEFAULT 0 AFTER `IconFileDataID`,
  MODIFY COLUMN `MaxItemLevel` int NOT NULL DEFAULT 0 AFTER `MinItemLevel`,
  MODIFY COLUMN `TransmogUseConditionID` int NOT NULL DEFAULT 0 AFTER `MaxItemLevel`,
  MODIFY COLUMN `TransmogCost` int NOT NULL DEFAULT 0 AFTER `TransmogUseConditionID`;
