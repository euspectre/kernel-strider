/* module_info.cpp - implementation of the API to deal with the information
 * about the target kernel modules. */

/* ========================================================================
 * Copyright (C) 2013, ROSA Laboratory
 *
 * Author: 
 *      Eugene A. Shatokhin <eugene.shatokhin@rosalab.ru>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 ======================================================================== */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h> /* basename */

#include <cstdlib>
#include <cstring>

#include <cassert>

#include <libelf.h>
#include <gelf.h>
#include <dwarf.h>
#include <elfutils/libdwfl.h>

#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "module_info.h"
#include "rc_ptr.h"

using namespace std;
/* ====================================================================== */

/* The next effective address that can be assigned to a core area.
 * Should always be a multiple of 0x1000; cannot be 0. Each assigned
 * address should be greater than the previously assigned ones. */
static unsigned int addr_eff = 0x1000;
const unsigned int addr_eff_align = 0x1000;

typedef std::map<std::string, rc_ptr<ModuleInfo> > TModuleMap;
typedef std::map<unsigned int, rc_ptr<ModuleInfo> > TAddrMap;

/* {module name => module info} mapping. */
static TModuleMap module_map;

/* {real address of a code area => module info} mapping. */
static TAddrMap real_addr_map;

/* {effective address of a code area => module info} mapping. */
static TAddrMap eff_addr_map;
/* ====================================================================== */

/* NULL means DWARF debug info should not be used. */
static rc_ptr<DwflWrapper> dwfl; 
/* ====================================================================== */

//<>
static void 
print_section(const rc_ptr<SectionInfo> &si)
{
	cout << "[DBG]      " << si->name << " at " 
		<< (void *)(unsigned long)si->addr
		<< " , size is " << (void *)(unsigned long)si->size
		<< " , dw_addr is " << (void *)(unsigned long)si->dw_addr
		<< "\n";
}

void 
ModuleInfo::debug()
{
	cout << "[DBG] modules:\n\n";
	
	TModuleMap::const_iterator it;
	for (it = module_map.begin(); it != module_map.end(); ++it) {
		cout << "[DBG]   " << it->first << ": " 
			<< "eff. init at " 
			<< (void *)(unsigned long)it->second->init_ca.addr_eff
			<< ", eff. core at "
			<< (void *)(unsigned long)it->second->core_ca.addr_eff
			/*<< ", refcount is " << it->second.ref_count()*/
			<< ", the file is "
			<< it->second->path << "\n";
		cout << "[DBG]   Sections:\n";
		for_each(it->second->sections.begin(), 
			 it->second->sections.end(),
			 print_section);
		cout << "\n";
	}
	
	TAddrMap::const_iterator pos;
	
	cout << "[DBG] {real address => module}:\n";
	for (pos = real_addr_map.begin(); pos != real_addr_map.end(); ++pos)
	{
		cout << "[DBG]   " 
			<< (void *)(unsigned long)pos->first 
			<< ": " << pos->second->name << "\n";
	}
	
	cout << "[DBG] {effective address => module}:\n";
	for (pos = eff_addr_map.begin(); pos != eff_addr_map.end(); ++pos) {
		cout << "[DBG]   " 
			<< (void *)(unsigned long)pos->first 
			<< ": " << pos->second->name << "\n";
	}
}
//<>

/* Align 'value' to the multiple of 'align' and return the result.
 * 'align' must be a power of 2. */
static unsigned long
align_value(unsigned long value, unsigned long align)
{
	unsigned long mask = align - 1;
	return (value + mask) & ~mask;
}

/* Same as strstarts() from the kernel. */
static bool 
starts_with(const char *str, const char *prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static void
load_dwarf_info(rc_ptr<ModuleInfo> &mi, Elf * /* unused */, int fd)
{
	assert(!dwfl.isNULL());
	
	/* dwfl_report_*() functions close the file descriptor passed there 
	 * if successful, so make a duplicate first. */
	int dwfl_fd = dup(fd);
	if (dwfl_fd < 0) {
		throw ModuleInfo::Error(
			"Failed to duplicate a file descriptor.");
	}
	
	mi->dwfl_mod = dwfl_report_elf(
		dwfl->get_handle(), mi->name.c_str(), mi->path.c_str(), 
		dwfl_fd, 0 /* base address */);
	
	if (mi->dwfl_mod == NULL) {
		/* Not always an error but worth notifying the user. 
		 * Missing debug info, perhaps? */
		cerr << "No debug info is present in or can be loaded from "
			<< mi->path << ". " << dwfl_errmsg(-1) << endl;
		close(dwfl_fd);
		return;
	}
	
	dwfl_report_end(dwfl->get_handle(), NULL, NULL);
	
	/* Load the base addresses of the sections from the point of view 
	 * of libdw/libdwfl. */
	size_t sh_str_index;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	char *name;
	
	/* {section name => base address of the section in DWARF info} */
	map<string, unsigned int> dw_addr_map;

	GElf_Addr base_addr = 0;
	Elf *e = dwfl_module_getelf(mi->dwfl_mod, &base_addr);
	if (e == NULL) {
		ostringstream err;
		err << mi->name 
			<< ": failed to get ELF object for DWARF file: "
			<< elf_errmsg(-1);
		throw ModuleInfo::Error(err.str());
	}

	if (elf_getshdrstrndx(e, &sh_str_index) != 0) {
		ostringstream err;
		err << mi->name 
			<< ": elf_getshdrstrndx() failed: "
			<< elf_errmsg(-1);
		throw ModuleInfo::Error(err.str());
	}

	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			ostringstream err;
			err << mi->name 
				<< ": failed to retrieve section header: "
				<< elf_errmsg(-1);
			throw ModuleInfo::Error(err.str());
		}

		name = elf_strptr(e, sh_str_index, shdr.sh_name);
		if (name == NULL) {
			ostringstream err;
			err << mi->name 
				<< ": failed to retrieve section name: "
				<< elf_errmsg(-1);
			throw ModuleInfo::Error(err.str());
		}

		if ((shdr.sh_flags & SHF_ALLOC) != SHF_ALLOC)
			continue;

		dw_addr_map[string(name)] = shdr.sh_addr;
	}
	
	ModuleInfo::TSections::iterator it;
	for (it = mi->sections.begin(); it != mi->sections.end(); ++it) {
		rc_ptr<SectionInfo> si = *it;
		
		map<string, unsigned int>::const_iterator dw_it;
		dw_it = dw_addr_map.find(si->name);
		if (dw_it != dw_addr_map.end()) {
			si->dw_addr = dw_it->second;
		}
	}
}

/* Load names and sizes of the ELF sections.
 * 
 * In addition, the function checks if the sections with debug info are 
 * present. 
 * [NB] libdw/libdwfl leak memory if used on a module without debug info,
 * so it is better to try to determine in advance if debug info exists. */
static void
load_elf_info(rc_ptr<ModuleInfo> &mi, Elf *e, int /* unused */)
{
	size_t sh_str_index;
	Elf_Scn *scn;
	GElf_Shdr shdr;
	char *name;
	
	assert(mi->sections.empty());
	
	if (elf_getshdrstrndx(e, &sh_str_index) != 0) {
		ostringstream err;
		err << mi->name << ": elf_getshdrstrndx() failed: "
			<< elf_errmsg(-1);
		throw ModuleInfo::Error(err.str());
	}
	
	unsigned long mask = SHF_ALLOC | SHF_EXECINSTR;
	
	bool has_debug_info = false;
	bool has_debug_line = false;
	
	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) != &shdr) {
			ostringstream err;
			err << mi->name 
				<< ": failed to retrieve section header: "
				<< elf_errmsg(-1);
			throw ModuleInfo::Error(err.str());
		}
		
		name = elf_strptr(e, sh_str_index, shdr.sh_name);
		if (name == NULL) {
			ostringstream err;
			err << mi->name 
				<< ": failed to retrieve section name: "
				<< elf_errmsg(-1);
			throw ModuleInfo::Error(err.str());
		}
		
		/* .debug_info section - the core DWARF data containing 
		 * DIEs. */
		if (strcmp(name, ".debug_info") == 0)
			has_debug_info = true;
		
		/* .debug_line section - line number program (DWARF). */
		if (strcmp(name, ".debug_line") == 0)
			has_debug_line = true;
		
		if ((shdr.sh_flags & mask) != mask)
			continue;
		
		rc_ptr<SectionInfo> si(new SectionInfo(name));
		if (starts_with(name, ".init"))
			si->is_init = true;
		
		si->size = shdr.sh_size;
		si->align = shdr.sh_addralign;
		
		mi->sections.push_back(si);
	}
	
	if (has_debug_info && has_debug_line)
		mi->has_debug_info = true;
	
	if (mi->sections.empty()) {
		cerr << "Warning: \"" << mi->name 
			<< "\" seems to have no loadable code sections.";
		/* Not sure if such modules exist and if they are "legal" */
	}
}

static void
process_elf_file(rc_ptr<ModuleInfo> &mi, 
		 void (*func)(rc_ptr<ModuleInfo> &, Elf *, int))
{
	int fd;
	Elf *e;
	Elf_Kind ek;
	
	errno = 0;
	fd = open(mi->path.c_str(), O_RDONLY, 0);
	if (fd == -1) {
		ostringstream err;
		err << "Failed to open \"" << mi->path << "\": " <<
			strerror(errno) << endl;
		throw ModuleInfo::Error(err.str());
	}
	
	e = elf_begin(fd, ELF_C_READ, NULL);
	if (e == NULL) {
		close(fd);
		ostringstream err;
		err << "elf_begin() failed for " << mi->path << ": " <<
			elf_errmsg(-1) << endl;
		throw ModuleInfo::Error(err.str());
	}
	
	ek = elf_kind(e);
	if (ek != ELF_K_ELF) {
		elf_end(e);
		close(fd);
		throw ModuleInfo::Error(
			string("Not an ELF object file: ") + mi->path);
	}
	
	try {
		func(mi, e, fd);
	}
	catch (ModuleInfo::Error &err) {
		elf_end(e);
		close(fd);
		throw;
	}
			
	elf_end(e);
	close(fd);
}

static unsigned int
update_offset(rc_ptr<ModuleInfo> &mi, rc_ptr<SectionInfo> &si, 
	      unsigned int &current_pos)
{
	unsigned int off;
	
	if (current_pos == 0) {
		/* Weird, but may happen if the trace is corrupted. */
		ostringstream err;
		err << "\"" << mi->name << "\" has ELF section \""
			<< si->name << "\" but the trace states "
			<< "the corresponding code area is not present. "
			<< "Corrupted trace?";
		throw ModuleInfo::Error(err.str());
	}
	
	off = align_value(current_pos, (si->align ? si->align : 1));
	current_pos = off + si->size; 
	
	return off;
}

static void
set_section_addresses(rc_ptr<ModuleInfo> &mi)
{
	unsigned int init = mi->init_ca.addr_eff;
	unsigned int core = mi->core_ca.addr_eff;
	
	ModuleInfo::TSections::iterator it;
	for (it = mi->sections.begin(); it != mi->sections.end(); ++it) {
		rc_ptr<SectionInfo> &si = *it;
		si->addr = update_offset(
			mi, si, (si->is_init ? init : core));
	}
	
	stable_sort(mi->sections.begin(), mi->sections.end(), 
		    SectionInfo::Less());
}

void
ModuleInfo::add_module(const string &mod_path, const string &mod_dir)
{
	if (mod_path.empty()) {
		throw ModuleInfo::Error(
			"Path to the module file should not be empty.");
	}
	
	size_t beg = mod_path.find_last_of('/');
	if (beg == string::npos) {
		beg = 0;
	}
	else {
		++beg; 
		if (beg == mod_path.size()) {
			/* 'mod_path' ends with '/' */
			throw ModuleInfo::Error(
				string("Invalid module path: \"") + 
				mod_path + string("\"."));
		}
	}
	
	size_t pos_ko = mod_path.find(".ko", beg);
	size_t pos_debug = mod_path.find(".debug", beg);
	size_t pos = string::npos;
	
	if (pos_ko != string::npos) {
		if (pos_debug == string::npos)
			pos = pos_ko;
		else if (pos_debug > pos_ko)
			pos = pos_ko;
		else 
			pos = pos_debug;
	}
	else if (pos_debug != string::npos) {
		pos = pos_debug;
	}
	
	if (pos == string::npos) {
		throw ModuleInfo::Error(
			string("Invalid module path: \"") + 
			mod_path + string("\"."));
	}
	
	string name = mod_path.substr(beg, pos - beg);
	if (name.empty()) {
		throw ModuleInfo::Error(
			string("Invalid module path: \"") + 
			mod_path + string("\"."));
	}
	
	/* Within the kernel, all modules have dashes replaced with 
	 * underscores in their names. */
	for (size_t i = 0; i < name.size(); ++i) {
		if (name[i] == '-')
			name[i] = '_';
	}
	
	TModuleMap::const_iterator it;
	it = module_map.find(name);
	if (it != module_map.end()) {
		throw ModuleInfo::Error(
			string("Module \"") + name + 
			string("\" is specified at least twice."));
	}
	
	rc_ptr<ModuleInfo> mi(new ModuleInfo(name));
	mi->path = (mod_path[0] == '/' ? mod_path : (mod_dir + mod_path));
	
	process_elf_file(mi, load_elf_info);
	if (!dwfl.isNULL() && mi->has_debug_info)
		process_elf_file(mi, load_dwarf_info);

	module_map[name] = mi;
	return;
}

static void 
assign_effective_address(rc_ptr<ModuleInfo> &mi, ModuleInfo::CodeArea &ca)
{
	if (ca.size == 0)
		return; /* No such code area - nothing to do. */
	
	ca.addr_eff = addr_eff;
	addr_eff = align_value(addr_eff + ca.size, addr_eff_align);
	
	bool ok = eff_addr_map.insert(make_pair(ca.addr_eff, mi)).second;
	if (!ok) {
		/* A corrupted trace cannot lead to this, only errors in
		 * this application itself can. */
		cerr << "Internal error: "
			<< "unable to assign effective address." << endl;
		assert(false);
	}
}

static void
add_real_address(rc_ptr<ModuleInfo> &mi, unsigned int addr)
{
	if (addr == 0)
		return; /* no code area - nothing to do */
	
	bool ok;
	ok = real_addr_map.insert(make_pair(addr, mi)).second;
	if (!ok) {
		/* May happen if some "target unload" events were lost. */
		ostringstream err;
		err << "\"" << mi->name << "\": the address of a code area "
			<< "(" << (void *)(unsigned long)addr 
			<< ") seems to belong to another module. "
			<< "Corrupted or incomplete trace?";
		throw ModuleInfo::Error(err.str());
	}
}

static void
remove_real_address(rc_ptr<ModuleInfo> &mi, unsigned int addr)
{
	if (addr == 0)
		return; /* no code area - nothing to do */
	
	TAddrMap::iterator it;
	it = real_addr_map.find(addr);
	if (it == real_addr_map.end()) {
		cerr << "Internal error: address " 
			<< (void *)(unsigned long)addr
			<< " is missing from the map." << endl;
		assert(false);
	}
	
	string owner = it->second->name;
	if (owner != mi->name) {
		cerr << "Internal error: address "
			<< (void *)(unsigned long)addr
			<< " belongs to \"" << owner 
			<< "\" rather than to \"" << mi->name
			<< "\"." << endl;
		assert(false);
	}
	
	real_addr_map.erase(it);
}

void 
ModuleInfo::on_module_load(
	const std::string &name, 
	unsigned int init_addr, unsigned int init_size,
	unsigned int core_addr, unsigned int core_size)
{
	TModuleMap::const_iterator it;
	it = module_map.find(name);
	if (it == module_map.end()) {
		throw ModuleInfo::Error(
			string("Unknown module: \"") + name + string("\""));
	}
	
	rc_ptr<ModuleInfo> mi = it->second;
	
	if (mi->loaded) {
		ostringstream err;
		err << "Encountered two \"target load\" events for the " 
			<< "module \"" << name << "\" "
			<< "without a \"target unload\" event in between.";
		throw ModuleInfo::Error(err.str());
	}
	
	/* Sanity checks. The size of the code areas must remain the same 
	 * except when it becomes non-zero on the first "target load" 
	 * event. */
	if (mi->core_ca.size != 0 || mi->init_ca.size != 0) {
		if (core_size != mi->core_ca.size) {
			ostringstream err;
			err << name 
				<< ": size of \"core\" area changed from "
				<< mi->core_ca.size << " to " 
				<< core_size << ".";
			throw ModuleInfo::Error(err.str());
		}
		
		if (init_size != mi->init_ca.size) {
			ostringstream err;
			err << name 
				<< ": size of \"init\" area changed from "
				<< mi->init_ca.size << " to " 
				<< init_size << ".";
			throw ModuleInfo::Error(err.str());
		}
	}
	else {
		/* The module was loaded for the first time. */
		mi->core_ca.size = core_size; 
		mi->init_ca.size = init_size;
	}

	/* Either both or neither of the code areas must have the effective
	 * addresses assigned to them. */
	assert( (mi->core_ca.addr_eff == 0 && mi->init_ca.addr_eff == 0) || 
		(mi->core_ca.addr_eff != 0 && mi->init_ca.addr_eff != 0));
	
	/* If the module has not been assigned the effective addresses yet,
	 * do so now. */
	if (mi->core_ca.addr_eff == 0 && mi->init_ca.addr_eff == 0) {
		assign_effective_address(mi, mi->core_ca);
		assign_effective_address(mi, mi->init_ca);
		
		set_section_addresses(mi);
	}
	
	mi->core_ca.addr_real = core_addr;
	mi->init_ca.addr_real = init_addr;

	add_real_address(mi, core_addr);
	add_real_address(mi, init_addr);
	
	mi->loaded = true;
}
	
void 
ModuleInfo::on_module_unload(const std::string &name)
{
	TModuleMap::const_iterator it;
	it = module_map.find(name);
	if (it == module_map.end()) {
		throw ModuleInfo::Error(
			string("Unknown module: \"") + name + string("\""));
	}
	
	rc_ptr<ModuleInfo> mi = it->second;
	
	if (!mi->loaded) {
		ostringstream err;
		err << "Encountered \"target unload\" event for the " 
			<< "module \"" << name << "\" "
			<< "without a matching \"target load\" event.";
		throw ModuleInfo::Error(err.str());
	}
	
	remove_real_address(mi, mi->core_ca.addr_real);
	remove_real_address(mi, mi->init_ca.addr_real);
	
	mi->core_ca.addr_real = 0;
	mi->init_ca.addr_real = 0;
	
	mi->loaded = false;
}

unsigned int 
ModuleInfo::effective_address(unsigned int addr)
{
	if (real_addr_map.empty()) {
		ostringstream err;
		err << "According to the trace, no module was loaded "
			<< "when the event at the address "
			<< (void *)(unsigned long)addr
			<< " occurred ("
			<< "the map {real address => module} is empty"
			<< "). Corrupted or incomplete trace?";
		throw ModuleInfo::Error(err.str());
	}
	
	TAddrMap::const_iterator it = real_addr_map.upper_bound(addr);
	if (it == real_addr_map.begin()) {
		ostringstream err;
		err << "Failed to find the module the code address "
			<< (void *)(unsigned long)addr
			<< " belongs to.";
		throw ModuleInfo::Error(err.str());
	}
	
	--it;
	rc_ptr<ModuleInfo> mi = it->second;

	if (mi->core_ca.contains(addr)) {
		return mi->core_ca.effective_address(addr);
	}
	else if (mi->init_ca.contains(addr)) {
		return mi->init_ca.effective_address(addr);
	}
	else {
		ostringstream err;
		err << "Failed to find the module the code address "
			<< (void *)(unsigned long)addr
			<< " belongs to.";
		throw ModuleInfo::Error(err.str());
	}
	
	assert(false);
	return 0;
}

/* Find the module and the section within the module the specified 
 * effective address belongs to. The function throws 
 * ModuleInfo::Error if not found. */
static std::pair<rc_ptr<ModuleInfo>, rc_ptr<SectionInfo> >
data_for_effective_address(unsigned int addr_eff)
{
	if (eff_addr_map.empty()) {
		ostringstream err;
		err << "Unable to find the module the effective address "
			<< (void *)(unsigned long)addr_eff
			<< " belongs to: "
			<< "the map {effective address => module} is empty"
			<< ".";
		throw ModuleInfo::Error(err.str());
	}
	
	TAddrMap::const_iterator it = eff_addr_map.upper_bound(addr_eff);
	if (it == eff_addr_map.begin()) {
		ostringstream err;
		err << "Failed to find the module the effective address "
			<< (void *)(unsigned long)addr_eff
			<< " belongs to.";
		throw ModuleInfo::Error(err.str());
	}
	
	--it;
	rc_ptr<ModuleInfo> mi = it->second;
	assert(!mi->sections.empty());
	
	/* The vector of sections must be sorted. */
	assert(adjacent_find(
		mi->sections.begin(), mi->sections.end(), 
		SectionInfo::Greater()) == mi->sections.end());
	
	rc_ptr<SectionInfo> holder(new SectionInfo());
	holder->addr = addr_eff;

	ModuleInfo::TSections::const_iterator sit;
	sit = upper_bound(mi->sections.begin(), mi->sections.end(), holder, 
			  SectionInfo::Less());
	if (sit == mi->sections.begin()) {
		ostringstream err;
		err << "Failed to find the section in \"" 
			<< mi->name << "\" the effective address "
			<< (void *)(unsigned long)addr_eff
			<< " belongs to.";
		throw ModuleInfo::Error(err.str());
	}
	
	--sit;
	rc_ptr<SectionInfo> sec = *sit;
	assert(addr_eff >= sec->addr);
	
	if (addr_eff >= sec->addr + sec->size) {
		ostringstream err;
		err << "Failed to find the section in \"" 
			<< mi->name << "\" the effective address "
			<< (void *)(unsigned long)addr_eff
			<< " belongs to (the address is outside \""
			<< sec->name << "\").";
		throw ModuleInfo::Error(err.str());
	}

	return make_pair(mi, sec);
}

void
ModuleInfo::print_address_plain(unsigned int addr_eff)
{
	std::pair<rc_ptr<ModuleInfo>, rc_ptr<SectionInfo> > r;
	r = data_for_effective_address(addr_eff);
	
	rc_ptr<ModuleInfo> &mi = r.first;
	rc_ptr<SectionInfo> &si = r.second;
	
	cout << mi->name << ":" << si->name << "+0x" << hex 
		<< (addr_eff - si->addr) << dec;
}

static void
print_line_header(unsigned int index)
{
	cout << "    #" << index << "  ";
}

/* Print name of the function an a position in it (file:line), similar to
 * a stack trace entry. */
static void
print_func(const char *name, const char *file, int line, unsigned int index)
{
	/* basename() needs a string that can be changed. */
	char *src = strdup(file);
	if (src == NULL) {
		cerr << "print_func(): not enough memory.\n";
		return;
	}
	
	print_line_header(index);
	cout << name << " (" << basename(src) << ":" << line
		<< ")\n";
	free(src);
}

/* Print information about the inline function corresponding to the given
 * scope in the given compilation unit ('cudie').
 * The location in the inline function is given in (*src_file, *src_line).
 * The function changes these two variables to the location where the
 * given inline is called ("substituted"). 
 *
 * Returns true if obtains all the needed info successfully, false 
 * otherwise. */
static bool
print_inline_info(Dwarf_Die *cudie, Dwarf_Die *die, const char **src_file,
		  int *src_line, unsigned int index)
{
	Dwarf_Files *files;
	const char *name = NULL;
	int ret;

	/* Get the name of the function. */
	name = dwarf_diename(die);
	if (name == NULL) {
		return false;
	}

	print_func(name, *src_file, *src_line, index);
	
	/* Get the list of the source files. */
	ret = dwarf_getsrcfiles(cudie, &files, NULL);
	if (ret != 0) {
		return false;
	}

	Dwarf_Attribute attr_mem;
	Dwarf_Word val;
	Dwarf_Attribute *att;

	/* Find DW_AT_call_file attribute. */
	att = dwarf_attr(die, DW_AT_call_file, &attr_mem);
	if (att == NULL) {
		return false;
	}
	
	/* Find the source file where the function has been inlined. */
	if (dwarf_formudata(att, &val) != 0) {
		return false;
	}

	/* Retrieve the name of the file. */
	*src_file = dwarf_filesrc(files, val, NULL, NULL);
	if (*src_file == NULL) {
		return false;
	}
	
	/* Find DW_AT_call_line attribute. */
	att = dwarf_attr(die, DW_AT_call_line, &attr_mem);
	if (att == NULL) {
		return false;
	}
	
	/* Find the source line where the function has been inlined. */
	if (dwarf_formudata(att, &val) != 0) {
		return false;
	}

	*src_line = val;
	return true;
}

/* Print the whole chain of inlined functions for a given DIE with
 * DW_TAG_inlined_subroutine tag. 
 *
 * Returns true if obtains all the needed info successfully, false 
 * otherwise. */
static bool
print_inline_info_full(Dwarf_Die *cudie, Dwarf_Die *die,
		       const char *src_file, int src_line,
		       unsigned int index)
{
	Dwarf_Die *scopes = NULL;
	int nscopes = dwarf_getscopes_die(die, &scopes);
	bool ret = true;
	const char *name = NULL;

	/* Find the containing scopes for an inline. At least one scope 
	 * (same as the DIE) should be found. */
	if (nscopes <= 0) {
		free(scopes);
		return false;
	}

	for (int i = 0; i < nscopes; ++i) {
		switch (dwarf_tag(&scopes[i])) {
		case DW_TAG_subprogram:
			/* End of the chain of inline functions. */
			name = dwarf_diename(&scopes[i]);
			if (name == NULL) {
				ret = false;
				break;
			}

			print_func(name, src_file, src_line, index);
			break;

		case DW_TAG_inlined_subroutine:
			ret = print_inline_info(
				cudie, &scopes[i], &src_file, &src_line,
				index);
			break;
			
		default:
			break;
		}
		
		if (!ret)
			break;
	}
	
	free(scopes);
	return ret;
}

static bool
print_dwarf_function(Dwfl_Module *mod, Dwarf_Addr addr,
		     const char *src_file, int src_line,
		     unsigned int index)
{
	bool ret = true;
	Dwarf_Addr bias = 0;
	
	/* DIE for the compilation unit. */
	Dwarf_Die *cudie = dwfl_module_addrdie(mod, addr, &bias);
	
	Dwarf_Die *scopes;
	int nscopes = dwarf_getscopes(cudie, addr - bias, &scopes);

	if (nscopes <= 0) {
		return false;
	}
	
	for (int i = 0; i < nscopes; ++i) {
		const char *name = NULL;
				
		switch (dwarf_tag(&scopes[i])) {
		case DW_TAG_subprogram:
			name = dwarf_diename(&scopes[i]);
			if (name == NULL) {
				break;
			}
			print_func(name, src_file, src_line, index);
			break;

		case DW_TAG_inlined_subroutine:
			ret = print_inline_info_full(
				cudie, &scopes[i], src_file, src_line, 
				index);
			break;

		default:
			break;
		}
		if (!ret)
			break;
	}
	
	free(scopes);
	return ret;
}

static bool
print_source_info(Dwfl_Module *mod, Dwarf_Addr addr, unsigned int index)
{
	bool ret = true;

	/* Find the source file and line number. */
	const char *src = NULL;
	int src_line = 0;
	
	Dwfl_Line *line = dwfl_module_getsrc(mod, addr);
	if (line == NULL) {
		return false;
	}
	
	int linecol;
	src = dwfl_lineinfo(line, &addr, &src_line, &linecol, NULL, NULL);

	if (src == NULL)
		return false;

	/* Find the function the address belongs to. DWARF information may
	 * be more detailed than what dwfl_module_addrname() returns, so
	 * try the former. */
	ret = print_dwarf_function(mod, addr, src, src_line, index);
	return ret;
}

void 
ModuleInfo::print_call_stack_item(unsigned int index, unsigned int addr_eff)
{
	std::pair<rc_ptr<ModuleInfo>, rc_ptr<SectionInfo> > r;
	r = data_for_effective_address(addr_eff);
	
	rc_ptr<ModuleInfo> &mi = r.first;
	rc_ptr<SectionInfo> &si = r.second;
	
	unsigned int offset = addr_eff - si->addr;
	
	if (si->dw_addr == 0) {
		/* We also get here when '--sections_only' was set. */
		print_line_header(index);
		cout << mi->name << ":" << si->name << "+0x" << hex 
			<< offset << dec << "\n";
	}
	else {
		Dwarf_Addr addr = (Dwarf_Addr)(offset + si->dw_addr);
		if (!print_source_info(mi->dwfl_mod, addr, index)) {
			/* If failed to output source info, output what 
			 * we can. */
			print_line_header(index);
			cout << mi->name << ":" << si->name << "+0x" << hex 
				<< offset << dec << "\n";
		}
	}
}
/* ====================================================================== */

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
 * be. */
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

DwflWrapper::DwflWrapper()
{
	cb.section_address = dwfl_offline_section_address;
	cb.find_debuginfo = find_debuginfo;
	cb.find_elf = find_elf;
	
	dwfl_handle = dwfl_begin(&cb);
	if (dwfl_handle == NULL) {
		throw runtime_error(string(
			"Failed to initialize DWARF facilities: ") +
			dwfl_errmsg(-1));
	}
}

DwflWrapper::~DwflWrapper()
{
	dwfl_end(dwfl_handle);
}

void
DwflWrapper::init()
{
	if (!dwfl.isNULL()) {
		cerr << "Attempt to initialize already initialized DWARF "
			<< "facilities." << endl;
		assert(false);
		return;
	}
	
	dwfl = rc_ptr<DwflWrapper>(new DwflWrapper());
}
/* ====================================================================== */
