#pragma once

#include "models/NutritionFacts.h"

namespace nutrition_target {

// 普通成年减重方案的默认三大营养素目标：蛋白质 20%、
// 碳水 50%、脂肪 30%。调用方负责保证 targetCalories 为正数。
NutritionFacts calculateDefault(double targetCalories);

} // namespace nutrition_target
