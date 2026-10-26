#ifndef SKYREWRITE_PUBLIC_API_H
#define SKYREWRITE_PUBLIC_API_H

/*
 * SkyRewrite public C ABI, version 1.
 * This header uses GetProcAddress and does not require SkyRewrite.lib.
 */
#include <stdint.h>
#pragma push_macro("InterlockedCompareExchange")
#include <windows.h>
#pragma pop_macro("InterlockedCompareExchange")

#ifdef __cplusplus
extern "C" {
#endif

#define SKYREWRITE_API_VERSION 1u

/* Public eventType values for player progression, crafting, disenchanting, and brawling. */
enum SkyRewriteEventTypeV1 {
    SKYREWRITE_EVENT_EQUIPPED = 30,
    SKYREWRITE_EVENT_UNEQUIPPED = 31,
    SKYREWRITE_EVENT_SKILL_INCREASED = 32,
    SKYREWRITE_EVENT_LEVEL_INCREASED = 33,
    SKYREWRITE_EVENT_BOOK_READ = 34,
    SKYREWRITE_EVENT_INGREDIENT_EFFECT_LEARNED = 35,
    SKYREWRITE_EVENT_FOLLOWER_STARTED = 36,
    SKYREWRITE_EVENT_FOLLOWER_STOPPED = 37,
    SKYREWRITE_EVENT_ITEM_SMITHED = 38,
    SKYREWRITE_EVENT_ITEM_TANNED = 39,
    SKYREWRITE_EVENT_POTION_CRAFTED = 40,
    SKYREWRITE_EVENT_ITEM_ENCHANTED = 41,
    SKYREWRITE_EVENT_CRAFT_MATERIAL_USED = 42,
    SKYREWRITE_EVENT_ITEM_COOKED = 43,
    SKYREWRITE_EVENT_ITEM_DISENCHANTED = 44,
    SKYREWRITE_EVENT_BRAWL = 45
};

typedef struct SkyRewriteEventV1 {
    uint32_t structSize;       /* Set to sizeof(SkyRewriteEventV1) before calling. */
    uint32_t eventType;        /* SkyRewrite journal eventType value. */
    float gameDay;             /* Negative when the event has no game-day timestamp. */
    uint32_t formID;
    uint32_t locationFormID;
    uint32_t baseFormID;
    uint16_t stage;
    uint16_t reserved;
    char formPlugin[64];
    char locationPlugin[64];
    char basePlugin[64];
    char subject[64];
    char detail[128];
    char location[64];
    char identity[96];
} SkyRewriteEventV1;

typedef struct SkyRewriteGameBountyV1 {
    uint32_t structSize;       /* Set to sizeof(SkyRewriteGameBountyV1) before calling. */
    uint32_t formID;           /* Skyrim hold crime-faction FormID. */
    uint32_t violent;
    uint32_t nonViolent;
    uint32_t total;
    char holdKey[24];
    char hold[64];
    char formPlugin[64];
} SkyRewriteGameBountyV1;

typedef uint32_t (__cdecl *SkyRewriteGetAPIVersionFn)(void);
typedef uint32_t (__cdecl *SkyRewriteGetVisibleEventCountFn)(void);
typedef int32_t (__cdecl *SkyRewriteGetVisibleEventAtFn)(uint32_t index, SkyRewriteEventV1* eventOut);
typedef uint32_t (__cdecl *SkyRewriteGetActiveGameBountyCountFn)(void);
typedef int32_t (__cdecl *SkyRewriteGetActiveGameBountyAtFn)(uint32_t index, SkyRewriteGameBountyV1* bountyOut);

typedef struct SkyRewriteAPI {
    uint32_t structSize;
    uint32_t apiVersion;
    SkyRewriteGetAPIVersionFn getAPIVersion;
    SkyRewriteGetVisibleEventCountFn getVisibleEventCount;
    SkyRewriteGetVisibleEventAtFn getVisibleEventAt;
    SkyRewriteGetActiveGameBountyCountFn getActiveGameBountyCount;
    SkyRewriteGetActiveGameBountyAtFn getActiveGameBountyAt;
} SkyRewriteAPI;

/* Pass the already-loaded SkyRewrite.dll module handle. Returns nonzero on success. */
static inline int32_t SkyRewrite_LoadAPI(HMODULE module, SkyRewriteAPI* apiOut)
{
    if (!module || !apiOut) return 0;
    SkyRewriteAPI api = { 0 };
    api.structSize = (uint32_t)sizeof(api);
    api.getAPIVersion = (SkyRewriteGetAPIVersionFn)(void*)GetProcAddress(module, "SkyRewrite_GetAPIVersion");
    api.getVisibleEventCount = (SkyRewriteGetVisibleEventCountFn)(void*)GetProcAddress(module, "SkyRewrite_GetVisibleEventCount");
    api.getVisibleEventAt = (SkyRewriteGetVisibleEventAtFn)(void*)GetProcAddress(module, "SkyRewrite_GetVisibleEventAt");
    api.getActiveGameBountyCount = (SkyRewriteGetActiveGameBountyCountFn)(void*)GetProcAddress(module, "SkyRewrite_GetActiveGameBountyCount");
    api.getActiveGameBountyAt = (SkyRewriteGetActiveGameBountyAtFn)(void*)GetProcAddress(module, "SkyRewrite_GetActiveGameBountyAt");
    if (!api.getAPIVersion || !api.getVisibleEventCount || !api.getVisibleEventAt ||
        !api.getActiveGameBountyCount || !api.getActiveGameBountyAt) return 0;
    api.apiVersion = api.getAPIVersion();
    if (api.apiVersion < SKYREWRITE_API_VERSION) return 0;
    *apiOut = api;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SKYREWRITE_PUBLIC_API_H */
