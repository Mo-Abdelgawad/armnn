//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "DynamicBackend.hpp"
#include "DynamicBackendUtils.hpp"

namespace armnn
{

DynamicBackend::DynamicBackend(const void* sharedObjectHandle)
    : m_BackendIdFunction(nullptr)
    , m_BackendVersionFunction(nullptr)
    , m_BackendFactoryFunction(nullptr)
    , m_Handle(const_cast<void*>(sharedObjectHandle), &DynamicBackendUtils::CloseHandle)
{
    if (m_Handle == nullptr)
    {
        throw InvalidArgumentException("Cannot create a DynamicBackend object from an invalid shared object handle");
    }

    // These calls will throw in case of error
    m_BackendIdFunction      = SetFunctionPointer<IdFunctionType>("GetBackendId");
    m_BackendVersionFunction = SetFunctionPointer<VersionFunctionType>("GetVersion");
    m_BackendFactoryFunction = SetFunctionPointer<FactoryFunctionType>("BackendFactory");

    // Check that the backend is compatible with the current Backend API
    BackendId backendId = GetBackendId();
    BackendVersion backendVersion = GetBackendVersion();
    if (!DynamicBackendUtils::IsBackendCompatible(backendVersion))
    {
        throw RuntimeException(boost::str(boost::format("The dynamic backend %1% (version %2%) is not compatible"
                                                        "with the current Backend API (vesion %3%)")
                                          % backendId
                                          % backendVersion
                                          % IBackendInternal::GetApiVersion()));
    }
}

BackendId DynamicBackend::GetBackendId()
{
    if (m_BackendIdFunction == nullptr)
    {
        throw RuntimeException("GetBackendId error: invalid function pointer");
    }

    return BackendId(m_BackendIdFunction());
}

BackendVersion DynamicBackend::GetBackendVersion()
{
    if (m_BackendVersionFunction == nullptr)
    {
        throw RuntimeException("GetBackendVersion error: invalid function pointer");
    }

    uint32_t major = 0;
    uint32_t minor = 0;
    m_BackendVersionFunction(&major, &minor);

    return BackendVersion{ major, minor };
}

IBackendInternalUniquePtr DynamicBackend::GetBackend()
{
    if (m_BackendFactoryFunction == nullptr)
    {
        throw RuntimeException("GetBackend error: invalid function pointer");
    }

    auto backendPointer = reinterpret_cast<IBackendInternal*>(m_BackendFactoryFunction());
    if (backendPointer == nullptr)
    {
        throw RuntimeException("GetBackend error: backend instance must not be null");
    }

    return std::unique_ptr<IBackendInternal>(backendPointer);
}

template<typename BackendFunctionType>
BackendFunctionType DynamicBackend::SetFunctionPointer(const std::string& backendFunctionName)
{
    if (m_Handle == nullptr)
    {
        throw RuntimeException("SetFunctionPointer error: invalid shared object handle");
    }

    if (backendFunctionName.empty())
    {
        throw RuntimeException("SetFunctionPointer error: backend function name must not be empty");
    }

    // This call will throw in case of error
    auto functionPointer = DynamicBackendUtils::GetEntryPoint<BackendFunctionType>(m_Handle.get(),
                                                                                   backendFunctionName.c_str());
    if (!functionPointer)
    {
        throw RuntimeException("SetFunctionPointer error: invalid backend function pointer returned");
    }

    return functionPointer;
}

} // namespace armnn