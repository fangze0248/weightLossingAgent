#include "contracts/ServiceResult.h"

#include "interfaces/IAppFacade.h"
#include "interfaces/IDataExchangeService.h"
#include "interfaces/IExerciseRecommender.h"
#include "interfaces/IExerciseRepository.h"
#include "interfaces/IFeedbackRepository.h"
#include "interfaces/IFeedbackService.h"
#include "interfaces/IHealthCalculator.h"
#include "interfaces/IMealRecommender.h"
#include "interfaces/IPlanRepository.h"
#include "interfaces/IRecipeRepository.h"
#include "interfaces/IUserRepository.h"
#include "interfaces/IWeeklyPlanner.h"

#include "models/DomainEnums.h"
#include "models/Exercise.h"
#include "models/Feedback.h"
#include "models/PlanModels.h"
#include "models/Recipe.h"
#include "models/RecommendationPreference.h"
#include "models/UserProfile.h"

#include "recommendation/recommendationcore.h"

#include <cmath>

int main()
{
    UserProfile user;
    Exercise exercise;
    Recipe recipe;
    WeeklyPlanOptions options;

    const auto result = ServiceResult<WeeklyPlan>::success(WeeklyPlan{});

    if (!result.ok) {
        return 1;
    }

    if (options.numberOfDays != 7) {
        return 2;
    }

    if (recommendation_core::interfaceVersion() != 1) {
        return 3;
    }

    user.exerciseGoal = ExerciseGoal::MuscleGain;
    if (toStorageString(user.exerciseGoal) != QStringLiteral("muscle_gain")
        || exerciseGoalFromStorageString(QStringLiteral("build_fitness"))
            != ExerciseGoal::BuildFitness) {
        return 4;
    }

    const auto oneStarWeight = feedbackWeightFromStars(1);
    const auto twoStarWeight = feedbackWeightFromStars(2);
    const auto threeStarWeight = feedbackWeightFromStars(3);
    const auto fourStarWeight = feedbackWeightFromStars(4);
    const auto fiveStarWeight = feedbackWeightFromStars(5);
    if (!oneStarWeight.has_value()
        || !twoStarWeight.has_value()
        || !threeStarWeight.has_value()
        || !fourStarWeight.has_value()
        || !fiveStarWeight.has_value()
        || std::abs(*oneStarWeight - 0.6) > 1e-9
        || std::abs(*twoStarWeight - 0.8) > 1e-9
        || std::abs(*threeStarWeight - 1.0) > 1e-9
        || std::abs(*fourStarWeight - 1.2) > 1e-9
        || std::abs(*fiveStarWeight - 1.4) > 1e-9
        || feedbackWeightFromStars(0).has_value()) {
        return 5;
    }

    (void)user;
    (void)exercise;
    (void)recipe;

    return 0;
}
