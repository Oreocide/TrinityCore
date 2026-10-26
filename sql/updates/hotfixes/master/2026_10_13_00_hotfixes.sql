ALTER TABLE `player_data_element_account`
  ADD `Unknown1215` int NOT NULL DEFAULT '0' AFTER `Unknown1125`;

ALTER TABLE `player_data_element_character`
  ADD `Unknown1215` int NOT NULL DEFAULT '0' AFTER `Unknown1125`;

ALTER TABLE `spell_item_enchantment`
  ADD `ItemSquishEraID` int NOT NULL DEFAULT '0' AFTER `TransmogCost`,
  MODIFY `Charges` int NOT NULL DEFAULT '0',
  MODIFY `Effect1` int NOT NULL DEFAULT '0',
  MODIFY `Effect2` int NOT NULL DEFAULT '0',
  MODIFY `Effect3` int NOT NULL DEFAULT '0',
  MODIFY `EffectPointsMin1` int NOT NULL DEFAULT '0',
  MODIFY `EffectPointsMin2` int NOT NULL DEFAULT '0',
  MODIFY `EffectPointsMin3` int NOT NULL DEFAULT '0',
  MODIFY `ScalingClass` int NOT NULL DEFAULT '0',
  MODIFY `ScalingClassRestricted` int NOT NULL DEFAULT '0',
  MODIFY `ConditionID` int NOT NULL DEFAULT '0',
  MODIFY `RequiredSkillID` int NOT NULL DEFAULT '0',
  MODIFY `RequiredSkillRank` int NOT NULL DEFAULT '0',
  MODIFY `MinLevel` int NOT NULL DEFAULT '0',
  MODIFY `MaxLevel` int NOT NULL DEFAULT '0';

ALTER TABLE `spell_procs_per_minute_mod`
  ADD `PlayerConditionID` int NOT NULL DEFAULT '0' AFTER `Coeff`;

ALTER TABLE `spell_visual_missile`
  ADD `MinimumSpellVisualDensityFilterType` int NOT NULL DEFAULT '0' AFTER `Unused1100`,
  ADD `MinimumSpellVisualDensityFilterParam` int NOT NULL DEFAULT '0' AFTER `MinimumSpellVisualDensityFilterType`,
  ADD `ReducedSpellVisualDensityFilterType` int NOT NULL DEFAULT '0' AFTER `MinimumSpellVisualDensityFilterParam`,
  ADD `ReducedSpellVisualDensityFilterParam` int NOT NULL DEFAULT '0' AFTER `ReducedSpellVisualDensityFilterType`;
