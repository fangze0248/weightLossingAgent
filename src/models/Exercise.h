#pragma once

#include "DomainEnums.h"

#include <QString>

struct Exercise {
    QString id;
    QString name;
    double metValue = 0.0;
    ExerciseCategory category = ExerciseCategory::Other;
    QString description;
};
