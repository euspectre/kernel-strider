/* Some parts of this example are based on the source code of eu-addr2line
 * tool from elfutils covered by the following notice. 
 *
 * ========================================================================
 * Copyright (C) 2005-2010, 2012 Red Hat, Inc.
 * This file is part of elfutils.
 * Written by Ulrich Drepper <drepper@redhat.com>, 2005.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * elfutils is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ========================================================================
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>
#include <cstdlib>
#include <cstring>

#include <libelf.h>
#include <gelf.h>
#include <dwarf.h>
#include <elfutils/libdwfl.h>

using namespace std;

unsigned long offset = 0x424;

/* Start address of the section as if all loadable sections from the given 
 * file have been loaded with the base address of 0. Alignment should have 
 * been taken into account too but it is 1 anyway in our case. */
GElf_Addr section_start = 0x12951;
/*static const char *section = ".devinit.text";*/

/* 
unsigned long offset = 0x47;
GElf_Addr section_start = 0x128c3;
*/
/* static const char *section = ".init.text"; */

static Dwfl_Callbacks dwfl_callbacks;

/* It is not needed for libdw to search itself for the files with debug 
 * info. So, a stub is used instead of the default callback of this kind. */
static int
find_debuginfo(Dwfl_Module *mod __attribute__ ((unused)),
	       void **userdata __attribute__ ((unused)),
	       const char *modname __attribute__ ((unused)),
	       GElf_Addr base __attribute__ ((unused)),
	       const char *file_name __attribute__ ((unused)),
	       const char *debuglink_file __attribute__ ((unused)),
	       GElf_Word debuglink_crc __attribute__ ((unused)),
	       char **debuginfo_file_name __attribute__ ((unused)))
{
	return -1; /* as if found nothing */ 
}

/* .find_elf callback should not be called by libdw because we use 
 * dwfl_report_elf() to inform the library about the file with debug info.
 * The callback is still provided in case something in libdw expects it to 
 * be.  */
static int
find_elf(Dwfl_Module *mod __attribute__ ((unused)),
	 void **userdata __attribute__ ((unused)),
	 const char *modname __attribute__ ((unused)),
	 Dwarf_Addr base __attribute__ ((unused)),
	 char **file_name __attribute__ ((unused)), 
	 Elf **elfp __attribute__ ((unused)))
{
	return -1; /* as if found nothing */ 
}

static void
print_last_dwarf_error()
{
	cerr << "Error: " << dwfl_errmsg(-1) << endl;
}

/* Print name of the function an a position in it (file:line), similar to
 * a stack trace entry. */
static void
print_func(const char *name, const char *file, int line)
{
	/* basename() needs a string that can be changed. */
	char *src = strdup(file);
	if (src == NULL) {
		cerr << "Not enough memory.\n";
		return;
	}
	
	cout << "\t" << name << " (" << basename(src) << ":" << line
		<< ")\n";
	free(src);
}

/* Print information about the inline function corresponding to the given
 * scope in the given compilation unit ('cudie').
 * The location in the inline function is given in (*src_file, *src_line).
 * The function changes these two variables to the location where the
 * given inline is called ("substituted"). */
static bool
print_inline_info(Dwarf_Die *cudie, Dwarf_Die *die, const char **src_file,
		  int *src_line)
{
	Dwarf_Files *files;
	//size_t nfiles;
	const char *name = NULL;
	int ret;

	name = dwarf_diename(die);
	if (name == NULL) {
		cerr << "[DWARF] No function name.\n";
		return false;
	}

	print_func(name, *src_file, *src_line);
	
	ret = dwarf_getsrcfiles(cudie, &files, NULL);
	if (ret != 0) {
		cerr << "Failed to get the list of the source files.\n";
		return false;
	}

	//cout << "Found " << nfiles << " source file(s).\n";

	Dwarf_Attribute attr_mem;
	Dwarf_Word val;
	Dwarf_Attribute *at;

	at = dwarf_attr(die, DW_AT_call_file, &attr_mem);
	if (at == NULL) {
		cerr << "Failed to find attribute: DW_AT_call_file.\n";
		return false;
	}
	
	if (dwarf_formudata(at, &val) != 0) {
		cerr << "Failed to find the source file "
			"where the function has been inlined.\n";
		return false;
	}

	*src_file = dwarf_filesrc(files, val, NULL, NULL);
	if (*src_file == NULL) {
		cerr << "Failed to retrieve the name of the file.\n";
		return false;
	}

	at = dwarf_attr(die, DW_AT_call_line, &attr_mem);
	if (at == NULL) {
		cerr << "Failed to find attribute: DW_AT_call_line.\n";
		return false;
	}
	
	if (dwarf_formudata(at, &val) != 0) {
		cerr << "Failed to find the source line "
			"where the function has been inlined.\n";
		return false;
	}

	*src_line = val;
	return true;
}

/* Print the whole chain of inlined functions for a given DIE with
 * DW_TAG_inlined_subroutine tag. */
static bool
print_inline_info_full(Dwarf_Die *cudie, Dwarf_Die *die,
		       const char *src_file, int src_line)
{
	Dwarf_Die *scopes = NULL;
	int nscopes = dwarf_getscopes_die(die, &scopes);
	bool ret = false;
	const char *name = NULL;

	/* At least one scope (same as the DIE) should be found. */
	if (nscopes <= 0) {
		cerr << "Failed to find containing scopes for an inline: "
			<< dwfl_errmsg(-1) << "\n";
		free(scopes);
		return false;
	}

	//cout << "[DBG] nscopes: " << nscopes << "\n";

	for (int i = 0; i < nscopes; ++i) {
		switch (dwarf_tag(&scopes[i])) {
		case DW_TAG_subprogram:
			/* End of the chain of inline functions. */
			name = dwarf_diename(&scopes[i]);
			if (name == NULL) {
				cerr << "[DWARF] No function name.\n";
				ret = false;
				break;
			}

			print_func(name, src_file, src_line);
			break;

		case DW_TAG_inlined_subroutine:
			ret = print_inline_info(cudie, &scopes[i],
						&src_file, &src_line);
			break;
			
		default:
			break;
		}
	}
	
	free(scopes);
	return ret;
}

static bool
print_dwarf_function(Dwfl_Module *mod, Dwarf_Addr addr,
		     const char *src_file, int src_line)
{
	bool ret = false;
	Dwarf_Addr bias = 0;
	
	/* DIE for the compilation unit. */
	Dwarf_Die *cudie = dwfl_module_addrdie(mod, addr, &bias);
	
	//<>
	//cout << "bias: " << hex << bias << dec << "\n";
	//<>
	
	Dwarf_Die *scopes;
	int nscopes = dwarf_getscopes(cudie, addr - bias, &scopes);
	/*cout << "Number of scopes for the address in the CU: " << nscopes
		<< "\n";*/
	
	if (nscopes < 0) {
		print_last_dwarf_error();
		return false;
	}
	else if (nscopes == 0) {
		return false;
	}
	
	for (int i = 0; i < nscopes; ++i) {
		const char *name = NULL;
				
		switch (dwarf_tag(&scopes[i])) {
		case DW_TAG_subprogram:
			name = dwarf_diename(&scopes[i]);
			if (name == NULL) {
				cerr << "[DWARF] No function name.\n";
				break;
			}

			print_func(name, src_file, src_line);
			ret = true;
			break;

		case DW_TAG_inlined_subroutine:
			ret = print_inline_info_full(cudie, &scopes[i],
				src_file, src_line);
			break;

		default:
			break;
		}
	}
	
	free(scopes);
	return ret;
}

static bool
get_source_info(Dwfl_Module *mod, Dwarf_Addr addr)
{
	bool ret = true;

	/* Find the source file and line number. */
	const char *src = NULL;
	int src_line = 0;
	
	Dwfl_Line *line = dwfl_module_getsrc(mod, addr);
	/* [NB] dwfl_module_getsrc leaks 32 bytes per module on x86-64 when
	 * loading libebl_*.so via dlopen(). */

	if (line == NULL) {
		print_last_dwarf_error();
		return false;
	}
	
	int linecol;
	src = dwfl_lineinfo(line, &addr, &src_line, &linecol, NULL, NULL);

	if (src == NULL)
		return false;

	/* Find the function the address belongs to. DWARF information may
	 * be more detailed than what dwfl_module_addrname() returns, so
	 * try the former. */
	bool ok = print_dwarf_function(mod, addr, src, src_line);
	if (!ok) {
		cout << "Failed to obtain the detailed function info."
			<< endl;
		ret = false;
	}

	return ret;
}

static int 
do_process_file(int fd)
{
	/* dwfl_report_*() functions close the file descriptor passed there, 
	 * so make a duplicate first. */
	int dwfl_fd = dup(fd);
	if (dwfl_fd < 0)
		return 1;
	
	dwfl_callbacks.section_address = dwfl_offline_section_address;
	dwfl_callbacks.find_debuginfo = find_debuginfo;
	dwfl_callbacks.find_elf = find_elf;
	
	Dwfl *dwfl = dwfl_begin(&dwfl_callbacks);
	if (dwfl == NULL) {
		print_last_dwarf_error();
		cerr << "Failed to initialize DWARF-related facilities." <<
			endl;
		close(dwfl_fd);
		return 1;
	}
	
	Dwfl_Module *mod = dwfl_report_elf(dwfl, "e1000", "e1000.ko.debug", 
					   dwfl_fd, 0 /* base address */);
	if (mod == NULL) {
		print_last_dwarf_error();
		cerr << "Failed to load the file with debug info." << endl;
		close(dwfl_fd);
		dwfl_end(dwfl);
		return 1;
	}
	dwfl_report_end(dwfl, NULL, NULL);
	
	/* Let us find the data corresponding to the given address
	 * (specified as <section>+<offset>). The base address is assumed
	 * to be 0. */
	GElf_Addr addr = (GElf_Addr)offset + section_start;

	bool ok = get_source_info(mod, addr);
	if (!ok) {
		cerr << "Failed to obtain source information.\n";
	}
	
	/* [NB] Name of the function the address belongs to (another way to
	 * find it). */
	/*cout << "[dwfl] Function: " << dwfl_module_addrname(mod, addr) 
		<< endl;*/
	
	dwfl_end(dwfl);
	return 0;
}

int 
main(int argc, char *argv[])
{
	int fd;
	Elf *e;
	Elf_Kind ek;
	int ret = EXIT_SUCCESS;
	
	if (argc != 2) {
		cerr << "Usage: " << argv[0] << " <file_path>" << endl;
		exit(EXIT_FAILURE);
	}
	
		if (elf_version(EV_CURRENT) == EV_NONE) {
		cerr << "Failed to initialize libelf: " << elf_errmsg(-1) 
			<< endl;
	}
	
	errno = 0;
	char *mod_path = argv[1];
	fd = open(mod_path, O_RDONLY, 0);
	if (fd == -1) {
		cerr << "Failed to open \"" << mod_path << "\": " <<
			strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
	
	e = elf_begin(fd, ELF_C_READ, NULL);
	if (e == NULL) {
		cerr << "elf_begin() failed: " << elf_errmsg(-1) << endl;
		close(fd);
		exit(EXIT_FAILURE);
	}
	
	ek = elf_kind(e);
	switch (ek) {
	case ELF_K_ELF:
		if (do_process_file(fd) != 0)
			ret = EXIT_FAILURE;
		break;
	default:
		ret = EXIT_FAILURE;
		cerr << "The input file is not an ELF object file." << endl;
	}
		
	elf_end(e);
	close(fd);
	return ret;
}
