//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/againparamids.h
// Created by  : Steinberg, 12/2007
// Description : define the parameter IDs used by AGain
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2010, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// This Software Development Kit may not be distributed in parts or its entirety  
// without prior written agreement by Steinberg Media Technologies GmbH. 
// This SDK must not be used to re-engineer or manipulate any technology used  
// in any Steinberg or Third-party application or software module, 
// unless permitted by law.
// Neither the name of the Steinberg Media Technologies nor the names of its
// contributors may be used to endorse or promote products derived from this 
// software without specific prior written permission.
// 
// THIS SDK IS PROVIDED BY STEINBERG MEDIA TECHNOLOGIES GMBH "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL STEINBERG MEDIA TECHNOLOGIES GMBH BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

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
	//kCurrentPadId,
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
	//kPadId = 1000,
	//kPadTypeId = 2000,
	//kPadValueId = 3000

};

enum effectFlags {
	eNoEffects		= 0,
	eDistorsion		= 1 << 0,
	ePreRoll		= 1 << 1,
	ePostRoll  		= 1 << 2,
	ePunchIn 		= 1 << 3,
	ePunchOut		= 1 << 4,
	eHold			= 1 << 5,
	eFreeze			= 1 << 6,
	eVintage		= 1 << 7,
	eLockTone		= 1 << 8
};

