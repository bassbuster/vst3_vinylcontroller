//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/againentry.cpp
// Created by  : Steinberg, 04/2005
// Description : AGain Example for VST 3.0
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

#include "vinylprocessor.h"     // for AGain
#include "vinylcontroller.h"    // for AGainController
#include "vinylcids.h"          // for class ids
#include "version.h"            // for versioning

#include "public.sdk/source/main/pluginfactory_constexpr.h"


// //------------------------------------------------------------------------
// //  Module init/exit
// //------------------------------------------------------------------------

// void* hInstance = 0;

// //------------------------------------------------------------------------
// // called after library was loaded
// bool InitModule ()
// {
// 	extern void* moduleHandle;
// 	hInstance = moduleHandle;
// 	return true;
// }

// //------------------------------------------------------------------------
// // called after library is unloaded
// bool DeinitModule ()
// {
// 	return true;
// }

// using namespace Steinberg::Vst;

//------------------------------------------------------------------------
//  VST Plug-In Entry
//------------------------------------------------------------------------
// Windows: do not forget to include a .def file in your project to export
// GetPluginFactory function!
//------------------------------------------------------------------------

BEGIN_FACTORY_DEF ("BassBuster",
              "http://www.bassbuster.ru",
              "mailto:bassbuster@mail.ru",
              2)

//---First Plug-in included in this factory-------
// its kVstAudioEffectClass component
DEF_CLASS (Steinberg::Vst::AVinylProcessorUID,
           Steinberg::PClassInfo::kManyInstances,  // cardinality
           kVstAudioEffectClass,                   // the component category (dont changed this)
           stringPluginName,                       // here the plug-in name (to be changed)
           Steinberg::Vst::kDistributable,         // means that component and controller could be distributed on different computers
           "Fx|LiveFx",                            // Subcategory for this plug-in (to be changed)
           FULL_VERSION_STR,                       // plug-in version (to be changed)
           kVstVersionString,                      // the VST 3 SDK version (dont changed this, use always this define)
           Steinberg::Vst::AVinyl::createInstance, // function pointer called when this component should be instanciated
           nullptr)

// its kVstComponentControllerClass component
DEF_CLASS (Steinberg::Vst::AVinylControllerUID,
           Steinberg::PClassInfo::kManyInstances,               // cardinality
           kVstComponentControllerClass,                        // the Controller category (dont changed this)
           stringPluginName" Controller",                       // controller name (could be the same than component name)
           0,                                                   // not used here
           "",                                                  // not used here
           FULL_VERSION_STR,                                    // plug-in version (to be changed)
           kVstVersionString,                                   // the VST 3 SDK version (dont changed this, use always this define)
           Steinberg::Vst::AVinylController::createInstance,    // function pointer called when this component should be instanciated
           nullptr)

//----for others plug-ins contained in this factory, put like for the first plug-in different DEF_CLASS2---

END_FACTORY



