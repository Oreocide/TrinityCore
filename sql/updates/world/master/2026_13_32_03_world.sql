-- Titan Translocator (Silithus <-> Chamber of Heart)
-- Sniff: dump_12.1.0.69933_2026-10-08_13-19-46_parsed.txt
-- Silithus GO 325722 cast 318381 (SCRIPT_EFFECT only) which retail scripts into 295439
-- (TELEPORT_UNITS + DEST_DB). Return GO 289522 casts 278490 -> 270193 (same pattern).
-- Missing spell_target_position left both teleports with nowhere to go; 318381 has no
-- client TriggerSpell link, so point the Silithus goober at 295439 directly.

DELETE FROM `spell_target_position` WHERE `ID` IN (295439, 270193, 269183);
INSERT INTO `spell_target_position`
(`ID`, `EffectIndex`, `OrderIndex`, `MapID`, `PositionX`, `PositionY`, `PositionZ`, `Orientation`, `VerifiedBuild`) VALUES
(295439, 0, 0, 2215, -8279.95, 1754.11, 311.96, 3.1456068, 69933), -- Chamber of Heart (sniffed)
(270193, 0, 0,    1, -7089.19, 1297.64, -92.61, 5.0174727, 69933), -- Silithus (sniffed)
(269183, 0, 0, 2215, -8279.95, 1754.11, 311.96, 3.1415927, 69933); -- older Silithus GO 289521 (same pad)

UPDATE `gameobject_template` SET `Data10` = 295439 WHERE `entry` = 325722 AND `Data10` = 318381;
