#ifndef SAFEPRIORITYTHREADPOOLWRAPPER_H
#define SAFEPRIORITYTHREADPOOLWRAPPER_H

#include "ConvenientJobCancellingAsyncCallWrapper_t.hpp"

#include "prioritythreadpoolinterface.h"

typedef ConvenientJobCancellingAsyncCallWrapper_t<NullData, PriorityThreadPoolInterface, JobPrio> SimpleSafePriorityThreadPoolWrapper;

#endif // SAFEPRIORITYTHREADPOOLWRAPPER_H
