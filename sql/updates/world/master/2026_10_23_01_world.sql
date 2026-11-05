-- Housing tutorial: "Teleport to Founder's Point" (1258476), TELEPORT_UNITS effect 0 to DEST_DB.
-- Destination = SMSG_NEW_WORLD after SMSG_TRANSFER_PENDING(TransferSpellID 1258476), two samples agree (build 70575 sniff).
-- Razorwind Shores (1258484, map 2736) has no row: no Horde sniff.
DELETE FROM `spell_target_position` WHERE `ID`=1258476 AND `EffectIndex`=0;
INSERT INTO `spell_target_position` (`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
(1258476, 0, 0, 2735, 3774.97, -151.89, 191.22, 2.725157, 70575);
