#include "recommendation/nutritiontargetcalculator.h"

namespace {

constexpr double kProteinEnergyRatio = 0.20;
constexpr double kCarbohydrateEnergyRatio = 0.50;
constexpr double kFatEnergyRatio = 0.30;
constexpr double kCaloriesPerGramProtein = 4.0;
constexpr double kCaloriesPerGramCarbohydrate = 4.0;
constexpr double kCaloriesPerGramFat = 9.0;

} // namespace

NutritionFacts nutrition_target::calculateDefault(double targetCalories)
{
    NutritionFacts target;
    target.caloriesKcal = targetCalories;
    target.proteinG = targetCalories
        * kProteinEnergyRatio / kCaloriesPerGramProtein;
    target.carbohydrateG = targetCalories
        * kCarbohydrateEnergyRatio / kCaloriesPerGramCarbohydrate;
    target.fatG = targetCalories
        * kFatEnergyRatio / kCaloriesPerGramFat;
    return target;
}
