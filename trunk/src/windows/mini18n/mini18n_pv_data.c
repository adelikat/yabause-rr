/*  Copyright 2008 Guillaume Duhamel
  
    This file is part of mini18n.
  
    mini18n is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
  
    mini18n is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
  
    You should have received a copy of the GNU General Public License
    along with mini18n; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mini18n_pv_data.h"
#include <string.h>
#define __USE_GNU
#include <wchar.h>

mini18n_data_t mini18n_str = {
	(mini18n_len_func) strlen,
	(mini18n_dup_func) strdup,
	(mini18n_cmp_func) strcmp
};

mini18n_data_t mini18n_wcs = {
	(mini18n_len_func) wcslen,
	(mini18n_dup_func) wcsdup,
	(mini18n_cmp_func) wcscmp
};
