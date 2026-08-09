/**
 * @file Introspection.h
 * @brief Passive-level process observation owned by the common lifecycle.
 */
#pragma once

#include "Hypervisor.h"

_IRQL_requires_(PASSIVE_LEVEL)
/** @brief Registers introspection callbacks for a running hypervisor state. */
NTSTATUS
HvIntrospectionStart(
    _Inout_ HV_STATE* State
    );

_IRQL_requires_(PASSIVE_LEVEL)
/** @brief Unregisters callbacks and drains their access to `State`. */
NTSTATUS
HvIntrospectionStop(
    _Inout_ HV_STATE* State
    );
