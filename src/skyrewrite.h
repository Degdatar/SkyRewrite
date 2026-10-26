#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include "PrismaUI_API.h"

#if defined(SKYOS_SKYREWRITE_EXPORTS)
#define SKYOS_SKYREWRITE_API __declspec(dllexport)
#else
#define SKYOS_SKYREWRITE_API __declspec(dllimport)
#endif

namespace SkyOS {
    // SkyRewrite stores its observational event journal in the active save.
    SKYOS_SKYREWRITE_API bool InitializeSkyRewriteDashboard(PRISMA_UI_API::IVPrismaUI1* prisma, PrismaView view);
    SKYOS_SKYREWRITE_API void RegisterSkyRewriteSerialization();
    SKYOS_SKYREWRITE_API void InstallSkyRewriteEventSinks();
    SKYOS_SKYREWRITE_API void SetSkyRewriteLocalEchoEnabled(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteLocalEchoNotificationEnabled(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteAdaptiveGuardResponseEnabled(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTemporaryBountyEnabled(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTemporaryBountyAmount(std::uint32_t amount);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTemporaryBountyDuration(float days);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTemporaryBountyPressureScaling(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteBountyVictimMode(std::uint32_t mode);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTheftBountyEnabled(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTheftBountyAmount(std::uint32_t amount);
    SKYOS_SKYREWRITE_API void SetSkyRewriteTheftBountyThreshold(std::uint32_t thefts);
    SKYOS_SKYREWRITE_API void SetSkyRewriteDispatchGuardOnBounty(bool enabled);
    SKYOS_SKYREWRITE_API void SetSkyRewriteLocalEchoRules(std::uint32_t killThreshold, float lookbackDays, float cooldownDays);
    SKYOS_SKYREWRITE_API void SetSkyRewriteJournalRetention(std::uint32_t maxEvents);
    [[nodiscard]] SKYOS_SKYREWRITE_API std::string UndoSkyRewriteTemporaryBounty(const std::string& identity);
    [[nodiscard]] SKYOS_SKYREWRITE_API std::string ClearSkyRewriteHoldBounty(const std::string& holdKey);
    SKYOS_SKYREWRITE_API void ClearSkyRewriteHistory();
    [[nodiscard]] SKYOS_SKYREWRITE_API std::string GetSkyRewriteHistoryJSON(std::size_t maxVisibleEvents = 0, std::size_t eventOffset = 0);
}
