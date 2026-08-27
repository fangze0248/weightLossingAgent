#include "interfaces/IMealRecommender.h"

#include <cmath>

int main()
{
    // 这里先验证公共食谱推荐选项的默认契约。
    // MealRecommender 实现完成后，再增加食谱组合业务测试。
    const MealRecommendationOptions options;

    if (std::abs(options.toleranceRatio - 0.10) > 1e-9) {
        return 1;
    }

    const double mealRatioTotal =
        options.breakfastRatio
        + options.lunchRatio
        + options.dinnerRatio
        + options.snackRatio;

    if (std::abs(mealRatioTotal - 1.0) > 1e-9) {
        return 2;
    }

    if (options.maximumItemsPerMeal != 2) {
        return 3;
    }

    if (options.includeSnack) {
        return 4;
    }

    return 0;
}
