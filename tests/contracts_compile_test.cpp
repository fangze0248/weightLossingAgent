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
#include "models/UserProfile.h"

#include "recommendation/recommendationcore.h"

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

    (void)user;
    (void)exercise;
    (void)recipe;

    return 0;
}
