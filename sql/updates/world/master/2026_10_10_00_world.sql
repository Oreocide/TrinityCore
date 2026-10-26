-- Data driven emitters for scenario game events (patterns/SCENARIO_FOUNDATION.md S2)
-- TriggerType: 1 creature death (arg creature entry), 2 gameobject use (entry), 3 areatrigger enter (AreaTrigger.db2 id), 4 area enter (AreaTable id), 5 spell cast (spell id)
CREATE TABLE IF NOT EXISTS `scenario_event_trigger` (
  `TriggerType` tinyint unsigned NOT NULL,
  `TriggerArg` int unsigned NOT NULL,
  `MapId` int unsigned NOT NULL DEFAULT '0' COMMENT '0 = any map',
  `ScenarioId` int unsigned NOT NULL DEFAULT '0' COMMENT '0 = any / no scenario required',
  `EventId` int unsigned NOT NULL COMMENT 'game event id used by AnyoneTriggerGameEventScenario / PlayerTriggerGameEvent criteria',
  `Comment` varchar(255) DEFAULT NULL,
  `VerifiedBuild` int NOT NULL DEFAULT '0',
  PRIMARY KEY (`TriggerType`,`TriggerArg`,`MapId`,`ScenarioId`,`EventId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
