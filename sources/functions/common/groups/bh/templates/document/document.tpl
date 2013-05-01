/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 * Author: 
 *      Eugene A. Shatokhin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <linux/kernel.h>

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/fh_drd/common.h>

#include "config.h"
/* ====================================================================== */
<$if concat(header)$>
<$header: join(\n)$>
/* ====================================================================== */
<$endif$>
<$if concat(function.name)$>
<$block : join(\n\n)$>
/* ====================================================================== */
<$endif$>
