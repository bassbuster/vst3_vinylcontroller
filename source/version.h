#pragma once

#include "projectversion.h"
#ifdef DEVELOPMENT
#define stringPluginName 		"Vinyl Sampler 2 dev"
#else
#define stringPluginName 		"Vinyl Sampler 2"
#endif
#if  defined(_WIN64) || defined(__x86_64__)
#define stringFileDescription	"VinylSampler VST3-SDK (64Bit)"
#else
#define stringFileDescription	"VinylSampler VST3-SDK"
#endif
#define stringCompanyName		"BassBuster\0"
#define stringLegalCopyright	"(c) 2011 BassBuster"
#define stringLegalTrademarks	"VST is a trademark of Steinberg Media Technologies GmbH"

