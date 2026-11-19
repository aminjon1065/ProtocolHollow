#pragma once

#include "UObject/Class.h"
#include "UObject/ReflectedTypeAccessors.h"

/** Short name of a UENUM value for logs and console output: "InSession" instead of "EHollowSessionState::InSession". */
template <typename TEnum>
FString HollowEnumToString(const TEnum Value)
{
	return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(Value));
}
