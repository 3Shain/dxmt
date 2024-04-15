/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#pragma once

#include <ostream>
#include <unknwn.h>

namespace dxmt {

/**
 * \brief Checks whether an unknown GUID should be logged
 *
 * \param [in] objectGuid GUID of the object that QueryInterface is called on
 * \param [in] requestGuid Requested unsupported GUID
 * \returns \c true if the error should be logged
 */
bool logQueryInterfaceError(REFIID objectGuid, REFIID requestedGuid);

}; // namespace dxmt

std::ostream &operator<<(std::ostream &os, REFIID guid);
