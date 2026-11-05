-- Neighborhood Laborers (246336 / 244850) on Founder's Point: the tools come from AURAS, not the equipped item (retail sniff 12.1.5.70575
-- 2026-10-23_14-37-06): emote 1113 <-> 1265374 Hold Hammer (13/13 clean samples), 1119 <-> 1234001 Hold Mining Tool, 1118 <-> 1233777 Shoveling.
-- Retail laborers never show a work emote while their spline is moving (EmoteState 0 updates precede movement), so walking spawns lose the
-- static emote until a waypoint toggle exists; stationary spawns get the tool aura.
UPDATE `creature_addon` a JOIN `creature` c ON c.`guid`=a.`guid` SET a.`auras`='1265374' WHERE c.`map`=2735 AND c.`id` IN (246336,244850) AND a.`emote`=1113 AND c.`MovementType`<>2;
UPDATE `creature_addon` a JOIN `creature` c ON c.`guid`=a.`guid` SET a.`auras`='1234001' WHERE c.`map`=2735 AND c.`id` IN (246336,244850) AND a.`emote`=1119 AND c.`MovementType`<>2;
UPDATE `creature_addon` a JOIN `creature` c ON c.`guid`=a.`guid` SET a.`auras`='1233777' WHERE c.`map`=2735 AND c.`id` IN (246336,244850) AND a.`emote`=1118 AND c.`MovementType`<>2;
UPDATE `creature_addon` a JOIN `creature` c ON c.`guid`=a.`guid` SET a.`emote`=0 WHERE c.`map`=2735 AND c.`id` IN (246336,244850) AND a.`emote` IN (1113,1118,1119) AND c.`MovementType`=2;
