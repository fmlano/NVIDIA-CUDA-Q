/****************************************************************-*- C++ -*-****
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once
#include <string>

namespace cudaq {
namespace registry {
extern "C" {
void deviceCodeHolderAdd(const char *, const char *, bool);
void cudaqRegisterKernelName(const char *);
void cudaqRegisterArgsCreator(const char *, char *);
void cudaqRegisterLambdaName(const char *, const char *);
}

} // namespace registry

namespace __internal__ {
/// Is the kernel `kernelName` registered?
bool isKernelGenerated(const std::string &kernelName);
} // namespace __internal__
} // namespace cudaq
