#pragma once
#include <Arduino.h>
#include <Logger.h>
#include "SerialLogAdapter.h"

// Redefine Serial for all user code
#define Serial SerialLog

