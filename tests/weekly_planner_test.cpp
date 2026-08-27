#include "interfaces/IWeeklyPlanner.h"

int main()
{
    // 这里先验证公共周计划选项的默认契约。
    // WeeklyPlanner 实现完成后，再增加七天计划的集成测试。
    const WeeklyPlanOptions options;

    if (options.numberOfDays != 7) {
        return 1;
    }

    if (!options.avoidConsecutiveDuplicateExercises) {
        return 2;
    }

    if (!options.avoidConsecutiveDuplicateRecipes) {
        return 3;
    }

    return 0;
}
