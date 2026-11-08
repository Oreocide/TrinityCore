/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// This is where scripts' loading functions should be declared:
void AddSC_covenant_spell_scripts();
void AddSC_shadowlands_raid_portals();
void AddSC_torghast_spell_scripts();
void AddSC_instance_torghast();
void AddSC_theater_of_pain_arena();

void AddSC_instance_castle_nathria();
void AddSC_boss_hungering_destroyer();
void AddSC_boss_huntsman_altimor();
void AddSC_boss_sludgefist();
void AddSC_boss_artificer_xymox();
void AddSC_boss_stone_legion_generals();
void AddSC_boss_lady_inerva_darkvein();

void AddSC_boss_eye_of_the_jailer_sod();
void AddSC_boss_the_tarragrue_sod();
void AddSC_boss_kelthuzad_sod();
void AddSC_boss_sylvanas_windrunner();
void AddSC_sylvanas_windrunner_adds();
void AddSC_instance_sanctum_of_domination();

void AddSC_boss_anduin_wrynn();
void AddSC_instance_sepulcher_of_the_first_ones();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddShadowlandsScripts()
{
    AddSC_covenant_spell_scripts();
    AddSC_shadowlands_raid_portals();
    AddSC_torghast_spell_scripts();
    AddSC_instance_torghast();

    // Maldraxxus
    AddSC_theater_of_pain_arena();

    // Castle Nathria
    AddSC_instance_castle_nathria();
    AddSC_boss_hungering_destroyer();
    AddSC_boss_huntsman_altimor();
    AddSC_boss_sludgefist();
    AddSC_boss_artificer_xymox();
    AddSC_boss_stone_legion_generals();
    AddSC_boss_lady_inerva_darkvein();

    // Sanctum of Domination
    AddSC_boss_eye_of_the_jailer_sod();
    AddSC_boss_the_tarragrue_sod();
    AddSC_boss_kelthuzad_sod();
    AddSC_boss_sylvanas_windrunner();
    AddSC_sylvanas_windrunner_adds();
    AddSC_instance_sanctum_of_domination();

    // Sepulcher of The First Ones
    AddSC_boss_anduin_wrynn();
    AddSC_instance_sepulcher_of_the_first_ones();
}
