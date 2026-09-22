/*
 * mod-bigbags - every character starts with its bag slots filled
 *
 * New characters get their bags from playercreateinfo_item (see the custom SQL
 * shipped with this server), this module covers everything else: characters
 * that already existed, characters created before the SQL was applied, and
 * bots that were generated earlier.
 *
 * Bags are granted once per character. The grant is remembered through the
 * core's player settings ("EnablePlayerSettings" must be on in
 * worldserver.conf), so a character cannot farm bags by deleting them and
 * logging back in.
 */

#include "Chat.h"
#include "Config.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerSettings.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldSession.h"

namespace
{
    constexpr char const* SETTINGS_SOURCE = "mod-bigbags";
    constexpr uint32 SETTING_GRANTED      = 0;

    struct BigBagsConfig
    {
        bool     Enable      = true;
        uint32   BagEntry    = 0;
        bool     Announce    = true;
        std::string AnnounceText;
    };

    BigBagsConfig cfg;

    void LoadConfig()
    {
        cfg.Enable       = sConfigMgr->GetOption<bool>("BigBags.Enable", true);
        cfg.BagEntry     = sConfigMgr->GetOption<uint32>("BigBags.BagEntry", 23162);
        cfg.Announce     = sConfigMgr->GetOption<bool>("BigBags.Announce", true);
        cfg.AnnounceText = sConfigMgr->GetOption<std::string>("BigBags.AnnounceText",
            "Your bag slots have been filled with large bags.");

        if (cfg.Enable && !sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
            LOG_WARN("module", "mod-bigbags: EnablePlayerSettings is off in worldserver.conf, "
                               "bags would be re-granted on every login. Module disabled.");
    }

    bool BagIsUsable()
    {
        if (!cfg.BagEntry)
            return false;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(cfg.BagEntry);
        if (!proto)
        {
            LOG_ERROR("module", "mod-bigbags: BagEntry {} does not exist in item_template.", cfg.BagEntry);
            return false;
        }

        if (proto->Class != ITEM_CLASS_CONTAINER)
        {
            LOG_ERROR("module", "mod-bigbags: BagEntry {} ({}) is not a container.", cfg.BagEntry, proto->Name1);
            return false;
        }

        return true;
    }
}

class BigBags_WorldScript : public WorldScript
{
public:
    BigBags_WorldScript() : WorldScript("BigBags_WorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        LoadConfig();
    }
};

class BigBags_PlayerScript : public PlayerScript
{
public:
    BigBags_PlayerScript() : PlayerScript("BigBags_PlayerScript", { PLAYERHOOK_ON_LOGIN }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!cfg.Enable || !player)
            return;

        // Without player settings the grant cannot be remembered, so skip it
        // rather than handing out an unlimited bag supply.
        if (!sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED))
            return;

        if (player->GetPlayerSetting(SETTINGS_SOURCE, SETTING_GRANTED).IsEnabled())
            return;

        if (!BagIsUsable())
            return;

        uint8 granted = 0;
        for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
        {
            if (player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                continue;

            if (player->StoreNewItemInBestSlots(cfg.BagEntry, 1))
                ++granted;
        }

        player->UpdatePlayerSetting(SETTINGS_SOURCE, SETTING_GRANTED, 1);

        if (granted && cfg.Announce && player->GetSession() && !player->GetSession()->IsBot())
            ChatHandler(player->GetSession()).PSendSysMessage("{}", cfg.AnnounceText);

        if (granted)
            LOG_DEBUG("module", "mod-bigbags: granted {} bag(s) to {}.", granted, player->GetName());
    }
};

void AddBigBagsScripts()
{
    new BigBags_WorldScript();
    new BigBags_PlayerScript();
}
