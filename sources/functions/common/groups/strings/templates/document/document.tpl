/* ========================================================================
 * Copyright (C) 2012, KEDR development team
 * Authors: 
 *      Eugene A. Shatokhin <spectre@ispras.ru>
 *      Andrey V. Tsyvarev  <tsyvarev@ispras.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <kedr/kedr_mem/core_api.h>
#include <kedr/kedr_mem/local_storage.h>
#include <kedr/kedr_mem/functions.h>
#include <kedr/object_types.h>

<$if concat(header)$><$header: join(\n)$><$endif$>
/* ====================================================================== */
<$if concat(header_aux)$>
<$header_aux : join(\n)$>
/* ====================================================================== */
<$endif$>
<$if concat(function.name)$><$block : join(\n\n)$>
<$endif$>/* ====================================================================== */
