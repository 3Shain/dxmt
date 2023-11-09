#include "objc-wrapper/message.h"
#include "objc-wrapper/runtime.h"

#define NO_BLOCK_IMPL
#define _EXTERN __attribute__((dllexport))
#include "Foundation/Foundation.hpp"
#include "QuartzCore/QuartzCore.hpp"
#include "Metal/Metal.hpp"

// #define _NS_IMPORT __attribute__((weak_import))

#define RTLD_DEFAULT ((void *)-2)
extern "C" SYSV_ABI void *unix_dlsym(void *__handle, const char *__symbol);

namespace NS::Private {
template <typename _Type> inline _Type LoadSymbol(const char *pSymbol) {
  const _Type *pAddress =
      static_cast<_Type *>(unix_dlsym(RTLD_DEFAULT, pSymbol));

  return pAddress ? *pAddress : nullptr;
}
} // namespace NS::Private

#define _NS_DEF_CLS(symbol)                                                    \
  NS::Private::Class::s_k##symbol = objc_lookUpClass(#symbol)
#define _NS_DEF_PRO(symbol)                                                    \
  NS::Private::Protocol::s_k##symbol = objc_getProtocol(#symbol)
#define _NS_DEF_SEL(accessor, symbol)                                          \
  NS::Private::Selector::s_k##accessor = sel_registerName(symbol)
// #define _NS_DEF_CONST(type, symbol)
//   _NS_EXTERN type const NS##symbol _NS_IMPORT;
//   type const NS::symbol = (nullptr != &NS##symbol) ? NS##symbol : nullptr
#define _NS_DEF_CONST(type, symbol)                                            \
  NS::symbol = NS::Private::LoadSymbol<type>("NS" #symbol)

static void __init_NS() {

  // classes

  _NS_DEF_CLS(NSArray);
  _NS_DEF_CLS(NSAutoreleasePool);
  _NS_DEF_CLS(NSBundle);
  _NS_DEF_CLS(NSCondition);
  _NS_DEF_CLS(NSDate);
  _NS_DEF_CLS(NSDictionary);
  _NS_DEF_CLS(NSError);
  _NS_DEF_CLS(NSNotificationCenter);
  _NS_DEF_CLS(NSNumber);
  _NS_DEF_CLS(NSObject);
  _NS_DEF_CLS(NSProcessInfo);
  _NS_DEF_CLS(NSSet);
  _NS_DEF_CLS(NSString);
  _NS_DEF_CLS(NSURL);
  _NS_DEF_CLS(NSValue);

  // selectors

  _NS_DEF_SEL(addObject_, "addObject:");
  _NS_DEF_SEL(addObserverName_object_queue_block_,
              "addObserverForName:object:queue:usingBlock:");
  _NS_DEF_SEL(activeProcessorCount, "activeProcessorCount");
  _NS_DEF_SEL(allBundles, "allBundles");
  _NS_DEF_SEL(allFrameworks, "allFrameworks");
  _NS_DEF_SEL(allObjects, "allObjects");
  _NS_DEF_SEL(alloc, "alloc");
  _NS_DEF_SEL(appStoreReceiptURL, "appStoreReceiptURL");
  _NS_DEF_SEL(arguments, "arguments");
  _NS_DEF_SEL(array, "array");
  _NS_DEF_SEL(arrayWithObject_, "arrayWithObject:");
  _NS_DEF_SEL(arrayWithObjects_count_, "arrayWithObjects:count:");
  _NS_DEF_SEL(automaticTerminationSupportEnabled,
              "automaticTerminationSupportEnabled");
  _NS_DEF_SEL(autorelease, "autorelease");
  _NS_DEF_SEL(beginActivityWithOptions_reason_,
              "beginActivityWithOptions:reason:");
  _NS_DEF_SEL(boolValue, "boolValue");
  _NS_DEF_SEL(broadcast, "broadcast");
  _NS_DEF_SEL(builtInPlugInsPath, "builtInPlugInsPath");
  _NS_DEF_SEL(builtInPlugInsURL, "builtInPlugInsURL");
  _NS_DEF_SEL(bundleIdentifier, "bundleIdentifier");
  _NS_DEF_SEL(bundlePath, "bundlePath");
  _NS_DEF_SEL(bundleURL, "bundleURL");
  _NS_DEF_SEL(bundleWithPath_, "bundleWithPath:");
  _NS_DEF_SEL(bundleWithURL_, "bundleWithURL:");
  _NS_DEF_SEL(caseInsensitiveCompare_, "caseInsensitiveCompare:");
  _NS_DEF_SEL(characterAtIndex_, "characterAtIndex:");
  _NS_DEF_SEL(charValue, "charValue");
  _NS_DEF_SEL(countByEnumeratingWithState_objects_count_,
              "countByEnumeratingWithState:objects:count:");
  _NS_DEF_SEL(cStringUsingEncoding_, "cStringUsingEncoding:");
  _NS_DEF_SEL(code, "code");
  _NS_DEF_SEL(compare_, "compare:");
  _NS_DEF_SEL(copy, "copy");
  _NS_DEF_SEL(count, "count");
  _NS_DEF_SEL(dateWithTimeIntervalSinceNow_, "dateWithTimeIntervalSinceNow:");
  _NS_DEF_SEL(defaultCenter, "defaultCenter");
  _NS_DEF_SEL(descriptionWithLocale_, "descriptionWithLocale:");
  _NS_DEF_SEL(disableAutomaticTermination_, "disableAutomaticTermination:");
  _NS_DEF_SEL(disableSuddenTermination, "disableSuddenTermination");
  _NS_DEF_SEL(debugDescription, "debugDescription");
  _NS_DEF_SEL(description, "description");
  _NS_DEF_SEL(dictionary, "dictionary");
  _NS_DEF_SEL(dictionaryWithObject_forKey_, "dictionaryWithObject:forKey:");
  _NS_DEF_SEL(dictionaryWithObjects_forKeys_count_,
              "dictionaryWithObjects:forKeys:count:");
  _NS_DEF_SEL(domain, "domain");
  _NS_DEF_SEL(doubleValue, "doubleValue");
  _NS_DEF_SEL(drain, "drain");
  _NS_DEF_SEL(enableAutomaticTermination_, "enableAutomaticTermination:");
  _NS_DEF_SEL(enableSuddenTermination, "enableSuddenTermination");
  _NS_DEF_SEL(endActivity_, "endActivity:");
  _NS_DEF_SEL(environment, "environment");
  _NS_DEF_SEL(errorWithDomain_code_userInfo_, "errorWithDomain:code:userInfo:");
  _NS_DEF_SEL(executablePath, "executablePath");
  _NS_DEF_SEL(executableURL, "executableURL");
  _NS_DEF_SEL(fileSystemRepresentation, "fileSystemRepresentation");
  _NS_DEF_SEL(fileURLWithPath_, "fileURLWithPath:");
  _NS_DEF_SEL(floatValue, "floatValue");
  _NS_DEF_SEL(fullUserName, "fullUserName");
  _NS_DEF_SEL(getValue_size_, "getValue:size:");
  _NS_DEF_SEL(globallyUniqueString, "globallyUniqueString");
  _NS_DEF_SEL(hash, "hash");
  _NS_DEF_SEL(hostName, "hostName");
  _NS_DEF_SEL(infoDictionary, "infoDictionary");
  _NS_DEF_SEL(init, "init");
  _NS_DEF_SEL(initFileURLWithPath_, "initFileURLWithPath:");
  _NS_DEF_SEL(initWithBool_, "initWithBool:");
  _NS_DEF_SEL(initWithBytes_objCType_, "initWithBytes:objCType:");
  _NS_DEF_SEL(initWithBytesNoCopy_length_encoding_freeWhenDone_,
              "initWithBytesNoCopy:length:encoding:freeWhenDone:");
  _NS_DEF_SEL(initWithChar_, "initWithChar:");
  _NS_DEF_SEL(initWithCoder_, "initWithCoder:");
  _NS_DEF_SEL(initWithCString_encoding_, "initWithCString:encoding:");
  _NS_DEF_SEL(initWithDomain_code_userInfo_, "initWithDomain:code:userInfo:");
  _NS_DEF_SEL(initWithDouble_, "initWithDouble:");
  _NS_DEF_SEL(initWithFloat_, "initWithFloat:");
  _NS_DEF_SEL(initWithInt_, "initWithInt:");
  _NS_DEF_SEL(initWithLong_, "initWithLong:");
  _NS_DEF_SEL(initWithLongLong_, "initWithLongLong:");
  _NS_DEF_SEL(initWithObjects_count_, "initWithObjects:count:");
  _NS_DEF_SEL(initWithObjects_forKeys_count_, "initWithObjects:forKeys:count:");
  _NS_DEF_SEL(initWithPath_, "initWithPath:");
  _NS_DEF_SEL(initWithShort_, "initWithShort:");
  _NS_DEF_SEL(initWithString_, "initWithString:");
  _NS_DEF_SEL(initWithUnsignedChar_, "initWithUnsignedChar:");
  _NS_DEF_SEL(initWithUnsignedInt_, "initWithUnsignedInt:");
  _NS_DEF_SEL(initWithUnsignedLong_, "initWithUnsignedLong:");
  _NS_DEF_SEL(initWithUnsignedLongLong_, "initWithUnsignedLongLong:");
  _NS_DEF_SEL(initWithUnsignedShort_, "initWithUnsignedShort:");
  _NS_DEF_SEL(initWithURL_, "initWithURL:");
  _NS_DEF_SEL(integerValue, "integerValue");
  _NS_DEF_SEL(intValue, "intValue");
  _NS_DEF_SEL(isEqual_, "isEqual:");
  _NS_DEF_SEL(isEqualToNumber_, "isEqualToNumber:");
  _NS_DEF_SEL(isEqualToString_, "isEqualToString:");
  _NS_DEF_SEL(isEqualToValue_, "isEqualToValue:");
  _NS_DEF_SEL(isiOSAppOnMac, "isiOSAppOnMac");
  _NS_DEF_SEL(isLoaded, "isLoaded");
  _NS_DEF_SEL(isLowPowerModeEnabled, "isLowPowerModeEnabled");
  _NS_DEF_SEL(isMacCatalystApp, "isMacCatalystApp");
  _NS_DEF_SEL(isOperatingSystemAtLeastVersion_,
              "isOperatingSystemAtLeastVersion:");
  _NS_DEF_SEL(keyEnumerator, "keyEnumerator");
  _NS_DEF_SEL(length, "length");
  _NS_DEF_SEL(lengthOfBytesUsingEncoding_, "lengthOfBytesUsingEncoding:");
  _NS_DEF_SEL(load, "load");
  _NS_DEF_SEL(loadAndReturnError_, "loadAndReturnError:");
  _NS_DEF_SEL(localizedDescription, "localizedDescription");
  _NS_DEF_SEL(localizedFailureReason, "localizedFailureReason");
  _NS_DEF_SEL(localizedInfoDictionary, "localizedInfoDictionary");
  _NS_DEF_SEL(localizedRecoveryOptions, "localizedRecoveryOptions");
  _NS_DEF_SEL(localizedRecoverySuggestion, "localizedRecoverySuggestion");
  _NS_DEF_SEL(localizedStringForKey_value_table_,
              "localizedStringForKey:value:table:");
  _NS_DEF_SEL(lock, "lock");
  _NS_DEF_SEL(longValue, "longValue");
  _NS_DEF_SEL(longLongValue, "longLongValue");
  _NS_DEF_SEL(mainBundle, "mainBundle");
  _NS_DEF_SEL(maximumLengthOfBytesUsingEncoding_,
              "maximumLengthOfBytesUsingEncoding:");
  _NS_DEF_SEL(methodSignatureForSelector_, "methodSignatureForSelector:");
  _NS_DEF_SEL(mutableBytes, "mutableBytes");
  _NS_DEF_SEL(name, "name");
  _NS_DEF_SEL(nextObject, "nextObject");
  _NS_DEF_SEL(numberWithBool_, "numberWithBool:");
  _NS_DEF_SEL(numberWithChar_, "numberWithChar:");
  _NS_DEF_SEL(numberWithDouble_, "numberWithDouble:");
  _NS_DEF_SEL(numberWithFloat_, "numberWithFloat:");
  _NS_DEF_SEL(numberWithInt_, "numberWithInt:");
  _NS_DEF_SEL(numberWithLong_, "numberWithLong:");
  _NS_DEF_SEL(numberWithLongLong_, "numberWithLongLong:");
  _NS_DEF_SEL(numberWithShort_, "numberWithShort:");
  _NS_DEF_SEL(numberWithUnsignedChar_, "numberWithUnsignedChar:");
  _NS_DEF_SEL(numberWithUnsignedInt_, "numberWithUnsignedInt:");
  _NS_DEF_SEL(numberWithUnsignedLong_, "numberWithUnsignedLong:");
  _NS_DEF_SEL(numberWithUnsignedLongLong_, "numberWithUnsignedLongLong:");
  _NS_DEF_SEL(numberWithUnsignedShort_, "numberWithUnsignedShort:");
  _NS_DEF_SEL(objCType, "objCType");
  _NS_DEF_SEL(object, "object");
  _NS_DEF_SEL(objectAtIndex_, "objectAtIndex:");
  _NS_DEF_SEL(objectEnumerator, "objectEnumerator");
  _NS_DEF_SEL(objectForInfoDictionaryKey_, "objectForInfoDictionaryKey:");
  _NS_DEF_SEL(objectForKey_, "objectForKey:");
  _NS_DEF_SEL(operatingSystem, "operatingSystem");
  _NS_DEF_SEL(operatingSystemVersion, "operatingSystemVersion");
  _NS_DEF_SEL(operatingSystemVersionString, "operatingSystemVersionString");
  _NS_DEF_SEL(pathForAuxiliaryExecutable_, "pathForAuxiliaryExecutable:");
  _NS_DEF_SEL(performActivityWithOptions_reason_usingBlock_,
              "performActivityWithOptions:reason:usingBlock:");
  _NS_DEF_SEL(performExpiringActivityWithReason_usingBlock_,
              "performExpiringActivityWithReason:usingBlock:");
  _NS_DEF_SEL(physicalMemory, "physicalMemory");
  _NS_DEF_SEL(pointerValue, "pointerValue");
  _NS_DEF_SEL(preflightAndReturnError_, "preflightAndReturnError:");
  _NS_DEF_SEL(privateFrameworksPath, "privateFrameworksPath");
  _NS_DEF_SEL(privateFrameworksURL, "privateFrameworksURL");
  _NS_DEF_SEL(processIdentifier, "processIdentifier");
  _NS_DEF_SEL(processInfo, "processInfo");
  _NS_DEF_SEL(processName, "processName");
  _NS_DEF_SEL(processorCount, "processorCount");
  _NS_DEF_SEL(rangeOfString_options_, "rangeOfString:options:");
  _NS_DEF_SEL(release, "release");
  _NS_DEF_SEL(removeObserver_, "removeObserver:");
  _NS_DEF_SEL(resourcePath, "resourcePath");
  _NS_DEF_SEL(resourceURL, "resourceURL");
  _NS_DEF_SEL(respondsToSelector_, "respondsToSelector:");
  _NS_DEF_SEL(retain, "retain");
  _NS_DEF_SEL(retainCount, "retainCount");
  _NS_DEF_SEL(setAutomaticTerminationSupportEnabled_,
              "setAutomaticTerminationSupportEnabled:");
  _NS_DEF_SEL(setProcessName_, "setProcessName:");
  _NS_DEF_SEL(sharedFrameworksPath, "sharedFrameworksPath");
  _NS_DEF_SEL(sharedFrameworksURL, "sharedFrameworksURL");
  _NS_DEF_SEL(sharedSupportPath, "sharedSupportPath");
  _NS_DEF_SEL(sharedSupportURL, "sharedSupportURL");
  _NS_DEF_SEL(shortValue, "shortValue");
  _NS_DEF_SEL(showPools, "showPools");
  _NS_DEF_SEL(signal, "signal");
  _NS_DEF_SEL(string, "string");
  _NS_DEF_SEL(stringValue, "stringValue");
  _NS_DEF_SEL(stringWithString_, "stringWithString:");
  _NS_DEF_SEL(stringWithCString_encoding_, "stringWithCString:encoding:");
  _NS_DEF_SEL(stringByAppendingString_, "stringByAppendingString:");
  _NS_DEF_SEL(systemUptime, "systemUptime");
  _NS_DEF_SEL(thermalState, "thermalState");
  _NS_DEF_SEL(unload, "unload");
  _NS_DEF_SEL(unlock, "unlock");
  _NS_DEF_SEL(unsignedCharValue, "unsignedCharValue");
  _NS_DEF_SEL(unsignedIntegerValue, "unsignedIntegerValue");
  _NS_DEF_SEL(unsignedIntValue, "unsignedIntValue");
  _NS_DEF_SEL(unsignedLongValue, "unsignedLongValue");
  _NS_DEF_SEL(unsignedLongLongValue, "unsignedLongLongValue");
  _NS_DEF_SEL(unsignedShortValue, "unsignedShortValue");
  _NS_DEF_SEL(URLForAuxiliaryExecutable_, "URLForAuxiliaryExecutable:");
  _NS_DEF_SEL(userInfo, "userInfo");
  _NS_DEF_SEL(userName, "userName");
  _NS_DEF_SEL(UTF8String, "UTF8String");
  _NS_DEF_SEL(valueWithBytes_objCType_, "valueWithBytes:objCType:");
  _NS_DEF_SEL(valueWithPointer_, "valueWithPointer:");
  _NS_DEF_SEL(wait, "wait");
  _NS_DEF_SEL(waitUntilDate_, "waitUntilDate:");
  // constants
  _NS_DEF_CONST(NS::NotificationName, BundleDidLoadNotification);
  _NS_DEF_CONST(NS::NotificationName,
                BundleResourceRequestLowDiskSpaceNotification);

  _NS_DEF_CONST(NS::ErrorDomain, CocoaErrorDomain);
  _NS_DEF_CONST(NS::ErrorDomain, POSIXErrorDomain);
  _NS_DEF_CONST(NS::ErrorDomain, OSStatusErrorDomain);
  _NS_DEF_CONST(NS::ErrorDomain, MachErrorDomain);

  _NS_DEF_CONST(NS::ErrorUserInfoKey, UnderlyingErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, LocalizedDescriptionKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, LocalizedFailureReasonErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, LocalizedRecoverySuggestionErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, LocalizedRecoveryOptionsErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, RecoveryAttempterErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, HelpAnchorErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, DebugDescriptionErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, LocalizedFailureErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, StringEncodingErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, URLErrorKey);
  _NS_DEF_CONST(NS::ErrorUserInfoKey, FilePathErrorKey);

  _NS_DEF_CONST(NS::NotificationName,
                ProcessInfoThermalStateDidChangeNotification);
  _NS_DEF_CONST(NS::NotificationName,
                ProcessInfoPowerStateDidChangeNotification);
}

// ---------------------------------------

#define _CA_IMPORT __attribute__((weak_import))

#define _CA_DEF_CLS(symbol)                                                    \
  CA::Private::Class::s_k##symbol = objc_lookUpClass(#symbol)
#define _CA_DEF_PRO(symbol)                                                    \
  CA::Private::Protocol::s_k##symbol = objc_getProtocol(#symbol)
#define _CA_DEF_SEL(accessor, symbol)                                          \
  CA::Private::Selector::s_k##accessor = sel_registerName(symbol)
#define _CA_DEF_STR(type, symbol)                                              \
  _CA_EXTERN type const CA##symbol _CA_IMPORT;                                 \
  type const CA::symbol = (nullptr != &CA##symbol) ? CA##symbol : nullptr

static void __init_ca() {
  // classes
  _CA_DEF_CLS(CAMetalLayer);
  // protocal
  _CA_DEF_PRO(CAMetalDrawable);
  // selectors
  _CA_DEF_SEL(device, "device");
  _CA_DEF_SEL(drawableSize, "drawableSize");
  _CA_DEF_SEL(framebufferOnly, "framebufferOnly");
  _CA_DEF_SEL(allowsNextDrawableTimeout, "allowsNextDrawableTimeout");
  _CA_DEF_SEL(layer, "layer");
  _CA_DEF_SEL(nextDrawable, "nextDrawable");
  _CA_DEF_SEL(pixelFormat, "pixelFormat");
  _CA_DEF_SEL(setDevice_, "setDevice:");
  _CA_DEF_SEL(setDrawableSize_, "setDrawableSize:");
  _CA_DEF_SEL(setFramebufferOnly_, "setFramebufferOnly:");
  _CA_DEF_SEL(setPixelFormat_, "setPixelFormat:");
  _CA_DEF_SEL(texture, "texture");
  //  constants
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#define _MTL_IMPORT __attribute__((weak_import))

#define _MTL_DEF_CLS(symbol)                                                   \
  MTL::Private::Class::s_k##symbol = objc_lookUpClass(#symbol)
#define _MTL_DEF_PRO(symbol)                                                   \
  MTL::Private::Protocol::s_k##symbol = objc_getProtocol(#symbol)
#define _MTL_DEF_SEL(accessor, symbol)                                         \
  MTL::Private::Selector::s_k##accessor = sel_registerName(symbol)

// not in use?

// #define MTL_DEF_FUNC(name, signature)
//   using Fn##name = signature;
//   Fn##name name = reinterpret_cast<Fn##name>(unix_dlsym(RTLD_DEFAULT, #name))

namespace MTL::Private {
template <typename _Type> inline _Type const LoadSymbol(const char *pSymbol) {
  const _Type *pAddress =
      static_cast<_Type *>(unix_dlsym(RTLD_DEFAULT, pSymbol));

  return pAddress ? *pAddress : nullptr;
}
} // namespace MTL::Private

#define _MTL_DEF_STR(type, symbol)                                             \
                                                                               \
  MTL::symbol = MTL::Private::LoadSymbol<type>("MTL" #symbol)

#define _MTL_DEF_CONST(type, symbol)                                           \
  MTL::symbol = MTL::Private::LoadSymbol<type>("MTL" #symbol)

#define _MTL_DEF_WEAK_CONST(type, symbol) _MTL_DEF_CONST(type, symbol)

static void __init_mtl() {

  // classes

  _MTL_DEF_CLS(MTLAccelerationStructureBoundingBoxGeometryDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructureDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructureGeometryDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructureMotionTriangleGeometryDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructurePassDescriptor);
  _MTL_DEF_CLS(MTLAccelerationStructurePassSampleBufferAttachmentDescriptor);
  _MTL_DEF_CLS(
      MTLAccelerationStructurePassSampleBufferAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLAccelerationStructureTriangleGeometryDescriptor);
  _MTL_DEF_CLS(MTLArgument);
  _MTL_DEF_CLS(MTLArgumentDescriptor);
  _MTL_DEF_CLS(MTLArrayType);
  _MTL_DEF_CLS(MTLAttribute);
  _MTL_DEF_CLS(MTLAttributeDescriptor);
  _MTL_DEF_CLS(MTLAttributeDescriptorArray);
  _MTL_DEF_CLS(MTLBinaryArchiveDescriptor);
  _MTL_DEF_CLS(MTLBlitPassDescriptor);
  _MTL_DEF_CLS(MTLBlitPassSampleBufferAttachmentDescriptor);
  _MTL_DEF_CLS(MTLBlitPassSampleBufferAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLBufferLayoutDescriptor);
  _MTL_DEF_CLS(MTLBufferLayoutDescriptorArray);
  _MTL_DEF_CLS(MTLCaptureDescriptor);
  _MTL_DEF_CLS(MTLCaptureManager);
  _MTL_DEF_CLS(MTLCommandBufferDescriptor);
  _MTL_DEF_CLS(MTLCompileOptions);
  _MTL_DEF_CLS(MTLComputePassDescriptor);
  _MTL_DEF_CLS(MTLComputePassSampleBufferAttachmentDescriptor);
  _MTL_DEF_CLS(MTLComputePassSampleBufferAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLComputePipelineDescriptor);
  _MTL_DEF_CLS(MTLComputePipelineReflection);
  _MTL_DEF_CLS(MTLCounterSampleBufferDescriptor);
  _MTL_DEF_CLS(MTLDepthStencilDescriptor);
  _MTL_DEF_CLS(MTLFunctionConstant);
  _MTL_DEF_CLS(MTLFunctionConstantValues);
  _MTL_DEF_CLS(MTLFunctionDescriptor);
  _MTL_DEF_CLS(MTLFunctionStitchingAttributeAlwaysInline);
  _MTL_DEF_CLS(MTLFunctionStitchingFunctionNode);
  _MTL_DEF_CLS(MTLFunctionStitchingGraph);
  _MTL_DEF_CLS(MTLFunctionStitchingInputNode);
  _MTL_DEF_CLS(MTLHeapDescriptor);
  _MTL_DEF_CLS(MTLIOCommandQueueDescriptor);
  _MTL_DEF_CLS(MTLIndirectCommandBufferDescriptor);
  _MTL_DEF_CLS(MTLInstanceAccelerationStructureDescriptor);
  _MTL_DEF_CLS(MTLIntersectionFunctionDescriptor);
  _MTL_DEF_CLS(MTLIntersectionFunctionTableDescriptor);
  _MTL_DEF_CLS(MTLLinkedFunctions);
  _MTL_DEF_CLS(MTLMeshRenderPipelineDescriptor);
  _MTL_DEF_CLS(MTLMotionKeyframeData);
  _MTL_DEF_CLS(MTLPipelineBufferDescriptor);
  _MTL_DEF_CLS(MTLPipelineBufferDescriptorArray);
  _MTL_DEF_CLS(MTLPointerType);
  _MTL_DEF_CLS(MTLPrimitiveAccelerationStructureDescriptor);
  _MTL_DEF_CLS(MTLRasterizationRateLayerArray);
  _MTL_DEF_CLS(MTLRasterizationRateLayerDescriptor);
  _MTL_DEF_CLS(MTLRasterizationRateMapDescriptor);
  _MTL_DEF_CLS(MTLRasterizationRateSampleArray);
  _MTL_DEF_CLS(MTLRenderPassAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPassColorAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPassColorAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLRenderPassDepthAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPassDescriptor);
  _MTL_DEF_CLS(MTLRenderPassSampleBufferAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPassSampleBufferAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLRenderPassStencilAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPipelineColorAttachmentDescriptor);
  _MTL_DEF_CLS(MTLRenderPipelineColorAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLRenderPipelineDescriptor);
  _MTL_DEF_CLS(MTLRenderPipelineFunctionsDescriptor);
  _MTL_DEF_CLS(MTLRenderPipelineReflection);
  _MTL_DEF_CLS(MTLResourceStatePassDescriptor);
  _MTL_DEF_CLS(MTLResourceStatePassSampleBufferAttachmentDescriptor);
  _MTL_DEF_CLS(MTLResourceStatePassSampleBufferAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLSamplerDescriptor);
  _MTL_DEF_CLS(MTLSharedEventHandle);
  _MTL_DEF_CLS(MTLSharedEventListener);
  _MTL_DEF_CLS(MTLSharedTextureHandle);
  _MTL_DEF_CLS(MTLStageInputOutputDescriptor);
  _MTL_DEF_CLS(MTLStencilDescriptor);
  _MTL_DEF_CLS(MTLStitchedLibraryDescriptor);
  _MTL_DEF_CLS(MTLStructMember);
  _MTL_DEF_CLS(MTLStructType);
  _MTL_DEF_CLS(MTLTextureDescriptor);
  _MTL_DEF_CLS(MTLTextureReferenceType);
  _MTL_DEF_CLS(MTLTileRenderPipelineColorAttachmentDescriptor);
  _MTL_DEF_CLS(MTLTileRenderPipelineColorAttachmentDescriptorArray);
  _MTL_DEF_CLS(MTLTileRenderPipelineDescriptor);
  _MTL_DEF_CLS(MTLType);
  _MTL_DEF_CLS(MTLVertexAttribute);
  _MTL_DEF_CLS(MTLVertexAttributeDescriptor);
  _MTL_DEF_CLS(MTLVertexAttributeDescriptorArray);
  _MTL_DEF_CLS(MTLVertexBufferLayoutDescriptor);
  _MTL_DEF_CLS(MTLVertexBufferLayoutDescriptorArray);
  _MTL_DEF_CLS(MTLVertexDescriptor);
  _MTL_DEF_CLS(MTLVisibleFunctionTableDescriptor);
  // protocols

  _MTL_DEF_PRO(MTLAccelerationStructure);
  _MTL_DEF_PRO(MTLAccelerationStructureCommandEncoder);
  _MTL_DEF_PRO(MTLArgumentEncoder);
  _MTL_DEF_PRO(MTLBinaryArchive);
  _MTL_DEF_PRO(MTLBinding);
  _MTL_DEF_PRO(MTLBlitCommandEncoder);
  _MTL_DEF_PRO(MTLBuffer);
  _MTL_DEF_PRO(MTLBufferBinding);
  _MTL_DEF_PRO(MTLCommandBuffer);
  _MTL_DEF_PRO(MTLCommandBufferEncoderInfo);
  _MTL_DEF_PRO(MTLCommandEncoder);
  _MTL_DEF_PRO(MTLCommandQueue);
  _MTL_DEF_PRO(MTLComputeCommandEncoder);
  _MTL_DEF_PRO(MTLComputePipelineState);
  _MTL_DEF_PRO(MTLCounter);
  _MTL_DEF_PRO(MTLCounterSampleBuffer);
  _MTL_DEF_PRO(MTLCounterSet);
  _MTL_DEF_PRO(MTLDepthStencilState);
  _MTL_DEF_PRO(MTLDevice);
  _MTL_DEF_PRO(MTLDrawable);
  _MTL_DEF_PRO(MTLDynamicLibrary);
  _MTL_DEF_PRO(MTLEvent);
  _MTL_DEF_PRO(MTLFence);
  _MTL_DEF_PRO(MTLFunction);
  _MTL_DEF_PRO(MTLFunctionHandle);
  _MTL_DEF_PRO(MTLFunctionLog);
  _MTL_DEF_PRO(MTLFunctionLogDebugLocation);
  _MTL_DEF_PRO(MTLFunctionStitchingAttribute);
  _MTL_DEF_PRO(MTLFunctionStitchingNode);
  _MTL_DEF_PRO(MTLHeap);
  _MTL_DEF_PRO(MTLIOCommandBuffer);
  _MTL_DEF_PRO(MTLIOCommandQueue);
  _MTL_DEF_PRO(MTLIOFileHandle);
  _MTL_DEF_PRO(MTLIOScratchBuffer);
  _MTL_DEF_PRO(MTLIOScratchBufferAllocator);
  _MTL_DEF_PRO(MTLIndirectCommandBuffer);
  _MTL_DEF_PRO(MTLIndirectComputeCommand);
  _MTL_DEF_PRO(MTLIndirectRenderCommand);
  _MTL_DEF_PRO(MTLIntersectionFunctionTable);
  _MTL_DEF_PRO(MTLLibrary);
  _MTL_DEF_PRO(MTLLogContainer);
  _MTL_DEF_PRO(MTLObjectPayloadBinding);
  _MTL_DEF_PRO(MTLParallelRenderCommandEncoder);
  _MTL_DEF_PRO(MTLRasterizationRateMap);
  _MTL_DEF_PRO(MTLRenderCommandEncoder);
  _MTL_DEF_PRO(MTLRenderPipelineState);
  _MTL_DEF_PRO(MTLResource);
  _MTL_DEF_PRO(MTLResourceStateCommandEncoder);
  _MTL_DEF_PRO(MTLSamplerState);
  _MTL_DEF_PRO(MTLSharedEvent);
  _MTL_DEF_PRO(MTLTexture);
  _MTL_DEF_PRO(MTLTextureBinding);
  _MTL_DEF_PRO(MTLThreadgroupBinding);
  _MTL_DEF_PRO(MTLVisibleFunctionTable);
  // selectors

  _MTL_DEF_SEL(GPUEndTime, "GPUEndTime");
  _MTL_DEF_SEL(GPUStartTime, "GPUStartTime");
  _MTL_DEF_SEL(URL, "URL");
  _MTL_DEF_SEL(accelerationStructureCommandEncoder,
               "accelerationStructureCommandEncoder");
  _MTL_DEF_SEL(accelerationStructureCommandEncoderWithDescriptor_,
               "accelerationStructureCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(accelerationStructurePassDescriptor,
               "accelerationStructurePassDescriptor");
  _MTL_DEF_SEL(accelerationStructureSizesWithDescriptor_,
               "accelerationStructureSizesWithDescriptor:");
  _MTL_DEF_SEL(access, "access");
  _MTL_DEF_SEL(addBarrier, "addBarrier");
  _MTL_DEF_SEL(addCompletedHandler_, "addCompletedHandler:");
  _MTL_DEF_SEL(addComputePipelineFunctionsWithDescriptor_error_,
               "addComputePipelineFunctionsWithDescriptor:error:");
  _MTL_DEF_SEL(addDebugMarker_range_, "addDebugMarker:range:");
  _MTL_DEF_SEL(addFunctionWithDescriptor_library_error_,
               "addFunctionWithDescriptor:library:error:");
  _MTL_DEF_SEL(addPresentedHandler_, "addPresentedHandler:");
  _MTL_DEF_SEL(addRenderPipelineFunctionsWithDescriptor_error_,
               "addRenderPipelineFunctionsWithDescriptor:error:");
  _MTL_DEF_SEL(addScheduledHandler_, "addScheduledHandler:");
  _MTL_DEF_SEL(addTileRenderPipelineFunctionsWithDescriptor_error_,
               "addTileRenderPipelineFunctionsWithDescriptor:error:");
  _MTL_DEF_SEL(alignment, "alignment");
  _MTL_DEF_SEL(allocatedSize, "allocatedSize");
  _MTL_DEF_SEL(allowDuplicateIntersectionFunctionInvocation,
               "allowDuplicateIntersectionFunctionInvocation");
  _MTL_DEF_SEL(allowGPUOptimizedContents, "allowGPUOptimizedContents");
  _MTL_DEF_SEL(allowReferencingUndefinedSymbols,
               "allowReferencingUndefinedSymbols");
  _MTL_DEF_SEL(alphaBlendOperation, "alphaBlendOperation");
  _MTL_DEF_SEL(areBarycentricCoordsSupported, "areBarycentricCoordsSupported");
  _MTL_DEF_SEL(areProgrammableSamplePositionsSupported,
               "areProgrammableSamplePositionsSupported");
  _MTL_DEF_SEL(areRasterOrderGroupsSupported, "areRasterOrderGroupsSupported");
  _MTL_DEF_SEL(argumentBuffersSupport, "argumentBuffersSupport");
  _MTL_DEF_SEL(argumentDescriptor, "argumentDescriptor");
  _MTL_DEF_SEL(argumentIndex, "argumentIndex");
  _MTL_DEF_SEL(argumentIndexStride, "argumentIndexStride");
  _MTL_DEF_SEL(arguments, "arguments");
  _MTL_DEF_SEL(arrayLength, "arrayLength");
  _MTL_DEF_SEL(arrayType, "arrayType");
  _MTL_DEF_SEL(attributeIndex, "attributeIndex");
  _MTL_DEF_SEL(attributeType, "attributeType");
  _MTL_DEF_SEL(attributes, "attributes");
  _MTL_DEF_SEL(backFaceStencil, "backFaceStencil");
  _MTL_DEF_SEL(binaryArchives, "binaryArchives");
  _MTL_DEF_SEL(binaryFunctions, "binaryFunctions");
  _MTL_DEF_SEL(bindings, "bindings");
  _MTL_DEF_SEL(blitCommandEncoder, "blitCommandEncoder");
  _MTL_DEF_SEL(blitCommandEncoderWithDescriptor_,
               "blitCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(blitPassDescriptor, "blitPassDescriptor");
  _MTL_DEF_SEL(borderColor, "borderColor");
  _MTL_DEF_SEL(boundingBoxBuffer, "boundingBoxBuffer");
  _MTL_DEF_SEL(boundingBoxBufferOffset, "boundingBoxBufferOffset");
  _MTL_DEF_SEL(boundingBoxBuffers, "boundingBoxBuffers");
  _MTL_DEF_SEL(boundingBoxCount, "boundingBoxCount");
  _MTL_DEF_SEL(boundingBoxStride, "boundingBoxStride");
  _MTL_DEF_SEL(buffer, "buffer");
  _MTL_DEF_SEL(bufferAlignment, "bufferAlignment");
  _MTL_DEF_SEL(bufferBytesPerRow, "bufferBytesPerRow");
  _MTL_DEF_SEL(bufferDataSize, "bufferDataSize");
  _MTL_DEF_SEL(bufferDataType, "bufferDataType");
  _MTL_DEF_SEL(bufferIndex, "bufferIndex");
  _MTL_DEF_SEL(bufferOffset, "bufferOffset");
  _MTL_DEF_SEL(bufferPointerType, "bufferPointerType");
  _MTL_DEF_SEL(bufferStructType, "bufferStructType");
  _MTL_DEF_SEL(buffers, "buffers");
  _MTL_DEF_SEL(
      buildAccelerationStructure_descriptor_scratchBuffer_scratchBufferOffset_,
      "buildAccelerationStructure:descriptor:scratchBuffer:"
      "scratchBufferOffset:");
  _MTL_DEF_SEL(captureObject, "captureObject");
  _MTL_DEF_SEL(clearBarrier, "clearBarrier");
  _MTL_DEF_SEL(clearColor, "clearColor");
  _MTL_DEF_SEL(clearDepth, "clearDepth");
  _MTL_DEF_SEL(clearStencil, "clearStencil");
  _MTL_DEF_SEL(colorAttachments, "colorAttachments");
  _MTL_DEF_SEL(column, "column");
  _MTL_DEF_SEL(commandBuffer, "commandBuffer");
  _MTL_DEF_SEL(commandBufferWithDescriptor_, "commandBufferWithDescriptor:");
  _MTL_DEF_SEL(commandBufferWithUnretainedReferences,
               "commandBufferWithUnretainedReferences");
  _MTL_DEF_SEL(commandQueue, "commandQueue");
  _MTL_DEF_SEL(commandTypes, "commandTypes");
  _MTL_DEF_SEL(commit, "commit");
  _MTL_DEF_SEL(compareFunction, "compareFunction");
  _MTL_DEF_SEL(compileSymbolVisibility, "compileSymbolVisibility");
  _MTL_DEF_SEL(compressionType, "compressionType");
  _MTL_DEF_SEL(computeCommandEncoder, "computeCommandEncoder");
  _MTL_DEF_SEL(computeCommandEncoderWithDescriptor_,
               "computeCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(computeCommandEncoderWithDispatchType_,
               "computeCommandEncoderWithDispatchType:");
  _MTL_DEF_SEL(computeFunction, "computeFunction");
  _MTL_DEF_SEL(computePassDescriptor, "computePassDescriptor");
  _MTL_DEF_SEL(concurrentDispatchThreadgroups_threadsPerThreadgroup_,
               "concurrentDispatchThreadgroups:threadsPerThreadgroup:");
  _MTL_DEF_SEL(concurrentDispatchThreads_threadsPerThreadgroup_,
               "concurrentDispatchThreads:threadsPerThreadgroup:");
  _MTL_DEF_SEL(constantBlockAlignment, "constantBlockAlignment");
  _MTL_DEF_SEL(constantDataAtIndex_, "constantDataAtIndex:");
  _MTL_DEF_SEL(constantValues, "constantValues");
  _MTL_DEF_SEL(contents, "contents");
  _MTL_DEF_SEL(controlDependencies, "controlDependencies");
  _MTL_DEF_SEL(
      convertSparsePixelRegions_toTileRegions_withTileSize_alignmentMode_numRegions_,
      "convertSparsePixelRegions:toTileRegions:withTileSize:alignmentMode:"
      "numRegions:");
  _MTL_DEF_SEL(
      convertSparseTileRegions_toPixelRegions_withTileSize_numRegions_,
      "convertSparseTileRegions:toPixelRegions:withTileSize:numRegions:");
  _MTL_DEF_SEL(copyAccelerationStructure_toAccelerationStructure_,
               "copyAccelerationStructure:toAccelerationStructure:");
  _MTL_DEF_SEL(copyAndCompactAccelerationStructure_toAccelerationStructure_,
               "copyAndCompactAccelerationStructure:toAccelerationStructure:");
  _MTL_DEF_SEL(
      copyFromBuffer_sourceOffset_sourceBytesPerRow_sourceBytesPerImage_sourceSize_toTexture_destinationSlice_destinationLevel_destinationOrigin_,
      "copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:"
      "sourceSize:toTexture:destinationSlice:destinationLevel:"
      "destinationOrigin:");
  _MTL_DEF_SEL(
      copyFromBuffer_sourceOffset_sourceBytesPerRow_sourceBytesPerImage_sourceSize_toTexture_destinationSlice_destinationLevel_destinationOrigin_options_,
      "copyFromBuffer:sourceOffset:sourceBytesPerRow:sourceBytesPerImage:"
      "sourceSize:toTexture:destinationSlice:destinationLevel:"
      "destinationOrigin:options:");
  _MTL_DEF_SEL(copyFromBuffer_sourceOffset_toBuffer_destinationOffset_size_,
               "copyFromBuffer:sourceOffset:toBuffer:destinationOffset:size:");
  _MTL_DEF_SEL(
      copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage_,
      "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:"
      "toBuffer:destinationOffset:destinationBytesPerRow:"
      "destinationBytesPerImage:");
  _MTL_DEF_SEL(
      copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toBuffer_destinationOffset_destinationBytesPerRow_destinationBytesPerImage_options_,
      "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:"
      "toBuffer:destinationOffset:destinationBytesPerRow:"
      "destinationBytesPerImage:options:");
  _MTL_DEF_SEL(
      copyFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toTexture_destinationSlice_destinationLevel_destinationOrigin_,
      "copyFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:"
      "toTexture:destinationSlice:destinationLevel:destinationOrigin:");
  _MTL_DEF_SEL(
      copyFromTexture_sourceSlice_sourceLevel_toTexture_destinationSlice_destinationLevel_sliceCount_levelCount_,
      "copyFromTexture:sourceSlice:sourceLevel:toTexture:destinationSlice:"
      "destinationLevel:sliceCount:levelCount:");
  _MTL_DEF_SEL(copyFromTexture_toTexture_, "copyFromTexture:toTexture:");
  _MTL_DEF_SEL(
      copyIndirectCommandBuffer_sourceRange_destination_destinationIndex_,
      "copyIndirectCommandBuffer:sourceRange:destination:destinationIndex:");
  _MTL_DEF_SEL(copyParameterDataToBuffer_offset_,
               "copyParameterDataToBuffer:offset:");
  _MTL_DEF_SEL(copyStatusToBuffer_offset_, "copyStatusToBuffer:offset:");
  _MTL_DEF_SEL(counterSet, "counterSet");
  _MTL_DEF_SEL(counterSets, "counterSets");
  _MTL_DEF_SEL(counters, "counters");
  _MTL_DEF_SEL(cpuCacheMode, "cpuCacheMode");
  _MTL_DEF_SEL(currentAllocatedSize, "currentAllocatedSize");
  _MTL_DEF_SEL(data, "data");
  _MTL_DEF_SEL(dataSize, "dataSize");
  _MTL_DEF_SEL(dataType, "dataType");
  _MTL_DEF_SEL(dealloc, "dealloc");
  _MTL_DEF_SEL(debugLocation, "debugLocation");
  _MTL_DEF_SEL(debugSignposts, "debugSignposts");
  _MTL_DEF_SEL(defaultCaptureScope, "defaultCaptureScope");
  _MTL_DEF_SEL(defaultRasterSampleCount, "defaultRasterSampleCount");
  _MTL_DEF_SEL(depth, "depth");
  _MTL_DEF_SEL(depthAttachment, "depthAttachment");
  _MTL_DEF_SEL(depthAttachmentPixelFormat, "depthAttachmentPixelFormat");
  _MTL_DEF_SEL(depthCompareFunction, "depthCompareFunction");
  _MTL_DEF_SEL(depthFailureOperation, "depthFailureOperation");
  _MTL_DEF_SEL(depthPlane, "depthPlane");
  _MTL_DEF_SEL(depthResolveFilter, "depthResolveFilter");
  _MTL_DEF_SEL(depthStencilPassOperation, "depthStencilPassOperation");
  _MTL_DEF_SEL(descriptor, "descriptor");
  _MTL_DEF_SEL(destination, "destination");
  _MTL_DEF_SEL(destinationAlphaBlendFactor, "destinationAlphaBlendFactor");
  _MTL_DEF_SEL(destinationRGBBlendFactor, "destinationRGBBlendFactor");
  _MTL_DEF_SEL(device, "device");
  _MTL_DEF_SEL(didModifyRange_, "didModifyRange:");
  _MTL_DEF_SEL(dispatchQueue, "dispatchQueue");
  _MTL_DEF_SEL(dispatchThreadgroups_threadsPerThreadgroup_,
               "dispatchThreadgroups:threadsPerThreadgroup:");
  _MTL_DEF_SEL(
      dispatchThreadgroupsWithIndirectBuffer_indirectBufferOffset_threadsPerThreadgroup_,
      "dispatchThreadgroupsWithIndirectBuffer:indirectBufferOffset:"
      "threadsPerThreadgroup:");
  _MTL_DEF_SEL(dispatchThreads_threadsPerThreadgroup_,
               "dispatchThreads:threadsPerThreadgroup:");
  _MTL_DEF_SEL(dispatchThreadsPerTile_, "dispatchThreadsPerTile:");
  _MTL_DEF_SEL(dispatchType, "dispatchType");
  _MTL_DEF_SEL(
      drawIndexedPatches_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_indirectBuffer_indirectBufferOffset_,
      "drawIndexedPatches:patchIndexBuffer:patchIndexBufferOffset:"
      "controlPointIndexBuffer:controlPointIndexBufferOffset:indirectBuffer:"
      "indirectBufferOffset:");
  _MTL_DEF_SEL(
      drawIndexedPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_instanceCount_baseInstance_,
      "drawIndexedPatches:patchStart:patchCount:patchIndexBuffer:"
      "patchIndexBufferOffset:controlPointIndexBuffer:"
      "controlPointIndexBufferOffset:instanceCount:baseInstance:");
  _MTL_DEF_SEL(
      drawIndexedPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_controlPointIndexBuffer_controlPointIndexBufferOffset_instanceCount_baseInstance_tessellationFactorBuffer_tessellationFactorBufferOffset_tessellationFactorBufferInstanceStride_,
      "drawIndexedPatches:patchStart:patchCount:patchIndexBuffer:"
      "patchIndexBufferOffset:controlPointIndexBuffer:"
      "controlPointIndexBufferOffset:instanceCount:baseInstance:"
      "tessellationFactorBuffer:tessellationFactorBufferOffset:"
      "tessellationFactorBufferInstanceStride:");
  _MTL_DEF_SEL(
      drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_,
      "drawIndexedPrimitives:indexCount:indexType:indexBuffer:"
      "indexBufferOffset:");
  _MTL_DEF_SEL(
      drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_instanceCount_,
      "drawIndexedPrimitives:indexCount:indexType:indexBuffer:"
      "indexBufferOffset:instanceCount:");
  _MTL_DEF_SEL(
      drawIndexedPrimitives_indexCount_indexType_indexBuffer_indexBufferOffset_instanceCount_baseVertex_baseInstance_,
      "drawIndexedPrimitives:indexCount:indexType:indexBuffer:"
      "indexBufferOffset:instanceCount:baseVertex:baseInstance:");
  _MTL_DEF_SEL(
      drawIndexedPrimitives_indexType_indexBuffer_indexBufferOffset_indirectBuffer_indirectBufferOffset_,
      "drawIndexedPrimitives:indexType:indexBuffer:indexBufferOffset:"
      "indirectBuffer:indirectBufferOffset:");
  _MTL_DEF_SEL(
      drawMeshThreadgroups_threadsPerObjectThreadgroup_threadsPerMeshThreadgroup_,
      "drawMeshThreadgroups:threadsPerObjectThreadgroup:"
      "threadsPerMeshThreadgroup:");
  _MTL_DEF_SEL(
      drawMeshThreadgroupsWithIndirectBuffer_indirectBufferOffset_threadsPerObjectThreadgroup_threadsPerMeshThreadgroup_,
      "drawMeshThreadgroupsWithIndirectBuffer:indirectBufferOffset:"
      "threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:");
  _MTL_DEF_SEL(
      drawMeshThreads_threadsPerObjectThreadgroup_threadsPerMeshThreadgroup_,
      "drawMeshThreads:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:");
  _MTL_DEF_SEL(
      drawPatches_patchIndexBuffer_patchIndexBufferOffset_indirectBuffer_indirectBufferOffset_,
      "drawPatches:patchIndexBuffer:patchIndexBufferOffset:indirectBuffer:"
      "indirectBufferOffset:");
  _MTL_DEF_SEL(
      drawPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_instanceCount_baseInstance_,
      "drawPatches:patchStart:patchCount:patchIndexBuffer:"
      "patchIndexBufferOffset:instanceCount:baseInstance:");
  _MTL_DEF_SEL(
      drawPatches_patchStart_patchCount_patchIndexBuffer_patchIndexBufferOffset_instanceCount_baseInstance_tessellationFactorBuffer_tessellationFactorBufferOffset_tessellationFactorBufferInstanceStride_,
      "drawPatches:patchStart:patchCount:patchIndexBuffer:"
      "patchIndexBufferOffset:instanceCount:baseInstance:"
      "tessellationFactorBuffer:tessellationFactorBufferOffset:"
      "tessellationFactorBufferInstanceStride:");
  _MTL_DEF_SEL(drawPrimitives_indirectBuffer_indirectBufferOffset_,
               "drawPrimitives:indirectBuffer:indirectBufferOffset:");
  _MTL_DEF_SEL(drawPrimitives_vertexStart_vertexCount_,
               "drawPrimitives:vertexStart:vertexCount:");
  _MTL_DEF_SEL(drawPrimitives_vertexStart_vertexCount_instanceCount_,
               "drawPrimitives:vertexStart:vertexCount:instanceCount:");
  _MTL_DEF_SEL(
      drawPrimitives_vertexStart_vertexCount_instanceCount_baseInstance_,
      "drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:");
  _MTL_DEF_SEL(drawableID, "drawableID");
  _MTL_DEF_SEL(elementArrayType, "elementArrayType");
  _MTL_DEF_SEL(elementIsArgumentBuffer, "elementIsArgumentBuffer");
  _MTL_DEF_SEL(elementPointerType, "elementPointerType");
  _MTL_DEF_SEL(elementStructType, "elementStructType");
  _MTL_DEF_SEL(elementTextureReferenceType, "elementTextureReferenceType");
  _MTL_DEF_SEL(elementType, "elementType");
  _MTL_DEF_SEL(encodeSignalEvent_value_, "encodeSignalEvent:value:");
  _MTL_DEF_SEL(encodeWaitForEvent_value_, "encodeWaitForEvent:value:");
  _MTL_DEF_SEL(encodedLength, "encodedLength");
  _MTL_DEF_SEL(encoderLabel, "encoderLabel");
  _MTL_DEF_SEL(endEncoding, "endEncoding");
  _MTL_DEF_SEL(endOfEncoderSampleIndex, "endOfEncoderSampleIndex");
  _MTL_DEF_SEL(endOfFragmentSampleIndex, "endOfFragmentSampleIndex");
  _MTL_DEF_SEL(endOfVertexSampleIndex, "endOfVertexSampleIndex");
  _MTL_DEF_SEL(enqueue, "enqueue");
  _MTL_DEF_SEL(enqueueBarrier, "enqueueBarrier");
  _MTL_DEF_SEL(error, "error");
  _MTL_DEF_SEL(errorOptions, "errorOptions");
  _MTL_DEF_SEL(errorState, "errorState");
  _MTL_DEF_SEL(executeCommandsInBuffer_indirectBuffer_indirectBufferOffset_,
               "executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:");
  _MTL_DEF_SEL(executeCommandsInBuffer_withRange_,
               "executeCommandsInBuffer:withRange:");
  _MTL_DEF_SEL(fastMathEnabled, "fastMathEnabled");
  _MTL_DEF_SEL(fillBuffer_range_value_, "fillBuffer:range:value:");
  _MTL_DEF_SEL(firstMipmapInTail, "firstMipmapInTail");
  _MTL_DEF_SEL(format, "format");
  _MTL_DEF_SEL(fragmentAdditionalBinaryFunctions,
               "fragmentAdditionalBinaryFunctions");
  _MTL_DEF_SEL(fragmentArguments, "fragmentArguments");
  _MTL_DEF_SEL(fragmentBindings, "fragmentBindings");
  _MTL_DEF_SEL(fragmentBuffers, "fragmentBuffers");
  _MTL_DEF_SEL(fragmentFunction, "fragmentFunction");
  _MTL_DEF_SEL(fragmentLinkedFunctions, "fragmentLinkedFunctions");
  _MTL_DEF_SEL(fragmentPreloadedLibraries, "fragmentPreloadedLibraries");
  _MTL_DEF_SEL(frontFaceStencil, "frontFaceStencil");
  _MTL_DEF_SEL(function, "function");
  _MTL_DEF_SEL(functionConstantsDictionary, "functionConstantsDictionary");
  _MTL_DEF_SEL(functionCount, "functionCount");
  _MTL_DEF_SEL(functionDescriptor, "functionDescriptor");
  _MTL_DEF_SEL(functionGraphs, "functionGraphs");
  _MTL_DEF_SEL(functionHandleWithFunction_, "functionHandleWithFunction:");
  _MTL_DEF_SEL(functionHandleWithFunction_stage_,
               "functionHandleWithFunction:stage:");
  _MTL_DEF_SEL(functionName, "functionName");
  _MTL_DEF_SEL(functionNames, "functionNames");
  _MTL_DEF_SEL(functionType, "functionType");
  _MTL_DEF_SEL(functions, "functions");
  _MTL_DEF_SEL(generateMipmapsForTexture_, "generateMipmapsForTexture:");
  _MTL_DEF_SEL(geometryDescriptors, "geometryDescriptors");
  _MTL_DEF_SEL(
      getBytes_bytesPerRow_bytesPerImage_fromRegion_mipmapLevel_slice_,
      "getBytes:bytesPerRow:bytesPerImage:fromRegion:mipmapLevel:slice:");
  _MTL_DEF_SEL(getBytes_bytesPerRow_fromRegion_mipmapLevel_,
               "getBytes:bytesPerRow:fromRegion:mipmapLevel:");
  _MTL_DEF_SEL(getDefaultSamplePositions_count_,
               "getDefaultSamplePositions:count:");
  _MTL_DEF_SEL(getSamplePositions_count_, "getSamplePositions:count:");
  _MTL_DEF_SEL(
      getTextureAccessCounters_region_mipLevel_slice_resetCounters_countersBuffer_countersBufferOffset_,
      "getTextureAccessCounters:region:mipLevel:slice:resetCounters:"
      "countersBuffer:countersBufferOffset:");
  _MTL_DEF_SEL(gpuAddress, "gpuAddress");
  _MTL_DEF_SEL(gpuResourceID, "gpuResourceID");
  _MTL_DEF_SEL(groups, "groups");
  _MTL_DEF_SEL(hasUnifiedMemory, "hasUnifiedMemory");
  _MTL_DEF_SEL(hazardTrackingMode, "hazardTrackingMode");
  _MTL_DEF_SEL(heap, "heap");
  _MTL_DEF_SEL(heapAccelerationStructureSizeAndAlignWithDescriptor_,
               "heapAccelerationStructureSizeAndAlignWithDescriptor:");
  _MTL_DEF_SEL(heapAccelerationStructureSizeAndAlignWithSize_,
               "heapAccelerationStructureSizeAndAlignWithSize:");
  _MTL_DEF_SEL(heapBufferSizeAndAlignWithLength_options_,
               "heapBufferSizeAndAlignWithLength:options:");
  _MTL_DEF_SEL(heapOffset, "heapOffset");
  _MTL_DEF_SEL(heapTextureSizeAndAlignWithDescriptor_,
               "heapTextureSizeAndAlignWithDescriptor:");
  _MTL_DEF_SEL(height, "height");
  _MTL_DEF_SEL(horizontal, "horizontal");
  _MTL_DEF_SEL(horizontalSampleStorage, "horizontalSampleStorage");
  _MTL_DEF_SEL(imageblockMemoryLengthForDimensions_,
               "imageblockMemoryLengthForDimensions:");
  _MTL_DEF_SEL(imageblockSampleLength, "imageblockSampleLength");
  _MTL_DEF_SEL(index, "index");
  _MTL_DEF_SEL(indexBuffer, "indexBuffer");
  _MTL_DEF_SEL(indexBufferIndex, "indexBufferIndex");
  _MTL_DEF_SEL(indexBufferOffset, "indexBufferOffset");
  _MTL_DEF_SEL(indexType, "indexType");
  _MTL_DEF_SEL(indirectComputeCommandAtIndex_,
               "indirectComputeCommandAtIndex:");
  _MTL_DEF_SEL(indirectRenderCommandAtIndex_, "indirectRenderCommandAtIndex:");
  _MTL_DEF_SEL(inheritBuffers, "inheritBuffers");
  _MTL_DEF_SEL(inheritPipelineState, "inheritPipelineState");
  _MTL_DEF_SEL(init, "init");
  _MTL_DEF_SEL(initWithArgumentIndex_, "initWithArgumentIndex:");
  _MTL_DEF_SEL(initWithDispatchQueue_, "initWithDispatchQueue:");
  _MTL_DEF_SEL(initWithFunctionName_nodes_outputNode_attributes_,
               "initWithFunctionName:nodes:outputNode:attributes:");
  _MTL_DEF_SEL(initWithName_arguments_controlDependencies_,
               "initWithName:arguments:controlDependencies:");
  _MTL_DEF_SEL(initWithSampleCount_, "initWithSampleCount:");
  _MTL_DEF_SEL(initWithSampleCount_horizontal_vertical_,
               "initWithSampleCount:horizontal:vertical:");
  _MTL_DEF_SEL(inputPrimitiveTopology, "inputPrimitiveTopology");
  _MTL_DEF_SEL(insertDebugCaptureBoundary, "insertDebugCaptureBoundary");
  _MTL_DEF_SEL(insertDebugSignpost_, "insertDebugSignpost:");
  _MTL_DEF_SEL(insertLibraries, "insertLibraries");
  _MTL_DEF_SEL(installName, "installName");
  _MTL_DEF_SEL(instanceCount, "instanceCount");
  _MTL_DEF_SEL(instanceDescriptorBuffer, "instanceDescriptorBuffer");
  _MTL_DEF_SEL(instanceDescriptorBufferOffset,
               "instanceDescriptorBufferOffset");
  _MTL_DEF_SEL(instanceDescriptorStride, "instanceDescriptorStride");
  _MTL_DEF_SEL(instanceDescriptorType, "instanceDescriptorType");
  _MTL_DEF_SEL(instancedAccelerationStructures,
               "instancedAccelerationStructures");
  _MTL_DEF_SEL(intersectionFunctionTableDescriptor,
               "intersectionFunctionTableDescriptor");
  _MTL_DEF_SEL(intersectionFunctionTableOffset,
               "intersectionFunctionTableOffset");
  _MTL_DEF_SEL(iosurface, "iosurface");
  _MTL_DEF_SEL(iosurfacePlane, "iosurfacePlane");
  _MTL_DEF_SEL(isActive, "isActive");
  _MTL_DEF_SEL(isAliasable, "isAliasable");
  _MTL_DEF_SEL(isAlphaToCoverageEnabled, "isAlphaToCoverageEnabled");
  _MTL_DEF_SEL(isAlphaToOneEnabled, "isAlphaToOneEnabled");
  _MTL_DEF_SEL(isArgument, "isArgument");
  _MTL_DEF_SEL(isBlendingEnabled, "isBlendingEnabled");
  _MTL_DEF_SEL(isCapturing, "isCapturing");
  _MTL_DEF_SEL(isDepth24Stencil8PixelFormatSupported,
               "isDepth24Stencil8PixelFormatSupported");
  _MTL_DEF_SEL(isDepthTexture, "isDepthTexture");
  _MTL_DEF_SEL(isDepthWriteEnabled, "isDepthWriteEnabled");
  _MTL_DEF_SEL(isFramebufferOnly, "isFramebufferOnly");
  _MTL_DEF_SEL(isHeadless, "isHeadless");
  _MTL_DEF_SEL(isLowPower, "isLowPower");
  _MTL_DEF_SEL(isPatchControlPointData, "isPatchControlPointData");
  _MTL_DEF_SEL(isPatchData, "isPatchData");
  _MTL_DEF_SEL(isRasterizationEnabled, "isRasterizationEnabled");
  _MTL_DEF_SEL(isRemovable, "isRemovable");
  _MTL_DEF_SEL(isShareable, "isShareable");
  _MTL_DEF_SEL(isSparse, "isSparse");
  _MTL_DEF_SEL(isTessellationFactorScaleEnabled,
               "isTessellationFactorScaleEnabled");
  _MTL_DEF_SEL(isUsed, "isUsed");
  _MTL_DEF_SEL(kernelEndTime, "kernelEndTime");
  _MTL_DEF_SEL(kernelStartTime, "kernelStartTime");
  _MTL_DEF_SEL(label, "label");
  _MTL_DEF_SEL(languageVersion, "languageVersion");
  _MTL_DEF_SEL(layerAtIndex_, "layerAtIndex:");
  _MTL_DEF_SEL(layerCount, "layerCount");
  _MTL_DEF_SEL(layers, "layers");
  _MTL_DEF_SEL(layouts, "layouts");
  _MTL_DEF_SEL(length, "length");
  _MTL_DEF_SEL(level, "level");
  _MTL_DEF_SEL(libraries, "libraries");
  _MTL_DEF_SEL(libraryType, "libraryType");
  _MTL_DEF_SEL(line, "line");
  _MTL_DEF_SEL(linkedFunctions, "linkedFunctions");
  _MTL_DEF_SEL(loadAction, "loadAction");
  _MTL_DEF_SEL(loadBuffer_offset_size_sourceHandle_sourceHandleOffset_,
               "loadBuffer:offset:size:sourceHandle:sourceHandleOffset:");
  _MTL_DEF_SEL(loadBytes_size_sourceHandle_sourceHandleOffset_,
               "loadBytes:size:sourceHandle:sourceHandleOffset:");
  _MTL_DEF_SEL(
      loadTexture_slice_level_size_sourceBytesPerRow_sourceBytesPerImage_destinationOrigin_sourceHandle_sourceHandleOffset_,
      "loadTexture:slice:level:size:sourceBytesPerRow:sourceBytesPerImage:"
      "destinationOrigin:sourceHandle:sourceHandleOffset:");
  _MTL_DEF_SEL(location, "location");
  _MTL_DEF_SEL(locationNumber, "locationNumber");
  _MTL_DEF_SEL(lodAverage, "lodAverage");
  _MTL_DEF_SEL(lodMaxClamp, "lodMaxClamp");
  _MTL_DEF_SEL(lodMinClamp, "lodMinClamp");
  _MTL_DEF_SEL(logs, "logs");
  _MTL_DEF_SEL(magFilter, "magFilter");
  _MTL_DEF_SEL(makeAliasable, "makeAliasable");
  _MTL_DEF_SEL(mapPhysicalToScreenCoordinates_forLayer_,
               "mapPhysicalToScreenCoordinates:forLayer:");
  _MTL_DEF_SEL(mapScreenToPhysicalCoordinates_forLayer_,
               "mapScreenToPhysicalCoordinates:forLayer:");
  _MTL_DEF_SEL(maxAnisotropy, "maxAnisotropy");
  _MTL_DEF_SEL(maxArgumentBufferSamplerCount, "maxArgumentBufferSamplerCount");
  _MTL_DEF_SEL(maxAvailableSizeWithAlignment_,
               "maxAvailableSizeWithAlignment:");
  _MTL_DEF_SEL(maxBufferLength, "maxBufferLength");
  _MTL_DEF_SEL(maxCallStackDepth, "maxCallStackDepth");
  _MTL_DEF_SEL(maxCommandBufferCount, "maxCommandBufferCount");
  _MTL_DEF_SEL(maxCommandsInFlight, "maxCommandsInFlight");
  _MTL_DEF_SEL(maxFragmentBufferBindCount, "maxFragmentBufferBindCount");
  _MTL_DEF_SEL(maxFragmentCallStackDepth, "maxFragmentCallStackDepth");
  _MTL_DEF_SEL(maxKernelBufferBindCount, "maxKernelBufferBindCount");
  _MTL_DEF_SEL(maxSampleCount, "maxSampleCount");
  _MTL_DEF_SEL(maxTessellationFactor, "maxTessellationFactor");
  _MTL_DEF_SEL(maxThreadgroupMemoryLength, "maxThreadgroupMemoryLength");
  _MTL_DEF_SEL(maxThreadsPerThreadgroup, "maxThreadsPerThreadgroup");
  _MTL_DEF_SEL(maxTotalThreadgroupsPerMeshGrid,
               "maxTotalThreadgroupsPerMeshGrid");
  _MTL_DEF_SEL(maxTotalThreadsPerMeshThreadgroup,
               "maxTotalThreadsPerMeshThreadgroup");
  _MTL_DEF_SEL(maxTotalThreadsPerObjectThreadgroup,
               "maxTotalThreadsPerObjectThreadgroup");
  _MTL_DEF_SEL(maxTotalThreadsPerThreadgroup, "maxTotalThreadsPerThreadgroup");
  _MTL_DEF_SEL(maxTransferRate, "maxTransferRate");
  _MTL_DEF_SEL(maxVertexAmplificationCount, "maxVertexAmplificationCount");
  _MTL_DEF_SEL(maxVertexBufferBindCount, "maxVertexBufferBindCount");
  _MTL_DEF_SEL(maxVertexCallStackDepth, "maxVertexCallStackDepth");
  _MTL_DEF_SEL(maximumConcurrentCompilationTaskCount,
               "maximumConcurrentCompilationTaskCount");
  _MTL_DEF_SEL(memberByName_, "memberByName:");
  _MTL_DEF_SEL(members, "members");
  _MTL_DEF_SEL(memoryBarrierWithResources_count_,
               "memoryBarrierWithResources:count:");
  _MTL_DEF_SEL(memoryBarrierWithResources_count_afterStages_beforeStages_,
               "memoryBarrierWithResources:count:afterStages:beforeStages:");
  _MTL_DEF_SEL(memoryBarrierWithScope_, "memoryBarrierWithScope:");
  _MTL_DEF_SEL(memoryBarrierWithScope_afterStages_beforeStages_,
               "memoryBarrierWithScope:afterStages:beforeStages:");
  _MTL_DEF_SEL(meshBindings, "meshBindings");
  _MTL_DEF_SEL(meshBuffers, "meshBuffers");
  _MTL_DEF_SEL(meshFunction, "meshFunction");
  _MTL_DEF_SEL(meshThreadExecutionWidth, "meshThreadExecutionWidth");
  _MTL_DEF_SEL(meshThreadgroupSizeIsMultipleOfThreadExecutionWidth,
               "meshThreadgroupSizeIsMultipleOfThreadExecutionWidth");
  _MTL_DEF_SEL(minFilter, "minFilter");
  _MTL_DEF_SEL(minimumLinearTextureAlignmentForPixelFormat_,
               "minimumLinearTextureAlignmentForPixelFormat:");
  _MTL_DEF_SEL(minimumTextureBufferAlignmentForPixelFormat_,
               "minimumTextureBufferAlignmentForPixelFormat:");
  _MTL_DEF_SEL(mipFilter, "mipFilter");
  _MTL_DEF_SEL(mipmapLevelCount, "mipmapLevelCount");
  _MTL_DEF_SEL(motionEndBorderMode, "motionEndBorderMode");
  _MTL_DEF_SEL(motionEndTime, "motionEndTime");
  _MTL_DEF_SEL(motionKeyframeCount, "motionKeyframeCount");
  _MTL_DEF_SEL(motionStartBorderMode, "motionStartBorderMode");
  _MTL_DEF_SEL(motionStartTime, "motionStartTime");
  _MTL_DEF_SEL(motionTransformBuffer, "motionTransformBuffer");
  _MTL_DEF_SEL(motionTransformBufferOffset, "motionTransformBufferOffset");
  _MTL_DEF_SEL(motionTransformCount, "motionTransformCount");
  _MTL_DEF_SEL(
      moveTextureMappingsFromTexture_sourceSlice_sourceLevel_sourceOrigin_sourceSize_toTexture_destinationSlice_destinationLevel_destinationOrigin_,
      "moveTextureMappingsFromTexture:sourceSlice:sourceLevel:sourceOrigin:"
      "sourceSize:toTexture:destinationSlice:destinationLevel:"
      "destinationOrigin:");
  _MTL_DEF_SEL(mutability, "mutability");
  _MTL_DEF_SEL(name, "name");
  _MTL_DEF_SEL(newAccelerationStructureWithDescriptor_,
               "newAccelerationStructureWithDescriptor:");
  _MTL_DEF_SEL(newAccelerationStructureWithDescriptor_offset_,
               "newAccelerationStructureWithDescriptor:offset:");
  _MTL_DEF_SEL(newAccelerationStructureWithSize_,
               "newAccelerationStructureWithSize:");
  _MTL_DEF_SEL(newAccelerationStructureWithSize_offset_,
               "newAccelerationStructureWithSize:offset:");
  _MTL_DEF_SEL(newArgumentEncoderForBufferAtIndex_,
               "newArgumentEncoderForBufferAtIndex:");
  _MTL_DEF_SEL(newArgumentEncoderWithArguments_,
               "newArgumentEncoderWithArguments:");
  _MTL_DEF_SEL(newArgumentEncoderWithBufferBinding_,
               "newArgumentEncoderWithBufferBinding:");
  _MTL_DEF_SEL(newArgumentEncoderWithBufferIndex_,
               "newArgumentEncoderWithBufferIndex:");
  _MTL_DEF_SEL(newArgumentEncoderWithBufferIndex_reflection_,
               "newArgumentEncoderWithBufferIndex:reflection:");
  _MTL_DEF_SEL(newBinaryArchiveWithDescriptor_error_,
               "newBinaryArchiveWithDescriptor:error:");
  _MTL_DEF_SEL(newBufferWithBytes_length_options_,
               "newBufferWithBytes:length:options:");
  _MTL_DEF_SEL(newBufferWithBytesNoCopy_length_options_deallocator_,
               "newBufferWithBytesNoCopy:length:options:deallocator:");
  _MTL_DEF_SEL(newBufferWithLength_options_, "newBufferWithLength:options:");
  _MTL_DEF_SEL(newBufferWithLength_options_offset_,
               "newBufferWithLength:options:offset:");
  _MTL_DEF_SEL(newCaptureScopeWithCommandQueue_,
               "newCaptureScopeWithCommandQueue:");
  _MTL_DEF_SEL(newCaptureScopeWithDevice_, "newCaptureScopeWithDevice:");
  _MTL_DEF_SEL(newCommandQueue, "newCommandQueue");
  _MTL_DEF_SEL(newCommandQueueWithMaxCommandBufferCount_,
               "newCommandQueueWithMaxCommandBufferCount:");
  _MTL_DEF_SEL(newComputePipelineStateWithAdditionalBinaryFunctions_error_,
               "newComputePipelineStateWithAdditionalBinaryFunctions:error:");
  _MTL_DEF_SEL(
      newComputePipelineStateWithDescriptor_options_completionHandler_,
      "newComputePipelineStateWithDescriptor:options:completionHandler:");
  _MTL_DEF_SEL(
      newComputePipelineStateWithDescriptor_options_reflection_error_,
      "newComputePipelineStateWithDescriptor:options:reflection:error:");
  _MTL_DEF_SEL(newComputePipelineStateWithFunction_completionHandler_,
               "newComputePipelineStateWithFunction:completionHandler:");
  _MTL_DEF_SEL(newComputePipelineStateWithFunction_error_,
               "newComputePipelineStateWithFunction:error:");
  _MTL_DEF_SEL(
      newComputePipelineStateWithFunction_options_completionHandler_,
      "newComputePipelineStateWithFunction:options:completionHandler:");
  _MTL_DEF_SEL(newComputePipelineStateWithFunction_options_reflection_error_,
               "newComputePipelineStateWithFunction:options:reflection:error:");
  _MTL_DEF_SEL(newCounterSampleBufferWithDescriptor_error_,
               "newCounterSampleBufferWithDescriptor:error:");
  _MTL_DEF_SEL(newDefaultLibrary, "newDefaultLibrary");
  _MTL_DEF_SEL(newDefaultLibraryWithBundle_error_,
               "newDefaultLibraryWithBundle:error:");
  _MTL_DEF_SEL(newDepthStencilStateWithDescriptor_,
               "newDepthStencilStateWithDescriptor:");
  _MTL_DEF_SEL(newDynamicLibrary_error_, "newDynamicLibrary:error:");
  _MTL_DEF_SEL(newDynamicLibraryWithURL_error_,
               "newDynamicLibraryWithURL:error:");
  _MTL_DEF_SEL(newEvent, "newEvent");
  _MTL_DEF_SEL(newFence, "newFence");
  _MTL_DEF_SEL(newFunctionWithDescriptor_completionHandler_,
               "newFunctionWithDescriptor:completionHandler:");
  _MTL_DEF_SEL(newFunctionWithDescriptor_error_,
               "newFunctionWithDescriptor:error:");
  _MTL_DEF_SEL(newFunctionWithName_, "newFunctionWithName:");
  _MTL_DEF_SEL(newFunctionWithName_constantValues_completionHandler_,
               "newFunctionWithName:constantValues:completionHandler:");
  _MTL_DEF_SEL(newFunctionWithName_constantValues_error_,
               "newFunctionWithName:constantValues:error:");
  _MTL_DEF_SEL(newHeapWithDescriptor_, "newHeapWithDescriptor:");
  _MTL_DEF_SEL(newIOCommandQueueWithDescriptor_error_,
               "newIOCommandQueueWithDescriptor:error:");
  _MTL_DEF_SEL(newIOHandleWithURL_compressionMethod_error_,
               "newIOHandleWithURL:compressionMethod:error:");
  _MTL_DEF_SEL(newIOHandleWithURL_error_, "newIOHandleWithURL:error:");
  _MTL_DEF_SEL(
      newIndirectCommandBufferWithDescriptor_maxCommandCount_options_,
      "newIndirectCommandBufferWithDescriptor:maxCommandCount:options:");
  _MTL_DEF_SEL(newIntersectionFunctionTableWithDescriptor_,
               "newIntersectionFunctionTableWithDescriptor:");
  _MTL_DEF_SEL(newIntersectionFunctionTableWithDescriptor_stage_,
               "newIntersectionFunctionTableWithDescriptor:stage:");
  _MTL_DEF_SEL(newIntersectionFunctionWithDescriptor_completionHandler_,
               "newIntersectionFunctionWithDescriptor:completionHandler:");
  _MTL_DEF_SEL(newIntersectionFunctionWithDescriptor_error_,
               "newIntersectionFunctionWithDescriptor:error:");
  _MTL_DEF_SEL(newLibraryWithData_error_, "newLibraryWithData:error:");
  _MTL_DEF_SEL(newLibraryWithFile_error_, "newLibraryWithFile:error:");
  _MTL_DEF_SEL(newLibraryWithSource_options_completionHandler_,
               "newLibraryWithSource:options:completionHandler:");
  _MTL_DEF_SEL(newLibraryWithSource_options_error_,
               "newLibraryWithSource:options:error:");
  _MTL_DEF_SEL(newLibraryWithStitchedDescriptor_completionHandler_,
               "newLibraryWithStitchedDescriptor:completionHandler:");
  _MTL_DEF_SEL(newLibraryWithStitchedDescriptor_error_,
               "newLibraryWithStitchedDescriptor:error:");
  _MTL_DEF_SEL(newLibraryWithURL_error_, "newLibraryWithURL:error:");
  _MTL_DEF_SEL(newRasterizationRateMapWithDescriptor_,
               "newRasterizationRateMapWithDescriptor:");
  _MTL_DEF_SEL(newRemoteBufferViewForDevice_, "newRemoteBufferViewForDevice:");
  _MTL_DEF_SEL(newRemoteTextureViewForDevice_,
               "newRemoteTextureViewForDevice:");
  _MTL_DEF_SEL(newRenderPipelineStateWithAdditionalBinaryFunctions_error_,
               "newRenderPipelineStateWithAdditionalBinaryFunctions:error:");
  _MTL_DEF_SEL(newRenderPipelineStateWithDescriptor_completionHandler_,
               "newRenderPipelineStateWithDescriptor:completionHandler:");
  _MTL_DEF_SEL(newRenderPipelineStateWithDescriptor_error_,
               "newRenderPipelineStateWithDescriptor:error:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithDescriptor_options_completionHandler_,
      "newRenderPipelineStateWithDescriptor:options:completionHandler:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithDescriptor_options_reflection_error_,
      "newRenderPipelineStateWithDescriptor:options:reflection:error:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithMeshDescriptor_options_completionHandler_,
      "newRenderPipelineStateWithMeshDescriptor:options:completionHandler:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithMeshDescriptor_options_reflection_error_,
      "newRenderPipelineStateWithMeshDescriptor:options:reflection:error:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithTileDescriptor_options_completionHandler_,
      "newRenderPipelineStateWithTileDescriptor:options:completionHandler:");
  _MTL_DEF_SEL(
      newRenderPipelineStateWithTileDescriptor_options_reflection_error_,
      "newRenderPipelineStateWithTileDescriptor:options:reflection:error:");
  _MTL_DEF_SEL(newSamplerStateWithDescriptor_,
               "newSamplerStateWithDescriptor:");
  _MTL_DEF_SEL(newScratchBufferWithMinimumSize_,
               "newScratchBufferWithMinimumSize:");
  _MTL_DEF_SEL(newSharedEvent, "newSharedEvent");
  _MTL_DEF_SEL(newSharedEventHandle, "newSharedEventHandle");
  _MTL_DEF_SEL(newSharedEventWithHandle_, "newSharedEventWithHandle:");
  _MTL_DEF_SEL(newSharedTextureHandle, "newSharedTextureHandle");
  _MTL_DEF_SEL(newSharedTextureWithDescriptor_,
               "newSharedTextureWithDescriptor:");
  _MTL_DEF_SEL(newSharedTextureWithHandle_, "newSharedTextureWithHandle:");
  _MTL_DEF_SEL(newTextureViewWithPixelFormat_,
               "newTextureViewWithPixelFormat:");
  _MTL_DEF_SEL(newTextureViewWithPixelFormat_textureType_levels_slices_,
               "newTextureViewWithPixelFormat:textureType:levels:slices:");
  _MTL_DEF_SEL(
      newTextureViewWithPixelFormat_textureType_levels_slices_swizzle_,
      "newTextureViewWithPixelFormat:textureType:levels:slices:swizzle:");
  _MTL_DEF_SEL(newTextureWithDescriptor_, "newTextureWithDescriptor:");
  _MTL_DEF_SEL(newTextureWithDescriptor_iosurface_plane_,
               "newTextureWithDescriptor:iosurface:plane:");
  _MTL_DEF_SEL(newTextureWithDescriptor_offset_,
               "newTextureWithDescriptor:offset:");
  _MTL_DEF_SEL(newTextureWithDescriptor_offset_bytesPerRow_,
               "newTextureWithDescriptor:offset:bytesPerRow:");
  _MTL_DEF_SEL(newVisibleFunctionTableWithDescriptor_,
               "newVisibleFunctionTableWithDescriptor:");
  _MTL_DEF_SEL(newVisibleFunctionTableWithDescriptor_stage_,
               "newVisibleFunctionTableWithDescriptor:stage:");
  _MTL_DEF_SEL(nodes, "nodes");
  _MTL_DEF_SEL(normalizedCoordinates, "normalizedCoordinates");
  _MTL_DEF_SEL(notifyListener_atValue_block_, "notifyListener:atValue:block:");
  _MTL_DEF_SEL(objectAtIndexedSubscript_, "objectAtIndexedSubscript:");
  _MTL_DEF_SEL(objectBindings, "objectBindings");
  _MTL_DEF_SEL(objectBuffers, "objectBuffers");
  _MTL_DEF_SEL(objectFunction, "objectFunction");
  _MTL_DEF_SEL(objectPayloadAlignment, "objectPayloadAlignment");
  _MTL_DEF_SEL(objectPayloadDataSize, "objectPayloadDataSize");
  _MTL_DEF_SEL(objectThreadExecutionWidth, "objectThreadExecutionWidth");
  _MTL_DEF_SEL(objectThreadgroupSizeIsMultipleOfThreadExecutionWidth,
               "objectThreadgroupSizeIsMultipleOfThreadExecutionWidth");
  _MTL_DEF_SEL(offset, "offset");
  _MTL_DEF_SEL(opaque, "opaque");
  _MTL_DEF_SEL(optimizationLevel, "optimizationLevel");
  _MTL_DEF_SEL(optimizeContentsForCPUAccess_, "optimizeContentsForCPUAccess:");
  _MTL_DEF_SEL(optimizeContentsForCPUAccess_slice_level_,
               "optimizeContentsForCPUAccess:slice:level:");
  _MTL_DEF_SEL(optimizeContentsForGPUAccess_, "optimizeContentsForGPUAccess:");
  _MTL_DEF_SEL(optimizeContentsForGPUAccess_slice_level_,
               "optimizeContentsForGPUAccess:slice:level:");
  _MTL_DEF_SEL(optimizeIndirectCommandBuffer_withRange_,
               "optimizeIndirectCommandBuffer:withRange:");
  _MTL_DEF_SEL(options, "options");
  _MTL_DEF_SEL(outputNode, "outputNode");
  _MTL_DEF_SEL(outputURL, "outputURL");
  _MTL_DEF_SEL(parallelRenderCommandEncoderWithDescriptor_,
               "parallelRenderCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(parameterBufferSizeAndAlign, "parameterBufferSizeAndAlign");
  _MTL_DEF_SEL(parentRelativeLevel, "parentRelativeLevel");
  _MTL_DEF_SEL(parentRelativeSlice, "parentRelativeSlice");
  _MTL_DEF_SEL(parentTexture, "parentTexture");
  _MTL_DEF_SEL(patchControlPointCount, "patchControlPointCount");
  _MTL_DEF_SEL(patchType, "patchType");
  _MTL_DEF_SEL(payloadMemoryLength, "payloadMemoryLength");
  _MTL_DEF_SEL(peerCount, "peerCount");
  _MTL_DEF_SEL(peerGroupID, "peerGroupID");
  _MTL_DEF_SEL(peerIndex, "peerIndex");
  _MTL_DEF_SEL(physicalGranularity, "physicalGranularity");
  _MTL_DEF_SEL(physicalSizeForLayer_, "physicalSizeForLayer:");
  _MTL_DEF_SEL(pixelFormat, "pixelFormat");
  _MTL_DEF_SEL(pointerType, "pointerType");
  _MTL_DEF_SEL(popDebugGroup, "popDebugGroup");
  _MTL_DEF_SEL(preloadedLibraries, "preloadedLibraries");
  _MTL_DEF_SEL(preprocessorMacros, "preprocessorMacros");
  _MTL_DEF_SEL(present, "present");
  _MTL_DEF_SEL(presentAfterMinimumDuration_, "presentAfterMinimumDuration:");
  _MTL_DEF_SEL(presentAtTime_, "presentAtTime:");
  _MTL_DEF_SEL(presentDrawable_, "presentDrawable:");
  _MTL_DEF_SEL(presentDrawable_afterMinimumDuration_,
               "presentDrawable:afterMinimumDuration:");
  _MTL_DEF_SEL(presentDrawable_atTime_, "presentDrawable:atTime:");
  _MTL_DEF_SEL(presentedTime, "presentedTime");
  _MTL_DEF_SEL(preserveInvariance, "preserveInvariance");
  _MTL_DEF_SEL(primitiveDataBuffer, "primitiveDataBuffer");
  _MTL_DEF_SEL(primitiveDataBufferOffset, "primitiveDataBufferOffset");
  _MTL_DEF_SEL(primitiveDataElementSize, "primitiveDataElementSize");
  _MTL_DEF_SEL(primitiveDataStride, "primitiveDataStride");
  _MTL_DEF_SEL(priority, "priority");
  _MTL_DEF_SEL(privateFunctions, "privateFunctions");
  _MTL_DEF_SEL(pushDebugGroup_, "pushDebugGroup:");
  _MTL_DEF_SEL(rAddressMode, "rAddressMode");
  _MTL_DEF_SEL(rasterSampleCount, "rasterSampleCount");
  _MTL_DEF_SEL(rasterizationRateMap, "rasterizationRateMap");
  _MTL_DEF_SEL(rasterizationRateMapDescriptorWithScreenSize_,
               "rasterizationRateMapDescriptorWithScreenSize:");
  _MTL_DEF_SEL(rasterizationRateMapDescriptorWithScreenSize_layer_,
               "rasterizationRateMapDescriptorWithScreenSize:layer:");
  _MTL_DEF_SEL(
      rasterizationRateMapDescriptorWithScreenSize_layerCount_layers_,
      "rasterizationRateMapDescriptorWithScreenSize:layerCount:layers:");
  _MTL_DEF_SEL(readMask, "readMask");
  _MTL_DEF_SEL(readWriteTextureSupport, "readWriteTextureSupport");
  _MTL_DEF_SEL(recommendedMaxWorkingSetSize, "recommendedMaxWorkingSetSize");
  _MTL_DEF_SEL(
      refitAccelerationStructure_descriptor_destination_scratchBuffer_scratchBufferOffset_,
      "refitAccelerationStructure:descriptor:destination:scratchBuffer:"
      "scratchBufferOffset:");
  _MTL_DEF_SEL(
      refitAccelerationStructure_descriptor_destination_scratchBuffer_scratchBufferOffset_options_,
      "refitAccelerationStructure:descriptor:destination:scratchBuffer:"
      "scratchBufferOffset:options:");
  _MTL_DEF_SEL(registryID, "registryID");
  _MTL_DEF_SEL(remoteStorageBuffer, "remoteStorageBuffer");
  _MTL_DEF_SEL(remoteStorageTexture, "remoteStorageTexture");
  _MTL_DEF_SEL(removeAllDebugMarkers, "removeAllDebugMarkers");
  _MTL_DEF_SEL(renderCommandEncoder, "renderCommandEncoder");
  _MTL_DEF_SEL(renderCommandEncoderWithDescriptor_,
               "renderCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(renderPassDescriptor, "renderPassDescriptor");
  _MTL_DEF_SEL(renderTargetArrayLength, "renderTargetArrayLength");
  _MTL_DEF_SEL(renderTargetHeight, "renderTargetHeight");
  _MTL_DEF_SEL(renderTargetWidth, "renderTargetWidth");
  _MTL_DEF_SEL(
      replaceRegion_mipmapLevel_slice_withBytes_bytesPerRow_bytesPerImage_,
      "replaceRegion:mipmapLevel:slice:withBytes:bytesPerRow:bytesPerImage:");
  _MTL_DEF_SEL(replaceRegion_mipmapLevel_withBytes_bytesPerRow_,
               "replaceRegion:mipmapLevel:withBytes:bytesPerRow:");
  _MTL_DEF_SEL(required, "required");
  _MTL_DEF_SEL(reset, "reset");
  _MTL_DEF_SEL(resetCommandsInBuffer_withRange_,
               "resetCommandsInBuffer:withRange:");
  _MTL_DEF_SEL(resetTextureAccessCounters_region_mipLevel_slice_,
               "resetTextureAccessCounters:region:mipLevel:slice:");
  _MTL_DEF_SEL(resetWithRange_, "resetWithRange:");
  _MTL_DEF_SEL(resolveCounterRange_, "resolveCounterRange:");
  _MTL_DEF_SEL(resolveCounters_inRange_destinationBuffer_destinationOffset_,
               "resolveCounters:inRange:destinationBuffer:destinationOffset:");
  _MTL_DEF_SEL(resolveDepthPlane, "resolveDepthPlane");
  _MTL_DEF_SEL(resolveLevel, "resolveLevel");
  _MTL_DEF_SEL(resolveSlice, "resolveSlice");
  _MTL_DEF_SEL(resolveTexture, "resolveTexture");
  _MTL_DEF_SEL(resourceOptions, "resourceOptions");
  _MTL_DEF_SEL(resourceStateCommandEncoder, "resourceStateCommandEncoder");
  _MTL_DEF_SEL(resourceStateCommandEncoderWithDescriptor_,
               "resourceStateCommandEncoderWithDescriptor:");
  _MTL_DEF_SEL(resourceStatePassDescriptor, "resourceStatePassDescriptor");
  _MTL_DEF_SEL(retainedReferences, "retainedReferences");
  _MTL_DEF_SEL(rgbBlendOperation, "rgbBlendOperation");
  _MTL_DEF_SEL(rootResource, "rootResource");
  _MTL_DEF_SEL(sAddressMode, "sAddressMode");
  _MTL_DEF_SEL(sampleBuffer, "sampleBuffer");
  _MTL_DEF_SEL(sampleBufferAttachments, "sampleBufferAttachments");
  _MTL_DEF_SEL(sampleCount, "sampleCount");
  _MTL_DEF_SEL(sampleCountersInBuffer_atSampleIndex_withBarrier_,
               "sampleCountersInBuffer:atSampleIndex:withBarrier:");
  _MTL_DEF_SEL(sampleTimestamps_gpuTimestamp_,
               "sampleTimestamps:gpuTimestamp:");
  _MTL_DEF_SEL(scratchBufferAllocator, "scratchBufferAllocator");
  _MTL_DEF_SEL(screenSize, "screenSize");
  _MTL_DEF_SEL(serializeToURL_error_, "serializeToURL:error:");
  _MTL_DEF_SEL(setAccelerationStructure_atBufferIndex_,
               "setAccelerationStructure:atBufferIndex:");
  _MTL_DEF_SEL(setAccelerationStructure_atIndex_,
               "setAccelerationStructure:atIndex:");
  _MTL_DEF_SEL(setAccess_, "setAccess:");
  _MTL_DEF_SEL(setAllowDuplicateIntersectionFunctionInvocation_,
               "setAllowDuplicateIntersectionFunctionInvocation:");
  _MTL_DEF_SEL(setAllowGPUOptimizedContents_, "setAllowGPUOptimizedContents:");
  _MTL_DEF_SEL(setAllowReferencingUndefinedSymbols_,
               "setAllowReferencingUndefinedSymbols:");
  _MTL_DEF_SEL(setAlphaBlendOperation_, "setAlphaBlendOperation:");
  _MTL_DEF_SEL(setAlphaToCoverageEnabled_, "setAlphaToCoverageEnabled:");
  _MTL_DEF_SEL(setAlphaToOneEnabled_, "setAlphaToOneEnabled:");
  _MTL_DEF_SEL(setArgumentBuffer_offset_, "setArgumentBuffer:offset:");
  _MTL_DEF_SEL(setArgumentBuffer_startOffset_arrayElement_,
               "setArgumentBuffer:startOffset:arrayElement:");
  _MTL_DEF_SEL(setArgumentIndex_, "setArgumentIndex:");
  _MTL_DEF_SEL(setArguments_, "setArguments:");
  _MTL_DEF_SEL(setArrayLength_, "setArrayLength:");
  _MTL_DEF_SEL(setAttributes_, "setAttributes:");
  _MTL_DEF_SEL(setBackFaceStencil_, "setBackFaceStencil:");
  _MTL_DEF_SEL(setBarrier, "setBarrier");
  _MTL_DEF_SEL(setBinaryArchives_, "setBinaryArchives:");
  _MTL_DEF_SEL(setBinaryFunctions_, "setBinaryFunctions:");
  _MTL_DEF_SEL(setBlendColorRed_green_blue_alpha_,
               "setBlendColorRed:green:blue:alpha:");
  _MTL_DEF_SEL(setBlendingEnabled_, "setBlendingEnabled:");
  _MTL_DEF_SEL(setBorderColor_, "setBorderColor:");
  _MTL_DEF_SEL(setBoundingBoxBuffer_, "setBoundingBoxBuffer:");
  _MTL_DEF_SEL(setBoundingBoxBufferOffset_, "setBoundingBoxBufferOffset:");
  _MTL_DEF_SEL(setBoundingBoxBuffers_, "setBoundingBoxBuffers:");
  _MTL_DEF_SEL(setBoundingBoxCount_, "setBoundingBoxCount:");
  _MTL_DEF_SEL(setBoundingBoxStride_, "setBoundingBoxStride:");
  _MTL_DEF_SEL(setBuffer_, "setBuffer:");
  _MTL_DEF_SEL(setBuffer_offset_atIndex_, "setBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setBufferIndex_, "setBufferIndex:");
  _MTL_DEF_SEL(setBufferOffset_atIndex_, "setBufferOffset:atIndex:");
  _MTL_DEF_SEL(setBuffers_offsets_withRange_, "setBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setBytes_length_atIndex_, "setBytes:length:atIndex:");
  _MTL_DEF_SEL(setCaptureObject_, "setCaptureObject:");
  _MTL_DEF_SEL(setClearColor_, "setClearColor:");
  _MTL_DEF_SEL(setClearDepth_, "setClearDepth:");
  _MTL_DEF_SEL(setClearStencil_, "setClearStencil:");
  _MTL_DEF_SEL(setColorStoreAction_atIndex_, "setColorStoreAction:atIndex:");
  _MTL_DEF_SEL(setColorStoreActionOptions_atIndex_,
               "setColorStoreActionOptions:atIndex:");
  _MTL_DEF_SEL(setCommandTypes_, "setCommandTypes:");
  _MTL_DEF_SEL(setCompareFunction_, "setCompareFunction:");
  _MTL_DEF_SEL(setCompileSymbolVisibility_, "setCompileSymbolVisibility:");
  _MTL_DEF_SEL(setCompressionType_, "setCompressionType:");
  _MTL_DEF_SEL(setComputeFunction_, "setComputeFunction:");
  _MTL_DEF_SEL(setComputePipelineState_, "setComputePipelineState:");
  _MTL_DEF_SEL(setComputePipelineState_atIndex_,
               "setComputePipelineState:atIndex:");
  _MTL_DEF_SEL(setComputePipelineStates_withRange_,
               "setComputePipelineStates:withRange:");
  _MTL_DEF_SEL(setConstantBlockAlignment_, "setConstantBlockAlignment:");
  _MTL_DEF_SEL(setConstantValue_type_atIndex_,
               "setConstantValue:type:atIndex:");
  _MTL_DEF_SEL(setConstantValue_type_withName_,
               "setConstantValue:type:withName:");
  _MTL_DEF_SEL(setConstantValues_, "setConstantValues:");
  _MTL_DEF_SEL(setConstantValues_type_withRange_,
               "setConstantValues:type:withRange:");
  _MTL_DEF_SEL(setControlDependencies_, "setControlDependencies:");
  _MTL_DEF_SEL(setCounterSet_, "setCounterSet:");
  _MTL_DEF_SEL(setCpuCacheMode_, "setCpuCacheMode:");
  _MTL_DEF_SEL(setCullMode_, "setCullMode:");
  _MTL_DEF_SEL(setDataType_, "setDataType:");
  _MTL_DEF_SEL(setDefaultCaptureScope_, "setDefaultCaptureScope:");
  _MTL_DEF_SEL(setDefaultRasterSampleCount_, "setDefaultRasterSampleCount:");
  _MTL_DEF_SEL(setDepth_, "setDepth:");
  _MTL_DEF_SEL(setDepthAttachment_, "setDepthAttachment:");
  _MTL_DEF_SEL(setDepthAttachmentPixelFormat_,
               "setDepthAttachmentPixelFormat:");
  _MTL_DEF_SEL(setDepthBias_slopeScale_clamp_,
               "setDepthBias:slopeScale:clamp:");
  _MTL_DEF_SEL(setDepthClipMode_, "setDepthClipMode:");
  _MTL_DEF_SEL(setDepthCompareFunction_, "setDepthCompareFunction:");
  _MTL_DEF_SEL(setDepthFailureOperation_, "setDepthFailureOperation:");
  _MTL_DEF_SEL(setDepthPlane_, "setDepthPlane:");
  _MTL_DEF_SEL(setDepthResolveFilter_, "setDepthResolveFilter:");
  _MTL_DEF_SEL(setDepthStencilPassOperation_, "setDepthStencilPassOperation:");
  _MTL_DEF_SEL(setDepthStencilState_, "setDepthStencilState:");
  _MTL_DEF_SEL(setDepthStoreAction_, "setDepthStoreAction:");
  _MTL_DEF_SEL(setDepthStoreActionOptions_, "setDepthStoreActionOptions:");
  _MTL_DEF_SEL(setDepthWriteEnabled_, "setDepthWriteEnabled:");
  _MTL_DEF_SEL(setDestination_, "setDestination:");
  _MTL_DEF_SEL(setDestinationAlphaBlendFactor_,
               "setDestinationAlphaBlendFactor:");
  _MTL_DEF_SEL(setDestinationRGBBlendFactor_, "setDestinationRGBBlendFactor:");
  _MTL_DEF_SEL(setDispatchType_, "setDispatchType:");
  _MTL_DEF_SEL(setEndOfEncoderSampleIndex_, "setEndOfEncoderSampleIndex:");
  _MTL_DEF_SEL(setEndOfFragmentSampleIndex_, "setEndOfFragmentSampleIndex:");
  _MTL_DEF_SEL(setEndOfVertexSampleIndex_, "setEndOfVertexSampleIndex:");
  _MTL_DEF_SEL(setErrorOptions_, "setErrorOptions:");
  _MTL_DEF_SEL(setFastMathEnabled_, "setFastMathEnabled:");
  _MTL_DEF_SEL(setFormat_, "setFormat:");
  _MTL_DEF_SEL(setFragmentAccelerationStructure_atBufferIndex_,
               "setFragmentAccelerationStructure:atBufferIndex:");
  _MTL_DEF_SEL(setFragmentAdditionalBinaryFunctions_,
               "setFragmentAdditionalBinaryFunctions:");
  _MTL_DEF_SEL(setFragmentBuffer_offset_atIndex_,
               "setFragmentBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setFragmentBufferOffset_atIndex_,
               "setFragmentBufferOffset:atIndex:");
  _MTL_DEF_SEL(setFragmentBuffers_offsets_withRange_,
               "setFragmentBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setFragmentBytes_length_atIndex_,
               "setFragmentBytes:length:atIndex:");
  _MTL_DEF_SEL(setFragmentFunction_, "setFragmentFunction:");
  _MTL_DEF_SEL(setFragmentIntersectionFunctionTable_atBufferIndex_,
               "setFragmentIntersectionFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setFragmentIntersectionFunctionTables_withBufferRange_,
               "setFragmentIntersectionFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setFragmentLinkedFunctions_, "setFragmentLinkedFunctions:");
  _MTL_DEF_SEL(setFragmentPreloadedLibraries_,
               "setFragmentPreloadedLibraries:");
  _MTL_DEF_SEL(setFragmentSamplerState_atIndex_,
               "setFragmentSamplerState:atIndex:");
  _MTL_DEF_SEL(setFragmentSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setFragmentSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setFragmentSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setFragmentSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setFragmentSamplerStates_withRange_,
               "setFragmentSamplerStates:withRange:");
  _MTL_DEF_SEL(setFragmentTexture_atIndex_, "setFragmentTexture:atIndex:");
  _MTL_DEF_SEL(setFragmentTextures_withRange_,
               "setFragmentTextures:withRange:");
  _MTL_DEF_SEL(setFragmentVisibleFunctionTable_atBufferIndex_,
               "setFragmentVisibleFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setFragmentVisibleFunctionTables_withBufferRange_,
               "setFragmentVisibleFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setFrontFaceStencil_, "setFrontFaceStencil:");
  _MTL_DEF_SEL(setFrontFacingWinding_, "setFrontFacingWinding:");
  _MTL_DEF_SEL(setFunction_atIndex_, "setFunction:atIndex:");
  _MTL_DEF_SEL(setFunctionCount_, "setFunctionCount:");
  _MTL_DEF_SEL(setFunctionGraphs_, "setFunctionGraphs:");
  _MTL_DEF_SEL(setFunctionName_, "setFunctionName:");
  _MTL_DEF_SEL(setFunctions_, "setFunctions:");
  _MTL_DEF_SEL(setFunctions_withRange_, "setFunctions:withRange:");
  _MTL_DEF_SEL(setGeometryDescriptors_, "setGeometryDescriptors:");
  _MTL_DEF_SEL(setGroups_, "setGroups:");
  _MTL_DEF_SEL(setHazardTrackingMode_, "setHazardTrackingMode:");
  _MTL_DEF_SEL(setHeight_, "setHeight:");
  _MTL_DEF_SEL(setImageblockSampleLength_, "setImageblockSampleLength:");
  _MTL_DEF_SEL(setImageblockWidth_height_, "setImageblockWidth:height:");
  _MTL_DEF_SEL(setIndex_, "setIndex:");
  _MTL_DEF_SEL(setIndexBuffer_, "setIndexBuffer:");
  _MTL_DEF_SEL(setIndexBufferIndex_, "setIndexBufferIndex:");
  _MTL_DEF_SEL(setIndexBufferOffset_, "setIndexBufferOffset:");
  _MTL_DEF_SEL(setIndexType_, "setIndexType:");
  _MTL_DEF_SEL(setIndirectCommandBuffer_atIndex_,
               "setIndirectCommandBuffer:atIndex:");
  _MTL_DEF_SEL(setIndirectCommandBuffers_withRange_,
               "setIndirectCommandBuffers:withRange:");
  _MTL_DEF_SEL(setInheritBuffers_, "setInheritBuffers:");
  _MTL_DEF_SEL(setInheritPipelineState_, "setInheritPipelineState:");
  _MTL_DEF_SEL(setInputPrimitiveTopology_, "setInputPrimitiveTopology:");
  _MTL_DEF_SEL(setInsertLibraries_, "setInsertLibraries:");
  _MTL_DEF_SEL(setInstallName_, "setInstallName:");
  _MTL_DEF_SEL(setInstanceCount_, "setInstanceCount:");
  _MTL_DEF_SEL(setInstanceDescriptorBuffer_, "setInstanceDescriptorBuffer:");
  _MTL_DEF_SEL(setInstanceDescriptorBufferOffset_,
               "setInstanceDescriptorBufferOffset:");
  _MTL_DEF_SEL(setInstanceDescriptorStride_, "setInstanceDescriptorStride:");
  _MTL_DEF_SEL(setInstanceDescriptorType_, "setInstanceDescriptorType:");
  _MTL_DEF_SEL(setInstancedAccelerationStructures_,
               "setInstancedAccelerationStructures:");
  _MTL_DEF_SEL(setIntersectionFunctionTable_atBufferIndex_,
               "setIntersectionFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setIntersectionFunctionTable_atIndex_,
               "setIntersectionFunctionTable:atIndex:");
  _MTL_DEF_SEL(setIntersectionFunctionTableOffset_,
               "setIntersectionFunctionTableOffset:");
  _MTL_DEF_SEL(setIntersectionFunctionTables_withBufferRange_,
               "setIntersectionFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setIntersectionFunctionTables_withRange_,
               "setIntersectionFunctionTables:withRange:");
  _MTL_DEF_SEL(setKernelBuffer_offset_atIndex_,
               "setKernelBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setLabel_, "setLabel:");
  _MTL_DEF_SEL(setLanguageVersion_, "setLanguageVersion:");
  _MTL_DEF_SEL(setLayer_atIndex_, "setLayer:atIndex:");
  _MTL_DEF_SEL(setLevel_, "setLevel:");
  _MTL_DEF_SEL(setLibraries_, "setLibraries:");
  _MTL_DEF_SEL(setLibraryType_, "setLibraryType:");
  _MTL_DEF_SEL(setLinkedFunctions_, "setLinkedFunctions:");
  _MTL_DEF_SEL(setLoadAction_, "setLoadAction:");
  _MTL_DEF_SEL(setLodAverage_, "setLodAverage:");
  _MTL_DEF_SEL(setLodMaxClamp_, "setLodMaxClamp:");
  _MTL_DEF_SEL(setLodMinClamp_, "setLodMinClamp:");
  _MTL_DEF_SEL(setMagFilter_, "setMagFilter:");
  _MTL_DEF_SEL(setMaxAnisotropy_, "setMaxAnisotropy:");
  _MTL_DEF_SEL(setMaxCallStackDepth_, "setMaxCallStackDepth:");
  _MTL_DEF_SEL(setMaxCommandBufferCount_, "setMaxCommandBufferCount:");
  _MTL_DEF_SEL(setMaxCommandsInFlight_, "setMaxCommandsInFlight:");
  _MTL_DEF_SEL(setMaxFragmentBufferBindCount_,
               "setMaxFragmentBufferBindCount:");
  _MTL_DEF_SEL(setMaxFragmentCallStackDepth_, "setMaxFragmentCallStackDepth:");
  _MTL_DEF_SEL(setMaxKernelBufferBindCount_, "setMaxKernelBufferBindCount:");
  _MTL_DEF_SEL(setMaxTessellationFactor_, "setMaxTessellationFactor:");
  _MTL_DEF_SEL(setMaxTotalThreadgroupsPerMeshGrid_,
               "setMaxTotalThreadgroupsPerMeshGrid:");
  _MTL_DEF_SEL(setMaxTotalThreadsPerMeshThreadgroup_,
               "setMaxTotalThreadsPerMeshThreadgroup:");
  _MTL_DEF_SEL(setMaxTotalThreadsPerObjectThreadgroup_,
               "setMaxTotalThreadsPerObjectThreadgroup:");
  _MTL_DEF_SEL(setMaxTotalThreadsPerThreadgroup_,
               "setMaxTotalThreadsPerThreadgroup:");
  _MTL_DEF_SEL(setMaxVertexAmplificationCount_,
               "setMaxVertexAmplificationCount:");
  _MTL_DEF_SEL(setMaxVertexBufferBindCount_, "setMaxVertexBufferBindCount:");
  _MTL_DEF_SEL(setMaxVertexCallStackDepth_, "setMaxVertexCallStackDepth:");
  _MTL_DEF_SEL(setMeshBuffer_offset_atIndex_, "setMeshBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setMeshBufferOffset_atIndex_, "setMeshBufferOffset:atIndex:");
  _MTL_DEF_SEL(setMeshBuffers_offsets_withRange_,
               "setMeshBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setMeshBytes_length_atIndex_, "setMeshBytes:length:atIndex:");
  _MTL_DEF_SEL(setMeshFunction_, "setMeshFunction:");
  _MTL_DEF_SEL(setMeshSamplerState_atIndex_, "setMeshSamplerState:atIndex:");
  _MTL_DEF_SEL(setMeshSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setMeshSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setMeshSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setMeshSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setMeshSamplerStates_withRange_,
               "setMeshSamplerStates:withRange:");
  _MTL_DEF_SEL(setMeshTexture_atIndex_, "setMeshTexture:atIndex:");
  _MTL_DEF_SEL(setMeshTextures_withRange_, "setMeshTextures:withRange:");
  _MTL_DEF_SEL(setMeshThreadgroupSizeIsMultipleOfThreadExecutionWidth_,
               "setMeshThreadgroupSizeIsMultipleOfThreadExecutionWidth:");
  _MTL_DEF_SEL(setMinFilter_, "setMinFilter:");
  _MTL_DEF_SEL(setMipFilter_, "setMipFilter:");
  _MTL_DEF_SEL(setMipmapLevelCount_, "setMipmapLevelCount:");
  _MTL_DEF_SEL(setMotionEndBorderMode_, "setMotionEndBorderMode:");
  _MTL_DEF_SEL(setMotionEndTime_, "setMotionEndTime:");
  _MTL_DEF_SEL(setMotionKeyframeCount_, "setMotionKeyframeCount:");
  _MTL_DEF_SEL(setMotionStartBorderMode_, "setMotionStartBorderMode:");
  _MTL_DEF_SEL(setMotionStartTime_, "setMotionStartTime:");
  _MTL_DEF_SEL(setMotionTransformBuffer_, "setMotionTransformBuffer:");
  _MTL_DEF_SEL(setMotionTransformBufferOffset_,
               "setMotionTransformBufferOffset:");
  _MTL_DEF_SEL(setMotionTransformCount_, "setMotionTransformCount:");
  _MTL_DEF_SEL(setMutability_, "setMutability:");
  _MTL_DEF_SEL(setName_, "setName:");
  _MTL_DEF_SEL(setNodes_, "setNodes:");
  _MTL_DEF_SEL(setNormalizedCoordinates_, "setNormalizedCoordinates:");
  _MTL_DEF_SEL(setObject_atIndexedSubscript_, "setObject:atIndexedSubscript:");
  _MTL_DEF_SEL(setObjectBuffer_offset_atIndex_,
               "setObjectBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setObjectBufferOffset_atIndex_,
               "setObjectBufferOffset:atIndex:");
  _MTL_DEF_SEL(setObjectBuffers_offsets_withRange_,
               "setObjectBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setObjectBytes_length_atIndex_,
               "setObjectBytes:length:atIndex:");
  _MTL_DEF_SEL(setObjectFunction_, "setObjectFunction:");
  _MTL_DEF_SEL(setObjectSamplerState_atIndex_,
               "setObjectSamplerState:atIndex:");
  _MTL_DEF_SEL(setObjectSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setObjectSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setObjectSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setObjectSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setObjectSamplerStates_withRange_,
               "setObjectSamplerStates:withRange:");
  _MTL_DEF_SEL(setObjectTexture_atIndex_, "setObjectTexture:atIndex:");
  _MTL_DEF_SEL(setObjectTextures_withRange_, "setObjectTextures:withRange:");
  _MTL_DEF_SEL(setObjectThreadgroupMemoryLength_atIndex_,
               "setObjectThreadgroupMemoryLength:atIndex:");
  _MTL_DEF_SEL(setObjectThreadgroupSizeIsMultipleOfThreadExecutionWidth_,
               "setObjectThreadgroupSizeIsMultipleOfThreadExecutionWidth:");
  _MTL_DEF_SEL(setOffset_, "setOffset:");
  _MTL_DEF_SEL(setOpaque_, "setOpaque:");
  _MTL_DEF_SEL(setOpaqueTriangleIntersectionFunctionWithSignature_atIndex_,
               "setOpaqueTriangleIntersectionFunctionWithSignature:atIndex:");
  _MTL_DEF_SEL(setOpaqueTriangleIntersectionFunctionWithSignature_withRange_,
               "setOpaqueTriangleIntersectionFunctionWithSignature:withRange:");
  _MTL_DEF_SEL(setOptimizationLevel_, "setOptimizationLevel:");
  _MTL_DEF_SEL(setOptions_, "setOptions:");
  _MTL_DEF_SEL(setOutputNode_, "setOutputNode:");
  _MTL_DEF_SEL(setOutputURL_, "setOutputURL:");
  _MTL_DEF_SEL(setPayloadMemoryLength_, "setPayloadMemoryLength:");
  _MTL_DEF_SEL(setPixelFormat_, "setPixelFormat:");
  _MTL_DEF_SEL(setPreloadedLibraries_, "setPreloadedLibraries:");
  _MTL_DEF_SEL(setPreprocessorMacros_, "setPreprocessorMacros:");
  _MTL_DEF_SEL(setPreserveInvariance_, "setPreserveInvariance:");
  _MTL_DEF_SEL(setPrimitiveDataBuffer_, "setPrimitiveDataBuffer:");
  _MTL_DEF_SEL(setPrimitiveDataBufferOffset_, "setPrimitiveDataBufferOffset:");
  _MTL_DEF_SEL(setPrimitiveDataElementSize_, "setPrimitiveDataElementSize:");
  _MTL_DEF_SEL(setPrimitiveDataStride_, "setPrimitiveDataStride:");
  _MTL_DEF_SEL(setPriority_, "setPriority:");
  _MTL_DEF_SEL(setPrivateFunctions_, "setPrivateFunctions:");
  _MTL_DEF_SEL(setPurgeableState_, "setPurgeableState:");
  _MTL_DEF_SEL(setRAddressMode_, "setRAddressMode:");
  _MTL_DEF_SEL(setRasterSampleCount_, "setRasterSampleCount:");
  _MTL_DEF_SEL(setRasterizationEnabled_, "setRasterizationEnabled:");
  _MTL_DEF_SEL(setRasterizationRateMap_, "setRasterizationRateMap:");
  _MTL_DEF_SEL(setReadMask_, "setReadMask:");
  _MTL_DEF_SEL(setRenderPipelineState_, "setRenderPipelineState:");
  _MTL_DEF_SEL(setRenderPipelineState_atIndex_,
               "setRenderPipelineState:atIndex:");
  _MTL_DEF_SEL(setRenderPipelineStates_withRange_,
               "setRenderPipelineStates:withRange:");
  _MTL_DEF_SEL(setRenderTargetArrayLength_, "setRenderTargetArrayLength:");
  _MTL_DEF_SEL(setRenderTargetHeight_, "setRenderTargetHeight:");
  _MTL_DEF_SEL(setRenderTargetWidth_, "setRenderTargetWidth:");
  _MTL_DEF_SEL(setResolveDepthPlane_, "setResolveDepthPlane:");
  _MTL_DEF_SEL(setResolveLevel_, "setResolveLevel:");
  _MTL_DEF_SEL(setResolveSlice_, "setResolveSlice:");
  _MTL_DEF_SEL(setResolveTexture_, "setResolveTexture:");
  _MTL_DEF_SEL(setResourceOptions_, "setResourceOptions:");
  _MTL_DEF_SEL(setRetainedReferences_, "setRetainedReferences:");
  _MTL_DEF_SEL(setRgbBlendOperation_, "setRgbBlendOperation:");
  _MTL_DEF_SEL(setSAddressMode_, "setSAddressMode:");
  _MTL_DEF_SEL(setSampleBuffer_, "setSampleBuffer:");
  _MTL_DEF_SEL(setSampleCount_, "setSampleCount:");
  _MTL_DEF_SEL(setSamplePositions_count_, "setSamplePositions:count:");
  _MTL_DEF_SEL(setSamplerState_atIndex_, "setSamplerState:atIndex:");
  _MTL_DEF_SEL(setSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setSamplerStates_withRange_, "setSamplerStates:withRange:");
  _MTL_DEF_SEL(setScissorRect_, "setScissorRect:");
  _MTL_DEF_SEL(setScissorRects_count_, "setScissorRects:count:");
  _MTL_DEF_SEL(setScratchBufferAllocator_, "setScratchBufferAllocator:");
  _MTL_DEF_SEL(setScreenSize_, "setScreenSize:");
  _MTL_DEF_SEL(setShouldMaximizeConcurrentCompilation_,
               "setShouldMaximizeConcurrentCompilation:");
  _MTL_DEF_SEL(setSignaledValue_, "setSignaledValue:");
  _MTL_DEF_SEL(setSize_, "setSize:");
  _MTL_DEF_SEL(setSlice_, "setSlice:");
  _MTL_DEF_SEL(setSourceAlphaBlendFactor_, "setSourceAlphaBlendFactor:");
  _MTL_DEF_SEL(setSourceRGBBlendFactor_, "setSourceRGBBlendFactor:");
  _MTL_DEF_SEL(setSparsePageSize_, "setSparsePageSize:");
  _MTL_DEF_SEL(setSpecializedName_, "setSpecializedName:");
  _MTL_DEF_SEL(setStageInRegion_, "setStageInRegion:");
  _MTL_DEF_SEL(setStageInRegionWithIndirectBuffer_indirectBufferOffset_,
               "setStageInRegionWithIndirectBuffer:indirectBufferOffset:");
  _MTL_DEF_SEL(setStageInputDescriptor_, "setStageInputDescriptor:");
  _MTL_DEF_SEL(setStartOfEncoderSampleIndex_, "setStartOfEncoderSampleIndex:");
  _MTL_DEF_SEL(setStartOfFragmentSampleIndex_,
               "setStartOfFragmentSampleIndex:");
  _MTL_DEF_SEL(setStartOfVertexSampleIndex_, "setStartOfVertexSampleIndex:");
  _MTL_DEF_SEL(setStencilAttachment_, "setStencilAttachment:");
  _MTL_DEF_SEL(setStencilAttachmentPixelFormat_,
               "setStencilAttachmentPixelFormat:");
  _MTL_DEF_SEL(setStencilCompareFunction_, "setStencilCompareFunction:");
  _MTL_DEF_SEL(setStencilFailureOperation_, "setStencilFailureOperation:");
  _MTL_DEF_SEL(setStencilFrontReferenceValue_backReferenceValue_,
               "setStencilFrontReferenceValue:backReferenceValue:");
  _MTL_DEF_SEL(setStencilReferenceValue_, "setStencilReferenceValue:");
  _MTL_DEF_SEL(setStencilResolveFilter_, "setStencilResolveFilter:");
  _MTL_DEF_SEL(setStencilStoreAction_, "setStencilStoreAction:");
  _MTL_DEF_SEL(setStencilStoreActionOptions_, "setStencilStoreActionOptions:");
  _MTL_DEF_SEL(setStepFunction_, "setStepFunction:");
  _MTL_DEF_SEL(setStepRate_, "setStepRate:");
  _MTL_DEF_SEL(setStorageMode_, "setStorageMode:");
  _MTL_DEF_SEL(setStoreAction_, "setStoreAction:");
  _MTL_DEF_SEL(setStoreActionOptions_, "setStoreActionOptions:");
  _MTL_DEF_SEL(setStride_, "setStride:");
  _MTL_DEF_SEL(setSupportAddingBinaryFunctions_,
               "setSupportAddingBinaryFunctions:");
  _MTL_DEF_SEL(setSupportAddingFragmentBinaryFunctions_,
               "setSupportAddingFragmentBinaryFunctions:");
  _MTL_DEF_SEL(setSupportAddingVertexBinaryFunctions_,
               "setSupportAddingVertexBinaryFunctions:");
  _MTL_DEF_SEL(setSupportArgumentBuffers_, "setSupportArgumentBuffers:");
  _MTL_DEF_SEL(setSupportIndirectCommandBuffers_,
               "setSupportIndirectCommandBuffers:");
  _MTL_DEF_SEL(setSupportRayTracing_, "setSupportRayTracing:");
  _MTL_DEF_SEL(setSwizzle_, "setSwizzle:");
  _MTL_DEF_SEL(setTAddressMode_, "setTAddressMode:");
  _MTL_DEF_SEL(setTessellationControlPointIndexType_,
               "setTessellationControlPointIndexType:");
  _MTL_DEF_SEL(setTessellationFactorBuffer_offset_instanceStride_,
               "setTessellationFactorBuffer:offset:instanceStride:");
  _MTL_DEF_SEL(setTessellationFactorFormat_, "setTessellationFactorFormat:");
  _MTL_DEF_SEL(setTessellationFactorScale_, "setTessellationFactorScale:");
  _MTL_DEF_SEL(setTessellationFactorScaleEnabled_,
               "setTessellationFactorScaleEnabled:");
  _MTL_DEF_SEL(setTessellationFactorStepFunction_,
               "setTessellationFactorStepFunction:");
  _MTL_DEF_SEL(setTessellationOutputWindingOrder_,
               "setTessellationOutputWindingOrder:");
  _MTL_DEF_SEL(setTessellationPartitionMode_, "setTessellationPartitionMode:");
  _MTL_DEF_SEL(setTexture_, "setTexture:");
  _MTL_DEF_SEL(setTexture_atIndex_, "setTexture:atIndex:");
  _MTL_DEF_SEL(setTextureType_, "setTextureType:");
  _MTL_DEF_SEL(setTextures_withRange_, "setTextures:withRange:");
  _MTL_DEF_SEL(setThreadGroupSizeIsMultipleOfThreadExecutionWidth_,
               "setThreadGroupSizeIsMultipleOfThreadExecutionWidth:");
  _MTL_DEF_SEL(setThreadgroupMemoryLength_, "setThreadgroupMemoryLength:");
  _MTL_DEF_SEL(setThreadgroupMemoryLength_atIndex_,
               "setThreadgroupMemoryLength:atIndex:");
  _MTL_DEF_SEL(setThreadgroupMemoryLength_offset_atIndex_,
               "setThreadgroupMemoryLength:offset:atIndex:");
  _MTL_DEF_SEL(setThreadgroupSizeMatchesTileSize_,
               "setThreadgroupSizeMatchesTileSize:");
  _MTL_DEF_SEL(setTileAccelerationStructure_atBufferIndex_,
               "setTileAccelerationStructure:atBufferIndex:");
  _MTL_DEF_SEL(setTileAdditionalBinaryFunctions_,
               "setTileAdditionalBinaryFunctions:");
  _MTL_DEF_SEL(setTileBuffer_offset_atIndex_, "setTileBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setTileBufferOffset_atIndex_, "setTileBufferOffset:atIndex:");
  _MTL_DEF_SEL(setTileBuffers_offsets_withRange_,
               "setTileBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setTileBytes_length_atIndex_, "setTileBytes:length:atIndex:");
  _MTL_DEF_SEL(setTileFunction_, "setTileFunction:");
  _MTL_DEF_SEL(setTileHeight_, "setTileHeight:");
  _MTL_DEF_SEL(setTileIntersectionFunctionTable_atBufferIndex_,
               "setTileIntersectionFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setTileIntersectionFunctionTables_withBufferRange_,
               "setTileIntersectionFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setTileSamplerState_atIndex_, "setTileSamplerState:atIndex:");
  _MTL_DEF_SEL(setTileSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setTileSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setTileSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setTileSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setTileSamplerStates_withRange_,
               "setTileSamplerStates:withRange:");
  _MTL_DEF_SEL(setTileTexture_atIndex_, "setTileTexture:atIndex:");
  _MTL_DEF_SEL(setTileTextures_withRange_, "setTileTextures:withRange:");
  _MTL_DEF_SEL(setTileVisibleFunctionTable_atBufferIndex_,
               "setTileVisibleFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setTileVisibleFunctionTables_withBufferRange_,
               "setTileVisibleFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setTileWidth_, "setTileWidth:");
  _MTL_DEF_SEL(setTransformationMatrixBuffer_,
               "setTransformationMatrixBuffer:");
  _MTL_DEF_SEL(setTransformationMatrixBufferOffset_,
               "setTransformationMatrixBufferOffset:");
  _MTL_DEF_SEL(setTriangleCount_, "setTriangleCount:");
  _MTL_DEF_SEL(setTriangleFillMode_, "setTriangleFillMode:");
  _MTL_DEF_SEL(setType_, "setType:");
  _MTL_DEF_SEL(setUrl_, "setUrl:");
  _MTL_DEF_SEL(setUsage_, "setUsage:");
  _MTL_DEF_SEL(setVertexAccelerationStructure_atBufferIndex_,
               "setVertexAccelerationStructure:atBufferIndex:");
  _MTL_DEF_SEL(setVertexAdditionalBinaryFunctions_,
               "setVertexAdditionalBinaryFunctions:");
  _MTL_DEF_SEL(setVertexAmplificationCount_viewMappings_,
               "setVertexAmplificationCount:viewMappings:");
  _MTL_DEF_SEL(setVertexBuffer_, "setVertexBuffer:");
  _MTL_DEF_SEL(setVertexBuffer_offset_atIndex_,
               "setVertexBuffer:offset:atIndex:");
  _MTL_DEF_SEL(setVertexBufferOffset_, "setVertexBufferOffset:");
  _MTL_DEF_SEL(setVertexBufferOffset_atIndex_,
               "setVertexBufferOffset:atIndex:");
  _MTL_DEF_SEL(setVertexBuffers_, "setVertexBuffers:");
  _MTL_DEF_SEL(setVertexBuffers_offsets_withRange_,
               "setVertexBuffers:offsets:withRange:");
  _MTL_DEF_SEL(setVertexBytes_length_atIndex_,
               "setVertexBytes:length:atIndex:");
  _MTL_DEF_SEL(setVertexDescriptor_, "setVertexDescriptor:");
  _MTL_DEF_SEL(setVertexFormat_, "setVertexFormat:");
  _MTL_DEF_SEL(setVertexFunction_, "setVertexFunction:");
  _MTL_DEF_SEL(setVertexIntersectionFunctionTable_atBufferIndex_,
               "setVertexIntersectionFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setVertexIntersectionFunctionTables_withBufferRange_,
               "setVertexIntersectionFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setVertexLinkedFunctions_, "setVertexLinkedFunctions:");
  _MTL_DEF_SEL(setVertexPreloadedLibraries_, "setVertexPreloadedLibraries:");
  _MTL_DEF_SEL(setVertexSamplerState_atIndex_,
               "setVertexSamplerState:atIndex:");
  _MTL_DEF_SEL(setVertexSamplerState_lodMinClamp_lodMaxClamp_atIndex_,
               "setVertexSamplerState:lodMinClamp:lodMaxClamp:atIndex:");
  _MTL_DEF_SEL(setVertexSamplerStates_lodMinClamps_lodMaxClamps_withRange_,
               "setVertexSamplerStates:lodMinClamps:lodMaxClamps:withRange:");
  _MTL_DEF_SEL(setVertexSamplerStates_withRange_,
               "setVertexSamplerStates:withRange:");
  _MTL_DEF_SEL(setVertexStride_, "setVertexStride:");
  _MTL_DEF_SEL(setVertexTexture_atIndex_, "setVertexTexture:atIndex:");
  _MTL_DEF_SEL(setVertexTextures_withRange_, "setVertexTextures:withRange:");
  _MTL_DEF_SEL(setVertexVisibleFunctionTable_atBufferIndex_,
               "setVertexVisibleFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setVertexVisibleFunctionTables_withBufferRange_,
               "setVertexVisibleFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setViewport_, "setViewport:");
  _MTL_DEF_SEL(setViewports_count_, "setViewports:count:");
  _MTL_DEF_SEL(setVisibilityResultBuffer_, "setVisibilityResultBuffer:");
  _MTL_DEF_SEL(setVisibilityResultMode_offset_,
               "setVisibilityResultMode:offset:");
  _MTL_DEF_SEL(setVisibleFunctionTable_atBufferIndex_,
               "setVisibleFunctionTable:atBufferIndex:");
  _MTL_DEF_SEL(setVisibleFunctionTable_atIndex_,
               "setVisibleFunctionTable:atIndex:");
  _MTL_DEF_SEL(setVisibleFunctionTables_withBufferRange_,
               "setVisibleFunctionTables:withBufferRange:");
  _MTL_DEF_SEL(setVisibleFunctionTables_withRange_,
               "setVisibleFunctionTables:withRange:");
  _MTL_DEF_SEL(setWidth_, "setWidth:");
  _MTL_DEF_SEL(setWriteMask_, "setWriteMask:");
  _MTL_DEF_SEL(sharedCaptureManager, "sharedCaptureManager");
  _MTL_DEF_SEL(shouldMaximizeConcurrentCompilation,
               "shouldMaximizeConcurrentCompilation");
  _MTL_DEF_SEL(signalEvent_value_, "signalEvent:value:");
  _MTL_DEF_SEL(signaledValue, "signaledValue");
  _MTL_DEF_SEL(size, "size");
  _MTL_DEF_SEL(slice, "slice");
  _MTL_DEF_SEL(sourceAlphaBlendFactor, "sourceAlphaBlendFactor");
  _MTL_DEF_SEL(sourceRGBBlendFactor, "sourceRGBBlendFactor");
  _MTL_DEF_SEL(sparsePageSize, "sparsePageSize");
  _MTL_DEF_SEL(sparseTileSizeInBytes, "sparseTileSizeInBytes");
  _MTL_DEF_SEL(sparseTileSizeInBytesForSparsePageSize_,
               "sparseTileSizeInBytesForSparsePageSize:");
  _MTL_DEF_SEL(sparseTileSizeWithTextureType_pixelFormat_sampleCount_,
               "sparseTileSizeWithTextureType:pixelFormat:sampleCount:");
  _MTL_DEF_SEL(
      sparseTileSizeWithTextureType_pixelFormat_sampleCount_sparsePageSize_,
      "sparseTileSizeWithTextureType:pixelFormat:sampleCount:sparsePageSize:");
  _MTL_DEF_SEL(specializedName, "specializedName");
  _MTL_DEF_SEL(stageInputAttributes, "stageInputAttributes");
  _MTL_DEF_SEL(stageInputDescriptor, "stageInputDescriptor");
  _MTL_DEF_SEL(stageInputOutputDescriptor, "stageInputOutputDescriptor");
  _MTL_DEF_SEL(startCaptureWithCommandQueue_, "startCaptureWithCommandQueue:");
  _MTL_DEF_SEL(startCaptureWithDescriptor_error_,
               "startCaptureWithDescriptor:error:");
  _MTL_DEF_SEL(startCaptureWithDevice_, "startCaptureWithDevice:");
  _MTL_DEF_SEL(startCaptureWithScope_, "startCaptureWithScope:");
  _MTL_DEF_SEL(startOfEncoderSampleIndex, "startOfEncoderSampleIndex");
  _MTL_DEF_SEL(startOfFragmentSampleIndex, "startOfFragmentSampleIndex");
  _MTL_DEF_SEL(startOfVertexSampleIndex, "startOfVertexSampleIndex");
  _MTL_DEF_SEL(staticThreadgroupMemoryLength, "staticThreadgroupMemoryLength");
  _MTL_DEF_SEL(status, "status");
  _MTL_DEF_SEL(stencilAttachment, "stencilAttachment");
  _MTL_DEF_SEL(stencilAttachmentPixelFormat, "stencilAttachmentPixelFormat");
  _MTL_DEF_SEL(stencilCompareFunction, "stencilCompareFunction");
  _MTL_DEF_SEL(stencilFailureOperation, "stencilFailureOperation");
  _MTL_DEF_SEL(stencilResolveFilter, "stencilResolveFilter");
  _MTL_DEF_SEL(stepFunction, "stepFunction");
  _MTL_DEF_SEL(stepRate, "stepRate");
  _MTL_DEF_SEL(stopCapture, "stopCapture");
  _MTL_DEF_SEL(storageMode, "storageMode");
  _MTL_DEF_SEL(storeAction, "storeAction");
  _MTL_DEF_SEL(storeActionOptions, "storeActionOptions");
  _MTL_DEF_SEL(stride, "stride");
  _MTL_DEF_SEL(structType, "structType");
  _MTL_DEF_SEL(supportAddingBinaryFunctions, "supportAddingBinaryFunctions");
  _MTL_DEF_SEL(supportAddingFragmentBinaryFunctions,
               "supportAddingFragmentBinaryFunctions");
  _MTL_DEF_SEL(supportAddingVertexBinaryFunctions,
               "supportAddingVertexBinaryFunctions");
  _MTL_DEF_SEL(supportArgumentBuffers, "supportArgumentBuffers");
  _MTL_DEF_SEL(supportIndirectCommandBuffers, "supportIndirectCommandBuffers");
  _MTL_DEF_SEL(supportRayTracing, "supportRayTracing");
  _MTL_DEF_SEL(supports32BitFloatFiltering, "supports32BitFloatFiltering");
  _MTL_DEF_SEL(supports32BitMSAA, "supports32BitMSAA");
  _MTL_DEF_SEL(supportsBCTextureCompression, "supportsBCTextureCompression");
  _MTL_DEF_SEL(supportsCounterSampling_, "supportsCounterSampling:");
  _MTL_DEF_SEL(supportsDestination_, "supportsDestination:");
  _MTL_DEF_SEL(supportsDynamicLibraries, "supportsDynamicLibraries");
  _MTL_DEF_SEL(supportsFamily_, "supportsFamily:");
  _MTL_DEF_SEL(supportsFeatureSet_, "supportsFeatureSet:");
  _MTL_DEF_SEL(supportsFunctionPointers, "supportsFunctionPointers");
  _MTL_DEF_SEL(supportsFunctionPointersFromRender,
               "supportsFunctionPointersFromRender");
  _MTL_DEF_SEL(supportsPrimitiveMotionBlur, "supportsPrimitiveMotionBlur");
  _MTL_DEF_SEL(supportsPullModelInterpolation,
               "supportsPullModelInterpolation");
  _MTL_DEF_SEL(supportsQueryTextureLOD, "supportsQueryTextureLOD");
  _MTL_DEF_SEL(supportsRasterizationRateMapWithLayerCount_,
               "supportsRasterizationRateMapWithLayerCount:");
  _MTL_DEF_SEL(supportsRaytracing, "supportsRaytracing");
  _MTL_DEF_SEL(supportsRaytracingFromRender, "supportsRaytracingFromRender");
  _MTL_DEF_SEL(supportsRenderDynamicLibraries,
               "supportsRenderDynamicLibraries");
  _MTL_DEF_SEL(supportsShaderBarycentricCoordinates,
               "supportsShaderBarycentricCoordinates");
  _MTL_DEF_SEL(supportsTextureSampleCount_, "supportsTextureSampleCount:");
  _MTL_DEF_SEL(supportsVertexAmplificationCount_,
               "supportsVertexAmplificationCount:");
  _MTL_DEF_SEL(swizzle, "swizzle");
  _MTL_DEF_SEL(synchronizeResource_, "synchronizeResource:");
  _MTL_DEF_SEL(synchronizeTexture_slice_level_,
               "synchronizeTexture:slice:level:");
  _MTL_DEF_SEL(tAddressMode, "tAddressMode");
  _MTL_DEF_SEL(tailSizeInBytes, "tailSizeInBytes");
  _MTL_DEF_SEL(tessellationControlPointIndexType,
               "tessellationControlPointIndexType");
  _MTL_DEF_SEL(tessellationFactorFormat, "tessellationFactorFormat");
  _MTL_DEF_SEL(tessellationFactorStepFunction,
               "tessellationFactorStepFunction");
  _MTL_DEF_SEL(tessellationOutputWindingOrder,
               "tessellationOutputWindingOrder");
  _MTL_DEF_SEL(tessellationPartitionMode, "tessellationPartitionMode");
  _MTL_DEF_SEL(texture, "texture");
  _MTL_DEF_SEL(texture2DDescriptorWithPixelFormat_width_height_mipmapped_,
               "texture2DDescriptorWithPixelFormat:width:height:mipmapped:");
  _MTL_DEF_SEL(textureBarrier, "textureBarrier");
  _MTL_DEF_SEL(
      textureBufferDescriptorWithPixelFormat_width_resourceOptions_usage_,
      "textureBufferDescriptorWithPixelFormat:width:resourceOptions:usage:");
  _MTL_DEF_SEL(textureCubeDescriptorWithPixelFormat_size_mipmapped_,
               "textureCubeDescriptorWithPixelFormat:size:mipmapped:");
  _MTL_DEF_SEL(textureDataType, "textureDataType");
  _MTL_DEF_SEL(textureReferenceType, "textureReferenceType");
  _MTL_DEF_SEL(textureType, "textureType");
  _MTL_DEF_SEL(threadExecutionWidth, "threadExecutionWidth");
  _MTL_DEF_SEL(threadGroupSizeIsMultipleOfThreadExecutionWidth,
               "threadGroupSizeIsMultipleOfThreadExecutionWidth");
  _MTL_DEF_SEL(threadgroupMemoryAlignment, "threadgroupMemoryAlignment");
  _MTL_DEF_SEL(threadgroupMemoryDataSize, "threadgroupMemoryDataSize");
  _MTL_DEF_SEL(threadgroupMemoryLength, "threadgroupMemoryLength");
  _MTL_DEF_SEL(threadgroupSizeMatchesTileSize,
               "threadgroupSizeMatchesTileSize");
  _MTL_DEF_SEL(tileAdditionalBinaryFunctions, "tileAdditionalBinaryFunctions");
  _MTL_DEF_SEL(tileArguments, "tileArguments");
  _MTL_DEF_SEL(tileBindings, "tileBindings");
  _MTL_DEF_SEL(tileBuffers, "tileBuffers");
  _MTL_DEF_SEL(tileFunction, "tileFunction");
  _MTL_DEF_SEL(tileHeight, "tileHeight");
  _MTL_DEF_SEL(tileWidth, "tileWidth");
  _MTL_DEF_SEL(transformationMatrixBuffer, "transformationMatrixBuffer");
  _MTL_DEF_SEL(transformationMatrixBufferOffset,
               "transformationMatrixBufferOffset");
  _MTL_DEF_SEL(triangleCount, "triangleCount");
  _MTL_DEF_SEL(tryCancel, "tryCancel");
  _MTL_DEF_SEL(type, "type");
  _MTL_DEF_SEL(updateFence_, "updateFence:");
  _MTL_DEF_SEL(updateFence_afterStages_, "updateFence:afterStages:");
  _MTL_DEF_SEL(
      updateTextureMapping_mode_indirectBuffer_indirectBufferOffset_,
      "updateTextureMapping:mode:indirectBuffer:indirectBufferOffset:");
  _MTL_DEF_SEL(updateTextureMapping_mode_region_mipLevel_slice_,
               "updateTextureMapping:mode:region:mipLevel:slice:");
  _MTL_DEF_SEL(
      updateTextureMappings_mode_regions_mipLevels_slices_numRegions_,
      "updateTextureMappings:mode:regions:mipLevels:slices:numRegions:");
  _MTL_DEF_SEL(url, "url");
  _MTL_DEF_SEL(usage, "usage");
  _MTL_DEF_SEL(useHeap_, "useHeap:");
  _MTL_DEF_SEL(useHeap_stages_, "useHeap:stages:");
  _MTL_DEF_SEL(useHeaps_count_, "useHeaps:count:");
  _MTL_DEF_SEL(useHeaps_count_stages_, "useHeaps:count:stages:");
  _MTL_DEF_SEL(useResource_usage_, "useResource:usage:");
  _MTL_DEF_SEL(useResource_usage_stages_, "useResource:usage:stages:");
  _MTL_DEF_SEL(useResources_count_usage_, "useResources:count:usage:");
  _MTL_DEF_SEL(useResources_count_usage_stages_,
               "useResources:count:usage:stages:");
  _MTL_DEF_SEL(usedSize, "usedSize");
  _MTL_DEF_SEL(vertexAdditionalBinaryFunctions,
               "vertexAdditionalBinaryFunctions");
  _MTL_DEF_SEL(vertexArguments, "vertexArguments");
  _MTL_DEF_SEL(vertexAttributes, "vertexAttributes");
  _MTL_DEF_SEL(vertexBindings, "vertexBindings");
  _MTL_DEF_SEL(vertexBuffer, "vertexBuffer");
  _MTL_DEF_SEL(vertexBufferOffset, "vertexBufferOffset");
  _MTL_DEF_SEL(vertexBuffers, "vertexBuffers");
  _MTL_DEF_SEL(vertexDescriptor, "vertexDescriptor");
  _MTL_DEF_SEL(vertexFormat, "vertexFormat");
  _MTL_DEF_SEL(vertexFunction, "vertexFunction");
  _MTL_DEF_SEL(vertexLinkedFunctions, "vertexLinkedFunctions");
  _MTL_DEF_SEL(vertexPreloadedLibraries, "vertexPreloadedLibraries");
  _MTL_DEF_SEL(vertexStride, "vertexStride");
  _MTL_DEF_SEL(vertical, "vertical");
  _MTL_DEF_SEL(verticalSampleStorage, "verticalSampleStorage");
  _MTL_DEF_SEL(visibilityResultBuffer, "visibilityResultBuffer");
  _MTL_DEF_SEL(visibleFunctionTableDescriptor,
               "visibleFunctionTableDescriptor");
  _MTL_DEF_SEL(waitForEvent_value_, "waitForEvent:value:");
  _MTL_DEF_SEL(waitForFence_, "waitForFence:");
  _MTL_DEF_SEL(waitForFence_beforeStages_, "waitForFence:beforeStages:");
  _MTL_DEF_SEL(waitUntilCompleted, "waitUntilCompleted");
  _MTL_DEF_SEL(waitUntilScheduled, "waitUntilScheduled");
  _MTL_DEF_SEL(width, "width");
  _MTL_DEF_SEL(writeCompactedAccelerationStructureSize_toBuffer_offset_,
               "writeCompactedAccelerationStructureSize:toBuffer:offset:");
  _MTL_DEF_SEL(
      writeCompactedAccelerationStructureSize_toBuffer_offset_sizeDataType_,
      "writeCompactedAccelerationStructureSize:toBuffer:offset:sizeDataType:");
  _MTL_DEF_SEL(writeMask, "writeMask");

  // constants
  _MTL_DEF_STR(NS::ErrorDomain, CounterErrorDomain);

  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterTimestamp);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterTessellationInputPatches);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterVertexInvocations);
  _MTL_DEF_STR(MTL::CommonCounter,
               CommonCounterPostTessellationVertexInvocations);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterClipperInvocations);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterClipperPrimitivesOut);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterFragmentInvocations);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterFragmentsPassed);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterComputeKernelInvocations);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterTotalCycles);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterVertexCycles);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterTessellationCycles);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterPostTessellationVertexCycles);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterFragmentCycles);
  _MTL_DEF_STR(MTL::CommonCounter, CommonCounterRenderTargetWriteCycles);

  _MTL_DEF_STR(MTL::CommonCounterSet, CommonCounterSetTimestamp);
  _MTL_DEF_STR(MTL::CommonCounterSet, CommonCounterSetStageUtilization);
  _MTL_DEF_STR(MTL::CommonCounterSet, CommonCounterSetStatistic);

  _MTL_DEF_WEAK_CONST(MTL::DeviceNotificationName, DeviceWasAddedNotification);
  _MTL_DEF_WEAK_CONST(MTL::DeviceNotificationName,
                      DeviceRemovalRequestedNotification);
  _MTL_DEF_WEAK_CONST(MTL::DeviceNotificationName,
                      DeviceWasRemovedNotification);
  _MTL_DEF_CONST(NS::ErrorUserInfoKey, CommandBufferEncoderInfoErrorKey);
}
// ---------

// extern "C" MTL::Device *MTLCreateSystemDefaultDevice();

// extern "C" NS::Array *MTLCopyAllDevices();

// extern "C" NS::Array *
// MTLCopyAllDevicesWithObserver(NS::Object **,
//                               MTL::DeviceNotificationHandlerBlock);

// extern "C" void MTLRemoveDeviceObserver(const NS::Object *);

// _NS_EXPORT MTL::Device *MTL::CreateSystemDefaultDevice() {
//   return ::MTLCreateSystemDefaultDevice();
// }

// _NS_EXPORT NS::Array *MTL::CopyAllDevices() { return ::MTLCopyAllDevices(); }

// _NS_EXPORT NS::Array *
// MTL::CopyAllDevicesWithObserver(NS::Object **pOutObserver,
//                                 DeviceNotificationHandlerBlock handler) {

//   return ::MTLCopyAllDevicesWithObserver(pOutObserver, handler);
// }

// _NS_EXPORT NS::Array *MTL::CopyAllDevicesWithObserver(
//     NS::Object **pOutObserver,
//     const DeviceNotificationHandlerFunction &handler) {
//   __block DeviceNotificationHandlerFunction function = handler;

//   return CopyAllDevicesWithObserver(
//       pOutObserver,
//       ^(Device *pDevice, DeviceNotificationName pNotificationName) {
//         function(pDevice, pNotificationName);
//       });
// }

// _NS_EXPORT void MTL::RemoveDeviceObserver(const NS::Object *pObserver) {

//   ::MTLRemoveDeviceObserver(pObserver);
// }

// ----------------

// namespace MTL::Private {

// MTL_DEF_FUNC(MTLIOCompressionContextDefaultChunkSize, size_t (*)(void));

// MTL_DEF_FUNC( MTLIOCreateCompressionContext, void* (*)(const char*,
// MTL::IOCompressionMethod, size_t) );

// MTL_DEF_FUNC( MTLIOCompressionContextAppendData, void (*)(void*, const void*,
// size_t) );

// MTL_DEF_FUNC( MTLIOFlushAndDestroyCompressionContext,
// MTL::IOCompressionStatus (*)(void*) );

// }

// _NS_EXPORT size_t MTL::IOCompressionContextDefaultChunkSize()
// {
//     return MTL::Private::MTLIOCompressionContextDefaultChunkSize();
// }

// _NS_EXPORT void* MTL::IOCreateCompressionContext(const char* path,
// IOCompressionMethod type, size_t chunkSize)
// {
//     if ( MTL::Private::MTLIOCreateCompressionContext )
//     {
//         return MTL::Private::MTLIOCreateCompressionContext( path, type,
//         chunkSize );
//     }
//     return nullptr;
// }

// _NS_EXPORT void MTL::IOCompressionContextAppendData(void* context, const
// void* data, size_t size)
// {
//     if ( MTL::Private::MTLIOCompressionContextAppendData )
//     {
//         MTL::Private::MTLIOCompressionContextAppendData( context, data, size
//         );
//     }
// }

// _NS_EXPORT MTL::IOCompressionStatus
// MTL::IOFlushAndDestroyCompressionContext(void* context)
// {
//     if ( MTL::Private::MTLIOFlushAndDestroyCompressionContext )
//     {
//         return MTL::Private::MTLIOFlushAndDestroyCompressionContext( context
//         );
//     }
//     return MTL::IOCompressionStatusError;
// }

extern "C" void __init_metalcpp() {
  __init_NS();
  __init_ca();
  __init_mtl();
}