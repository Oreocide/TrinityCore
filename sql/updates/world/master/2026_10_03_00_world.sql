-- Maldraxxus: Theater of Pain arena event (map 2222, area 11465). Scripted by Shadowlands/Maldraxxus/theater_of_pain_arena.cpp
-- Idempotent: every statement can be run more than once.

-- Script names
UPDATE `creature_template` SET `ScriptName` = 'npc_top_arena_center_stalker' WHERE `entry` = 172656;
UPDATE `creature_template` SET `ScriptName` = 'boss_top_unbreakable_urtz' WHERE `entry` = 162853;
UPDATE `creature_template` SET `ScriptName` = 'boss_top_xantuth_the_blighted' WHERE `entry` = 162872;
UPDATE `creature_template` SET `ScriptName` = 'npc_top_xantuth_deathshade' WHERE `entry` IN (172591, 172609);
UPDATE `creature_template` SET `ScriptName` = 'npc_top_arena_challenger' WHERE `entry` IN (161757, 163030, 163032);

-- Summoned by the controller instead of being static sniff spawns: Grandmaster Vole and the Deathshades of Xantuth
DELETE FROM `creature` WHERE `guid` IN (61004060, 61006517, 61006587, 61004012, 61001414, 61003916, 61004011);
-- Same-position copies of Urtz, Xantuth, Azmogal, Dyrax and of one Arena Challenger (the copy kept for Xantuth is the caged one at 2944.69 -2626.97)
DELETE FROM `creature` WHERE `guid` IN (61006430, 61006594, 61003974, 61006421, 61006393, 61006420, 61006533);
DELETE FROM `creature_addon` WHERE `guid` IN (61004060, 61006517, 61006587, 61004012, 61001414, 61003916, 61004011, 61006430, 61006594, 61003974, 61006421, 61006393, 61006420, 61006533);

-- The caged ladder bosses stay put until the controller releases them (path 760004854 is played by the script, 760004855 is for the unsniffed Dyrax variant)
UPDATE `creature` SET `MovementType` = 0 WHERE `guid` IN (61003986, 61003985);

-- Arena Challengers walk their sniffed path once when an event starts (the controller sends them), not at spawn
UPDATE `creature` c JOIN `creature_addon` ca ON ca.`guid` = c.`guid`
SET c.`MovementType` = 0
WHERE c.`map` = 2222 AND c.`id` IN (161757, 163030, 163032) AND ca.`PathId` <> 0;

-- Scouring Slime / Bubbling Sclerosis / Mindless Animate "paths" were idle jitter within ~7 yards of the spawn point: wander instead
UPDATE `creature` c JOIN `creature_addon` ca ON ca.`guid` = c.`guid`
SET c.`MovementType` = 1, c.`wander_distance` = 5
WHERE ca.`PathId` BETWEEN 760004809 AND 760004823 OR ca.`PathId` = 760004691;
UPDATE `creature_addon` SET `PathId` = 0 WHERE `PathId` BETWEEN 760004809 AND 760004823 OR `PathId` = 760004691;
DELETE FROM `waypoint_path_node` WHERE `PathId` BETWEEN 760004809 AND 760004823 OR `PathId` = 760004691;
DELETE FROM `waypoint_path` WHERE `PathId` BETWEEN 760004809 AND 760004823 OR `PathId` = 760004691;

-- Roaming rows in the arena that point at no path at all
UPDATE `creature` c LEFT JOIN `creature_addon` ca ON ca.`guid` = c.`guid`
SET c.`MovementType` = 0
WHERE c.`map` = 2222 AND c.`areaId` IN (0, 11465) AND c.`position_x` BETWEEN 2690 AND 3000 AND c.`position_y` BETWEEN -2720 AND -2300
  AND c.`MovementType` = 2 AND (ca.`PathId` IS NULL OR ca.`PathId` = 0);

-- Challenger paths: drop nodes that repeat the previous one (the sniff holds position that way; MoveSplineInitArgs rejects duplicate nodes),
-- then renumber the nodes from 1
DELETE n FROM `waypoint_path_node` n JOIN (
    SELECT `PathId`, `NodeId`,
        SQRT(POW(`PositionX` - LAG(`PositionX`) OVER w, 2) + POW(`PositionY` - LAG(`PositionY`) OVER w, 2) + POW(`PositionZ` - LAG(`PositionZ`) OVER w, 2)) AS dist
    FROM `waypoint_path_node`
    WHERE `PathId` IN (760004824, 760004825, 760004826, 760004856, 760004857, 760004858, 760004859, 760004860, 760004861, 760004862, 760004863, 760004864, 760004865)
    WINDOW w AS (PARTITION BY `PathId` ORDER BY `NodeId`)
) d ON d.`PathId` = n.`PathId` AND d.`NodeId` = n.`NodeId`
WHERE d.dist < 0.1;

UPDATE `waypoint_path_node` n JOIN (
    SELECT `PathId`, `NodeId`, ROW_NUMBER() OVER (PARTITION BY `PathId` ORDER BY `NodeId`) AS rn
    FROM `waypoint_path_node`
    WHERE `PathId` IN (760004824, 760004825, 760004826, 760004856, 760004857, 760004858, 760004859, 760004860, 760004861, 760004862, 760004863, 760004864, 760004865)
) r ON r.`PathId` = n.`PathId` AND r.`NodeId` = n.`NodeId`
SET n.`NodeId` = r.rn + 100000;
UPDATE `waypoint_path_node` SET `NodeId` = `NodeId` - 100000 WHERE `NodeId` > 100000 AND `PathId` BETWEEN 760004824 AND 760004865;
