/*
 *  Hamlib TCI backend — registration
 *  Copyright (c) 2026 by Jeff Francis N0GQ <gjfrancis@protonmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 */

#include "hamlib/config.h"
#include "hamlib/rig.h"
#include "register.h"

#include "tci2.h"

DECLARE_INITRIG_BACKEND(tci)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rig_register(&sunsdr2_pro_caps);

    return RIG_OK;
}
