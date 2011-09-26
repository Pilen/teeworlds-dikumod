/* (c) Soeren Pilgaard. See licence.txt in the root of the distribution for more informatnion. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                  */
#include <engine/shared/config.h>

#include <game/mapitems.h>

#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/player.h>
#include <game/server/gamecontext.h>
#include "wtf.h"

CGameControllerWTF::CGameControllerWTF(class CGameContext *pGameServer)
  : IGameController(pGameServer)
{
  m_pFlag = 0;
  m_pGameType = "DIKU_WTF";
  m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;
}

bool CGameControllerWTF::OnEntity(int Index, vec2 Pos)
{
  if(IGameController::OnEntity(Index, Pos))
    return true;

  if (Index == ENTITY_FLAGSTAND_BLUE && !m_pFlag) {
    m_pFlag = new CFlag(&GameServer()->m_World, TEAM_BLUE);
    m_pFlag->m_StandPos = Pos;
    m_pFlag->m_Pos = Pos;
    GameServer()->m_World.InsertEntity(m_pFlag);
    return true;
  }
  else
    return false;
}

int CGameControllerWTF::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
  IGameController::OnCharacterDeath(pVictim, pKiller, WeaponID);
  int HadFlag = 0;

  // killer has flag
  if (m_pFlag && pKiller && pKiller->GetCharacter() &&
      m_pFlag->m_pCarryingCharacter == pKiller->GetCharacter())
    HadFlag |= 2;
  // victims drop flag
  if (m_pFlag && m_pFlag->m_pCarryingCharacter == pVictim) {
    GameServer()->CreateSoundGlobal(SOUND_CTF_DROP);
    m_pFlag->m_DropTick = Server()->Tick();
    m_pFlag->m_pCarryingCharacter = NULL;
    m_pFlag->m_Vel = vec2(0,0);

    // add score to killer
    if (pKiller && pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam())
      pKiller->m_Score++;
    HadFlag |= 1;
  }
  return HadFlag;
}

void CGameControllerWTF::DoWincheck()
{
  if (m_GameOverTick == -1 && !m_Warmup) {
    //check score win condition
    if ((g_Config.m_SvScorelimit > 0 &&
         (m_aTeamscore[TEAM_RED] >= g_Config.m_SvScorelimit ||
          m_aTeamscore[TEAM_BLUE] >= g_Config.m_SvScorelimit)) ||
        (g_Config.m_SvTimelimit > 0 &&
         (Server()->Tick() - m_RoundStartTick) >= g_Config.m_SvTimelimit*Server()->TickSpeed() * 60)) {

      if (m_SuddenDeath) {
        if (m_aTeamscore[TEAM_RED]/100 != m_aTeamscore[TEAM_BLUE]/100)
          EndRound();
      }
      else {
        if (m_aTeamscore[TEAM_RED] != m_aTeamscore[TEAM_BLUE])
          EndRound();
        else
          m_SuddenDeath = 1;
      }
    }
  }
}

bool CGameControllerWTF::CanBeMovedOnBalance(int ClientID)
{
  CCharacter* Character = GameServer()->m_apPlayers[ClientID]->GetCharacter();
  if (Character && m_pFlag->m_pCarryingCharacter == Character)
    return false;
  return true;
}

void CGameControllerWTF::Snap(int SnappingClient)
{
  IGameController::Snap(SnappingClient);

  CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0,
                                                                             sizeof(CNetObj_GameData));

  if (!pGameDataObj)
    return;

  pGameDataObj->m_TeamscoreRed  = m_aTeamscore[TEAM_RED];
  pGameDataObj->m_TeamscoreBlue = m_aTeamscore[TEAM_BLUE];

  if (m_pFlag) {
    if (m_pFlag->m_AtStand) {
      pGameDataObj->m_FlagCarrierRed  = FLAG_ATSTAND;
      pGameDataObj->m_FlagCarrierBlue = FLAG_ATSTAND;
    }
    else if (m_pFlag->m_pCarryingCharacter && m_pFlag->m_pCarryingCharacter->GetPlayer()) {
      if (m_pFlag->m_pCarryingCharacter->GetPlayer()->GetTeam() == TEAM_RED) {
        pGameDataObj->m_FlagCarrierRed  = m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID();
        pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
      }
      else {
        pGameDataObj->m_FlagCarrierRed  = FLAG_MISSING;
        pGameDataObj->m_FlagCarrierBlue = m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID();
      }
    }
  }
  else {
    pGameDataObj->m_FlagCarrierRed  = FLAG_MISSING;
    pGameDataObj->m_FlagCarrierBlue = FLAG_MISSING;
  }
}

void CGameControllerWTF::Tick()
{
  IGameController::Tick();

  // Only tick if needed
  if (GameServer()->m_World.m_ResetRequested || GameServer()->m_World.m_Paused)
    return;


  if (m_pFlag) {

    // reset flag if it hits death-tile or left game layer
    if (GameServer()->Collision()->GetCollisionAt(m_pFlag->m_Pos.x, m_pFlag->m_Pos.y)&
        CCollision::COLFLAG_DEATH || m_pFlag->GameLayerClipped(m_pFlag->m_Pos)) {
      GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", "flag_return");
      GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
      m_pFlag->Reset();
        return;
    }

    // if carried, move + add points
    if (m_pFlag->m_pCarryingCharacter) {
      m_pFlag->m_Pos = m_pFlag->m_pCarryingCharacter->m_Pos;
      m_aTeamscore[m_pFlag->m_pCarryingCharacter->GetPlayer()->GetTeam()] += 1;
    }
    // flag on ground
    else {
      //list of players touching flag
      CCharacter *apCloseCCharacters[MAX_CLIENTS];
      int Num = GameServer()->m_World.FindEntities(m_pFlag->m_Pos, CFlag::ms_PhysSize,
                                                   (CEntity**)apCloseCCharacters, MAX_CLIENTS,
                                                   CGameWorld::ENTTYPE_CHARACTER);
      for (int i = 0; i < Num; i++) {
        //can the player pick up the flag?
        if (!apCloseCCharacters[i]->IsAlive() ||
            apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS ||
            GameServer()->Collision()->IntersectLine(m_pFlag->m_Pos, apCloseCCharacters[i]->m_Pos,
                                                     NULL, NULL))
          return;
        //yes!

        //pickup
        m_pFlag->m_AtStand = 0;
        m_pFlag->m_pCarryingCharacter = apCloseCCharacters[i];
        m_pFlag->m_pCarryingCharacter->GetPlayer()->m_Score += 1;

        //console
        char aBuf[256];
        str_format(aBuf, sizeof(aBuf), "flag_grab player='%d%s'",
                   m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID(),
                   Server()->ClientName(m_pFlag->m_pCarryingCharacter->GetPlayer()->GetCID()));
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

        //play sound
        for (int c = 0; c < MAX_CLIENTS; c++) {
          CPlayer *pPlayer = GameServer()->m_apPlayers[c];
          if (!pPlayer)
            continue;

          if (pPlayer->GetTeam() == TEAM_SPECTATORS &&
              pPlayer->m_SpectatorID != SPEC_FREEVIEW &&
              GameServer()->m_apPlayers[pPlayer->m_SpectatorID] &&
              GameServer()->m_apPlayers[pPlayer->m_SpectatorID]->GetTeam() ==
              m_pFlag->m_pCarryingCharacter->GetPlayer()->GetTeam())
            GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
          else if (pPlayer->GetTeam() == m_pFlag->m_pCarryingCharacter->GetPlayer()->GetTeam())
            GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_EN, c);
          else
            GameServer()->CreateSoundGlobal(SOUND_CTF_GRAB_PL, c);
        }
        break;
      }
    }
    // Uncarried flags drop
    if (!m_pFlag->m_pCarryingCharacter && !m_pFlag->m_AtStand) {
      if (Server()->Tick() > m_pFlag->m_DropTick + Server()->TickSpeed()*30) {
        GameServer()->CreateSoundGlobal(SOUND_CTF_RETURN);
        m_pFlag->Reset();
      }
      else {
        m_pFlag->m_Vel.y += GameServer()->m_World.m_Core.m_Tuning.m_Gravity;
        GameServer()->Collision()->MoveBox(&m_pFlag->m_Pos, &m_pFlag->m_Vel,
                                           vec2(m_pFlag->ms_PhysSize, m_pFlag->ms_PhysSize), 0.5f);
      }
    }
  }//flag exists
}
