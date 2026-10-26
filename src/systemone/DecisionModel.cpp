#include "DecisionModel.h"
#include "RemoteDecisionModel.h"
#include "LocalDecisionModel.h"
#include "SystemOneConfig.h"

namespace pluto {
namespace systemone {

std::unique_ptr<DecisionModel> CreateDecisionModel(
    DecisionModelType type,
    const SystemOneConfig& config) {

    switch (type) {
        case DecisionModelType::REMOTE_KEV:
        case DecisionModelType::REMOTE_LAYA:
        case DecisionModelType::REMOTE_JEV:
            return std::make_unique<RemoteDecisionModel>(type, config);

        case DecisionModelType::LOCAL:
            return std::make_unique<LocalDecisionModel>();

        case DecisionModelType::AUTO: {
            // Try remote first, fall back to local
            auto remote = std::make_unique<RemoteDecisionModel>(
                DecisionModelType::REMOTE_KEV, config);
            if (remote->is_available()) {
                return remote;
            }
            return std::make_unique<LocalDecisionModel>();
        }
    }

    // Fallback
    return std::make_unique<LocalDecisionModel>();
}

} // namespace systemone
} // namespace pluto
