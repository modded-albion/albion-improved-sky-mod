/******************************************************************************
 * Copyright (c) 2026 Masters of Albion Reforged developers
 *****************************************************************************/

#include "core/SkyRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace moa_mod::sky
{

LoadStage NextStage(LoadStage stage, StepResult result)
{
	if (stage == LoadStage::Ready || stage == LoadStage::Failed)
	{
		return stage;
	}
	switch (result)
	{
	case StepResult::Wait:
		return stage;
	case StepResult::Error:
		return LoadStage::Failed;
	case StepResult::Done:
		break;
	}
	switch (stage)
	{
	case LoadStage::WriteFile:
		return LoadStage::StartLoad;
	case LoadStage::StartLoad:
		return LoadStage::WaitForBundle;
	case LoadStage::WaitForBundle:
		return LoadStage::LoadMaterial;
	case LoadStage::LoadMaterial:
		return LoadStage::PlaceQuad;
	case LoadStage::PlaceQuad:
	case LoadStage::Ready:
		return LoadStage::Ready;
	case LoadStage::Failed:
		break;
	}
	return LoadStage::Failed;
}

std::string_view StageName(LoadStage stage)
{
	switch (stage)
	{
	case LoadStage::WriteFile:
		return "write the file";
	case LoadStage::StartLoad:
		return "start the load";
	case LoadStage::WaitForBundle:
		return "wait for the bundle";
	case LoadStage::LoadMaterial:
		return "load the material";
	case LoadStage::PlaceQuad:
		return "make the test quad";
	case LoadStage::Ready:
		return "ready";
	case LoadStage::Failed:
		return "failed";
	}
	return "unknown";
}

bool BundleFileIsCurrent(std::span<const std::byte> onDisk, std::span<const std::byte> embedded)
{
	return !embedded.empty() && std::ranges::equal(onDisk, embedded);
}

bool IsNight(float dayNightCycle)
{
	// The sun angle of the game is the cycle times 360 less 90 degrees. The sun is below the horizon for the
	// angles from 180 to 360 degrees, thus for the cycle from 0.75 to 1 and from 0 to 0.25.
	const auto cycle = dayNightCycle - std::floor(dayNightCycle);
	return cycle < 0.25f || cycle >= 0.75f;
}

QuadPlacement PlaceOnSky(float distance, float altitudeDegrees, float headingDegrees)
{
	constexpr auto k_Radians = std::numbers::pi_v<float> / 180.0f;
	const auto altitude = altitudeDegrees * k_Radians;
	const auto heading = headingDegrees * k_Radians;
	const WorldPoint direction {std::sin(heading) * std::cos(altitude), std::sin(altitude),
	                            std::cos(heading) * std::cos(altitude)};
	return {.offset = direction * distance, .rotation = LookRotation(direction * -1.0f, WorldPoint {0.0f, 1.0f, 0.0f})};
}

} // namespace moa_mod::sky
