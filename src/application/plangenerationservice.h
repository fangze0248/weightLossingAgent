#pragma once

#include "interfaces/IPlanGenerationService.h"

class IExerciseRepository;
class IFeedbackService;
class IHealthCalculator;
class IPlanRepository;
class IRecipeRepository;
class IUserRepository;
class IWeeklyPlanner;

class PlanGenerationService final : public IPlanGenerationService
{
public:
    PlanGenerationService(IUserRepository& userRepository,
                          IExerciseRepository& exerciseRepository,
                          IRecipeRepository& recipeRepository,
                          IPlanRepository& planRepository,
                          IHealthCalculator& healthCalculator,
                          IWeeklyPlanner& weeklyPlanner,
                          IFeedbackService& feedbackService);

    ServiceResult<WeeklyPlan> generateAndSave(
        const QString& userId,
        const QDate& startDate,
        const WeeklyPlanOptions& options = {}) override;

private:
    IUserRepository& userRepository_;
    IExerciseRepository& exerciseRepository_;
    IRecipeRepository& recipeRepository_;
    IPlanRepository& planRepository_;
    IHealthCalculator& healthCalculator_;
    IWeeklyPlanner& weeklyPlanner_;
    IFeedbackService& feedbackService_;
};

