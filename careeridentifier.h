#ifndef CAREERIDENTIFIER_H
#define CAREERIDENTIFIER_H

#include "genericidentifier.h"

inline int createCareerId(int professionId, int careerId)
{
    return professionId * 100 + careerId;
}

#endif // CAREERIDENTIFIER_H
