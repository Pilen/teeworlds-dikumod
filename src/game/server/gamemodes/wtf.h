//DIKUMOD begin
/* (c) Soeren Pilgaard. See licence.txt in the root of the distribution for more information. */
/* If you ar missing that file, acquire a complete relase at teeworlds.com.                   */
#ifndef GAME_SERVER_GAMEMODES_WTF_H
#define GAME_SERVER_GAMEMODES_WTF_H
#include <game/server/gamecontroller.h>
#include <game/server/entity.h>

class CGameControllerWTF : public IGameController
{
public:
  class CFlag *m_pFlag;

  CGameControllerWTF(class CGameContext *pGameServer);
  virtual void DoWincheck();
  virtual bool CanBeMovedOnBalance(int ClientID);
  virtual void Snap(int SnappingClient);
  virtual void Tick();

  virtual bool OnEntity(int Index, vec2 Pos);
  virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
};

#endif
//DIKUMOD end
