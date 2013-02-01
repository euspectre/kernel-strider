#ifndef MODULE_INFO_H_1802_INCLUDED
#define MODULE_INFO_H_1802_INCLUDED

#include <string>
#include <vector>
#include <stdexcept>
#include <utility>

#include <cassert>

#include <dwarf.h>
#include <elfutils/libdwfl.h>

#include "rc_ptr.h"

/* SectionInfo - information about an ELF section of a kernel module. */
class SectionInfo
{
public:
	/* Name of the section. */
	std::string name;
	
	/* The effective address of the section, i.e. the address the 
	 * section would have if the "init" and "core" areas of the module
	 * were loaded at their effective addresses. */
	unsigned int addr;
	
	/* The size of the section. */
	unsigned int size;
	
	/* Alignment of the section. */
	unsigned int align;
	
	/* The start address of the section from the point of view of DWARF 
	 * info. Used only if the debug info is available and source line 
	 * resolution is requested. 0 if debug info should not or cannot be 
	 * used for this section. */
	unsigned int dw_addr;

	/* true if the section belongs to "init" area of the code, false if
	 * the section belongs to "core" area. */
	bool is_init;

public:
	SectionInfo(const std::string& s_name = std::string(""))
		: name(s_name)
	{
		addr = 0;
		size = 0;
		dw_addr = 0;
		is_init = false;
		align = 1;
	}

public:
	/* A comparator for sorting and searching. */
	struct Less
	{
		bool 
		operator()(const rc_ptr<SectionInfo> &left, 
			   const rc_ptr<SectionInfo> &right) const
		{
			return left->addr < right->addr;
		}
	};
	
	struct Greater
	{
		bool 
		operator()(const rc_ptr<SectionInfo> &left, 
			   const rc_ptr<SectionInfo> &right) const
		{
			return left->addr > right->addr;
		}
	};
};

/* ModuleInfo - information about a loaded kernel module. */
class ModuleInfo
{
public:
	class Error : public std::runtime_error
	{
	public:
		Error(const std::string &message) 
			: std::runtime_error(message)
		{}
	};

public:
	/* Parameters of the "init" and "core" areas of the loaded code: 
	 * - the real start address (lower 32 bits are enough); 
	 * - the effective start address to be presented to TSan; 
	 * - the size of the area. 
	 *
	 * The real address is the one recorded in the trace; if the module
	 * is unloaded and loaded again, this address it may be different.
	 * The effective address is chosen when the first loading of the 
	 * module is encountered in the trace, this address does not change 
	 * if the module is reloaded. The real and the effective addresses
	 * are 0 if the module has not shown up in the trace so far. */
	struct CodeArea
	{
		unsigned int addr_real;
		unsigned int addr_eff;
		unsigned int size;
		
		CodeArea()
			: addr_real(0), addr_eff(0), size(0)
		{}
		
		bool contains(unsigned int addr) const
		{
			return (addr >= addr_real && 
				addr < addr_real + size);
		}
		
		unsigned int effective_address(unsigned int addr) const
		{
			assert(addr_real != 0);
			assert(addr_eff != 0);
			assert(contains(addr));
			return addr - addr_real + addr_eff;
		}
	};
	
public:
	/* Name of the module. */
	std::string name;
	
	/* Path to the binary file of the module (or to the file with debug
	 * info for the module). */
	std::string path;
	
	CodeArea init_ca;
	CodeArea core_ca;
	
	/* The array of pointers to SectionInfo objects, sorted by their
	 * effective addresses to simplify lookup. */
	typedef std::vector<rc_ptr<SectionInfo> > TSections;
	TSections sections;
	
	/* An object to access DWARF info of the kernel module, NULL if not
	 * used. */
	Dwfl_Module *dwfl_mod;
	
	/* true if the module has sections with debug info (DWARF), false 
	 * otherwise. */
	bool has_debug_info;
	
private:
	/* true if the module was loaded when the current event was 
	 * generated, false otherwise. */
	bool loaded;

public:
	ModuleInfo(const std::string &mod_name)
		: name(mod_name), dwfl_mod(NULL), has_debug_info(false),
		  loaded(false)
	{ }

public:
	bool is_loaded()
	{ return loaded; }
	
public:
	/* The main ModuleInfo API. 
	 * The functions throw ModuleInfo::Error on error. */
	
	/* Add information about the module (or a file with debug info for
	 * the module) at 'mod_path' to the system for future use. Prepends 
	 * 'mod_dir' to contruct the full path to the module if 'mod_path' 
	 * is relative. 
	 * 'mod_dir' must end with '/'. */
	static void add_module(
		const std::string &mod_path, 
		const std::string &mod_dir);
	
	/* Handle "target_load" event. 
	 * 'name' - name of the module */
	static void on_module_load(
		const std::string &name, 
		unsigned int init_addr, unsigned int init_size,
		unsigned int core_addr, unsigned int core_size);
			
	/* Handle "target_unload" event. */
	static void on_module_unload(const std::string &name);
	
	/* If the function that has finished is the init function of a
	 * target module, mark the init area of that module as freed, do
	 * nothing otherwise. */
	static void on_function_exit(unsigned int addr);
	
	/* Find the module this code address (a.k.a. program counter, PC)
	 * belongs to and return the effective address for it.
	 * If the appropriate module is not found, the function throws 
	 * ModuleInfo::Error. */
	static unsigned int effective_address(unsigned int addr);
	
	/* Find the module and the section the given effective address 
	 * belongs to and output "<module>:<section>+0x<offset>" to 
	 * stdout. */
	static void print_address_plain(unsigned int addr_eff);
	
	/* Print the call stack item corresponding to the given effective
	 * address to stdout, followed by a newline. 
	 * If it is requested not to use debug info or the debug info is not
	 * available, the name of the module, the name of the section and 
	 * offset in the latter are printed. Otherwise source line info 
	 * is printed (in case of inlines, on more than one line). 
	 *
	 * 'index' - the index of the stack item. Note that if the chains of
	 * inlined calls are reported, they will have the same stack item 
	 * index. */
	static void print_call_stack_item(unsigned int index, 
					  unsigned int addr_eff);
};

/* A wrapper around a handle to libdw/libdwfl that closes the handle on 
 * exit. */
class DwflWrapper
{
	Dwfl *dwfl_handle;
	Dwfl_Callbacks cb;
public:
	DwflWrapper();
	~DwflWrapper();
	
	Dwfl *get_handle() const
	{
		return dwfl_handle;
	}
public:
	/* Initialize DWARF processing facilities. 
	 * Throws std::runtime_error() if the initialization fails. */
	static void init();
};
/* ====================================================================== */

#endif /* MODULE_INFO_H_1802_INCLUDED */
