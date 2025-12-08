#include "vinylprocessor.h"     // for AGain
#include "vinylcontroller.h"    // for AGainController
#include "vinylcids.h"          // for class ids
#include "version.h"            // for versioning

#include "public.sdk/source/main/pluginfactory_constexpr.h"

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



