#pragma once

enum
{
	/** parameter ID */
	kGainId = 0,	///< for the gain value (is automatable)
	kVuLeftId,		///< for the Vu value return to host (ReadOnly parameter for our UI)
	kVuRightId,		///< for the Vu value return to host (ReadOnly parameter for our UI)
	kPositionId,	///< for the player wave (ReadOnly parameter for our UI)
	kSpeedId,		///< for the player wave (ReadOnly parameter for our UI)
	kBypassId,		///< Bypass value (we will handle the bypass process) (is automatable)
	kVolumeId,
	kPitchId,
	kCurrentEntryId,
	kCurrentSceneId,
	kLoopId,
	kSyncId,
	kReverseId,
	kVolCurveId,
	kPitchSwitchId,
	kTimecodeLearnId,
	kPunchInId,
	kPunchOutId,
	kDistorsionId,
	kPreRollId,
	kPostRollId,
	kHoldId,
	kFreezeId,
	kVintageId,
	kLockToneId,
	kAmpId,
	kTuneId
};
