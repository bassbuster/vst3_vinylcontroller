#include "resourcepath.h"

#include "vstgui/lib/platform/platformfactory.h"

#if MAC
#include "vstgui/lib/platform/mac/macfactory.h"
using VSTGUIPlatformFactory = VSTGUI::MacFactory;
#elif WINDOWS
#include "vstgui/lib/platform/win32/win32factory.h"
using VSTGUIPlatformFactory = VSTGUI::Win32Factory;
#elif LINUX
#include "vstgui/lib/platform/linux/linuxfactory.h"
using VSTGUIPlatformFactory = VSTGUI::LinuxFactory;
#endif

namespace Steinberg {
namespace Vst {

std::string getResourcePath() {
#if MAC
    return VSTGUI::getPlatformFactory().asMacFactory()->getResourceBasePath();
#elif WINDOWS
    return VSTGUI::getPlatformFactory().asWin32Factory()->getResourceBasePath().value().getString();
#elif LINUX
    return VSTGUI::getPlatformFactory().asLinuxFactory()->getResourcePath();
#endif
}


}}
